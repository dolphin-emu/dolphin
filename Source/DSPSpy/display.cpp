#include <stdlib.h>
#include <string.h>
#include <reent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "color.h"

#define FONT_XSIZE			8
#define FONT_YSIZE			16
#define FONT_XFACTOR		1
#define FONT_YFACTOR		1
#define FONT_XGAP			2
#define FONT_YGAP			0

typedef struct _display_data_s {
	u8 *framebuffer;
	u8 *font;
	int xres,yres,stride;
	int cursor_x,cursor_y;
	int border_left,border_right,border_top,border_bottom;
	int scrolled_lines;

	unsigned int foreground,background;
} display_data_s;

int displ_open(struct _reent *r, const char *path, int flags,int mode);
int displ_write(struct _reent *r, int fd, const char *ptr, int len);

static struct _display_data_s dsp_displ;
static struct _display_data_s *displ = &dsp_displ;
extern u8 display_font_8x16[];


static void __display_drawc(int xpos, int ypos, int c)
{
	xpos >>= 1;
	int ax, ay;
	unsigned int *ptr = (unsigned int*)(displ->framebuffer + displ->stride * ypos + xpos * 4);
	for (ay = 0; ay < FONT_YSIZE; ay++)
#if FONT_XFACTOR == 2
		for (ax = 0; ax < 8; ax++)
		{
			unsigned int color;
			if ((displ->font[c * FONT_YSIZE + ay] << ax) & 0x80)
				color = displ->foreground;
			else
				color = displ->background;
#if FONT_YFACTOR == 2
				// pixel doubling: we write u32
			ptr[ay * 2 * displ->stride/4 + ax] = color;
				// line doubling
			ptr[(ay * 2 +1) * displ->stride/4 + ax] = color;
#else
			ptr[ay * displ->stride/4 + ax] = color;
#endif
		}
#else
		for (ax = 0; ax < 4; ax ++)
		{
			unsigned int color[2];
			int bits = (displ->font[c * FONT_YSIZE + ay] << (ax*2));
			if (bits & 0x80)
				color[0] = displ->foreground;
			else
				color[0] = displ->background;
			if (bits & 0x40)
				color[1] = displ->foreground;
			else
				color[1] = displ->background;
			ptr[ay * displ->stride/4 + ax] = (color[0] & 0xFFFF00FF) | (color[1] & 0x0000FF00);
		}
#endif
}

void ds_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride)
{
	unsigned int level;

	_CPU_ISR_Disable(level);
	
	displ->framebuffer = (u8 *)framebuffer;
	displ->xres = xres;
	displ->yres = yres;
	displ->border_left = xstart;
	displ->border_top  = ystart;
	displ->border_right = displ->xres;
	displ->border_bottom = displ->yres;
	displ->stride = stride;
	displ->cursor_x = xstart;
	displ->cursor_y = ystart;
	
	displ->font = display_font_8x16;
	
	displ->foreground = COLOR_WHITE;
	displ->background = COLOR_BLACK;
	
	displ->scrolled_lines = 0;
	
	unsigned int c = (displ->xres*displ->yres)/2;
	unsigned int *p = (unsigned int*)displ->framebuffer;
	while(c--)
		*p++ = displ->background;

	_CPU_ISR_Restore(level);
}

void ds_clear(void)
{
	unsigned int c = (displ->xres*displ->yres)/2;
	unsigned int *p = (unsigned int*)displ->framebuffer;
	c /= 2;
	p += c;
	while(c--)
		*p++ = displ->background;
}

int display_putc(int c)
{
	if(!displ) return -1;

	switch(c) 
	{
		case '\n':
			displ->cursor_y += FONT_YSIZE*FONT_YFACTOR+FONT_YGAP;
			displ->cursor_x = displ->border_left;
			break;
		default:
			__display_drawc(displ->cursor_x,displ->cursor_y,c);
			displ->cursor_x += FONT_XSIZE*FONT_XFACTOR+FONT_XGAP;
			if((displ->cursor_x+FONT_XSIZE*FONT_XFACTOR)>displ->border_right) {
				displ->cursor_y += FONT_YSIZE*FONT_YFACTOR+FONT_YGAP;
				displ->cursor_x = displ->border_left;
			}
	}

	if((displ->cursor_y+FONT_YSIZE*FONT_YFACTOR)>=displ->border_bottom) {
		memcpy(displ->framebuffer,
			displ->framebuffer+displ->stride*(FONT_YSIZE*FONT_YFACTOR+FONT_YGAP),
			displ->stride*displ->yres-FONT_YSIZE);

		unsigned int cnt = (displ->stride * (FONT_YSIZE * FONT_YFACTOR + FONT_YGAP))/4;
		unsigned int *ptr = (unsigned int*)(displ->framebuffer + displ->stride * (displ->yres - FONT_YSIZE));
		while(cnt--)
			*ptr++ = displ->background;
		displ->cursor_y -= FONT_YSIZE * FONT_YFACTOR + FONT_YGAP;
	}
	
	return 1;
}

int displ_write(struct _reent *r,int fd, const char *ptr,int len)
{
	int i, count = 0;
	char *tmp = (char*)ptr;

	if(!tmp || len<=0) return -1;

	i = 0;
	while(*tmp!='\0' && i<len) {
		count += display_putc(*tmp++);
		i++;
	}

	return count;
}

void ds_text_out(int xpos, int ypos, const char *str)
{
	ypos *= (FONT_YSIZE * FONT_YFACTOR + FONT_YGAP);
	xpos *= (FONT_XSIZE * FONT_XFACTOR + FONT_XGAP);
	while(*str != '\0')
	{
		__display_drawc(xpos, ypos, *str++);
		xpos += (FONT_XSIZE * FONT_XFACTOR + FONT_XGAP);
	};
}

void ds_printf(int x, int y, const char *fmt, ...)
{
	char tmpbuf[255];
	va_list marker;
	va_start(marker,fmt);
	vsprintf(tmpbuf, fmt, marker );
	va_end(marker);
	ds_text_out(x, y, tmpbuf);

}

void ds_set_colour(int f, int b)
{
	displ->background = b;
	displ->foreground = f;
}
void ds_underline(int xpos, int ypos, int len, int col)
{
	int i;
	ypos = (ypos + 1) * (FONT_YSIZE * FONT_YFACTOR + FONT_YGAP) - 1;
	xpos *= (FONT_XSIZE * FONT_XFACTOR + FONT_XGAP)/2;
	len *= (FONT_XSIZE * FONT_XFACTOR + FONT_XGAP)/2;
	unsigned int *ptr = (unsigned int*)(displ->framebuffer + displ->stride * ypos + xpos * 4);
	for(i=0 ; i < len ; i++)
		ptr[i] = col;
}

