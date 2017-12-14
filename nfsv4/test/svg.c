#include <runtime.h>
#include <svg.h>
#include <math.h>


void print_vertical_scale(buffer b,
                          double min,
                          double max,
                          int width,
                          int height,
                          int offset,
                          int textsize)
{
    double quanta = max-min/10;
    double s = pow(10.0, ceil(log10(quanta)) -1);
    double i;
    
    for (i = floor(min/s) * s; i<max; i+=s/10){
        char label[64];
        sprintf(label, "%.3g", i);
        int x = width*(i-min)/(max-min);
        svg_text(b, x,                 
                 offset,
                 textsize,
                 label);
        svg_line(b, x, 0, x, offset-15); 
    }
}

void print_horizontal_scale(buffer b, 
                            double min, 
                            double max,
                            int width,
                            int height,
                            int offset,
                            int textsize)
{
    double quanta = max-min/10;
    double s = pow(10.0, ceil(log10(quanta)) -1);
    double i;
    
    for (i = floor(min/s) * s; i<max; i+=s/10){
        char label[64];
        sprintf(label, "%.3g", i);
        int x = width*(i-min)/(max-min);
        svg_text(b, x,                 
                 offset,
                 textsize,
                 label);
        svg_line(b, x, 0, x, offset-15); 
    }
}

static void prop(buffer b, char * name, int value)
{
    buffer_append(b, name);
    buffer_append(b, "=\"");
    print_u64(b, value);
    buffer_append(b, "\" ");
}

void svg_header(buffer b, int width, int height)
{
    buffer_append(b,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
    buffer_append(b, "<svg xmlns=\"http://www.w3.org/2000/svg\" ");
    prop(b, "width", width);
    prop(b, "height", height);
    buffer_append(b, ">\n");
}

void svg_trailer(buffer b)
{
    buffer_append(b, "</svg>\n");
}

void svg_rectangle(buffer b, int x, int y, int width, int height)
{
    buffer_append(b, "<rect style=\"fill:#ffffff;fill-rule:evenodd;stroke:#000000;stroke-width:1px;\" ");
    prop(b, "width", width);
    prop(b, "height", height);
    prop(b, "x", x);
    prop(b, "y", y);
    buffer_append(b, "/>\n");
}

void svg_line(buffer b, int x1, int y1, int x2, int y2)
{
    buffer_append(b, "<line style=\"fill:none;stroke:#000000;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1\" ");
    
    prop(b, "x1", x1);
    prop(b, "y1", y1);
    prop(b, "x2", x2);
    prop(b, "y2", y2);
    buffer_append(b, "/>\n");
}

void svg_polygon_start(buffer b)
{
    buffer_append(b, "<polygon style=\"fill:#c0c0c0;stroke:#000000;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1\" points=\"");
}

void svg_poly_point(buffer b, int x, int y)
{
    print_u64(b, x);
    buffer_append(b, ",");
    print_u64(b, y);
    buffer_append(b, " ");
}

void svg_polygon_end(buffer b)
{
    buffer_append(b, "\"/>");
}

void svg_text(buffer b, int x, int y, int size, char *text)
{
    buffer_append(b, "<text style=\"font-size:");
    print_u64(b, size);
    buffer_append(b, "px;text-anchor:left;font-family:Sans;\" ");
    prop (b, "x", x);
    prop (b, "y", y);
    buffer_append(b, ">");
    buffer_append(b, text);
    buffer_append(b, "</text>\n");
}

void group_transform(buffer b, int x, int y)
{
    buffer_append(b, "<g transform=\"translate(");
    print_u64(b, x);
    buffer_append(b, ",");
    print_u64(b, y);
    buffer_append(b, ")\">\n");
}

void end_group_transform(buffer b)
{
    buffer_append(b, "</g>\n");
}

