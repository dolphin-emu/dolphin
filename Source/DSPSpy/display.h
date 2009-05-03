
#ifndef _DISPLAY_H
#define _DISPLAY_H

void ds_text_out(int xpos, int ypos, const char *str);
void ds_set_colour(int f, int b);
void ds_init(void *framebuffer, int xstart, int ystart, int xres, int yres, int stride);
void ds_underline(int xpos, int ypos, int len, int col);
void ds_printf(int x, int y, const char *fmt, ...);
void ds_clear(void);

#endif
