static inline void buffer_append(buffer b, char *x)
{
    int len = strlen(x);
    push_bytes(b, x, len);
}




void svg_header(buffer b, int width, int height);
void svg_trailer(buffer b);
void svg_rectangle(buffer b, int x, int y, int width, int height);
void svg_poly_point(buffer b, int x, int y);
void svg_polygon_start(buffer b);
void svg_polygon_end(buffer b);
void svg_text(buffer b, int x, int y, int size, char *text);
void group_transform(buffer b, int x, int y);
void end_group_transform(buffer b);
void svg_line(buffer b, int x1, int y1, int x2, int y2);

void print_vertical_scale(buffer b,
                          double min,
                          double max,
                          int width,
                          int height,
                          int offset,
                          int textsize);
void print_horizontal_scale(buffer b,
                            double min,
                            double max,
                            int width,
                            int height,
                            int offset,
                            int textsize);
