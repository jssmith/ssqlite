#include <runtime.h>
#include <math.h>
#include <strings.h>
#include <svg.h>

typedef float domain;
typedef int rank;

#define fls(__x) __builtin_clz(__x)

typedef struct tuple { 
    domain v;
    rank g, delta;
    struct tuple *next;
    struct tuple *last;
} *tuple;


typedef struct summary {
    tuple head;
    int n;
    unsigned int epsilon;
} *summary;

#define forall_tuples(_x, _s)\
    for(tuple _x = _s->head->next; _x != s->head; _x = _x=_x->next)
 

static inline rank err(summary s) 
{
    return((((u64)(s->n * 2) * (u64)s->epsilon)) >> 32);
}

static void check_monotoic(summary s)
{
    forall_tuples(t, s) 
        if ((t->v > t->next->v) && (t->next != s->head))
            printf ("ordering error\n");
}

static inline int band(summary s, tuple t)
{
    int p = err(s);
    int z = p - t->delta;
    int f = fls(z);
    if (f != 0) f--;
    int mask = ((1<<f) - 1);
    int guess = (1<<f) + (p&mask);
    return(f + 1 + ((z > guess)?1:0));
}

static void delete(summary s, tuple t)
{
    if (t == s->head) {
        if (s->head == s->head->next) {
            s->head = 0;
            return;
        }
        s->head = s->head->next;
    }
    t->next->last = t->last;
    t->last->next = t->next;
    free(t);
}

void compress(summary s)
{
    int threshold = err(s);
    tuple b = s->head->last;
    while (b != s->head) {
        tuple a = b->last;
        if ((band(s, a) <= band(s, b)) &&
            ((a->g + b->g + b->delta) <= threshold)){
            b->g += a->g;
            delete(s, a);
        } else {
            b = a;
        }
    }
}


void insert(summary s, domain v)
{
    tuple n = malloc(sizeof(struct tuple));
    n->v = v;
    n->g = 1;
    n->delta = 0;
    s->n++;

    if ((s->n % (0xfffffffful/s->epsilon))  == 0)
        compress(s);

    if ((!s->head) || (s->head->v > v)){
        if ((n->next = s->head)){
            (n->last = s->head->last)->next = n;
        } else n->next = n;
        n->next->last = n;
        s->head = n;
    } else {
        tuple i;
        for (i = s->head->next;
             (i->v <= v) && (i != s->head) ;
             i = i->next);

        if (i != s->head) n->delta = err(s) - 1;

        (n->last = i->last)->next = n;
        (n->next = i)->last = n;
    }
}


summary allocate_summary(float epsilon)
{
    summary s = malloc(sizeof(struct summary));
    s->epsilon = (unsigned int)(epsilon * 0xfffffffful);
    s->n = 0;
    s->head = 0;
    return(s);
}

domain quantile(summary s, float p)
{
    // fake ceil
    rank target = (int)(p * s->n) + 1+ err(s);
    tuple last;
    rank min = 0;

    for (tuple a =s->head; a ; a = a->next) {
        if ((min + a->g + a->delta) > target) 
            return last->v;

        last = a;
        min += a->g;
    }
    return(0);
}



void print_summary(buffer b, 
                   summary s,
                   int width,
                   int height)
{
    rank r = 0;
    tuple t = s->head;
    int maxs = 0;
    domain maxv = s->head->last->v;
    domain v0 = 0.0;
    int x0 = 0, y0 = 0;
    int pheight = height - 20; //scale

    do {
        int y = (int)(t->g + t->delta);
        int x = (width*t->v)/maxv;
        y0 += y;
        if (x != x0){
            y0 = y0 / (t->v - v0);
            if (y0 > maxs) maxs = y0;            
            x0 = x; 
            v0 = t->v; 
            y0 = 0;
        }
        t = t->next;
    } while(t != s->head);

    print_horizontal_scale(b, 
                           s->head->v,
                           s->head->last->v,
                           width, height,
                           height, 12);

    svg_polygon_start(b);    
    svg_poly_point(b, 0, pheight);
    
    v0 = 0.0;
    x0 = 0;
    do {
        int y = (int)(t->g + t->delta);
        int x = (width*t->v)/maxv;
        y0 += y;
        if (x != x0){
            svg_poly_point(b, x, pheight - (int)((pheight * y0) / (maxs * (t->v - v0))));
            x0 = x; 
            v0 = t->v; 
            y0 = 0;
        }
        t = t->next;
    } while(t != s->head);

    svg_poly_point(b, width, pheight);
    svg_polygon_end(b);
}
