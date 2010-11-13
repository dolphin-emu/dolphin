#include "Common.h"
#include "wiiuse_internal.h"

int wiiuse_find(struct wiimote_t **wm, int max_wiimotes, int timeout)
{
	return 0;
}

int wiiuse_connect(struct wiimote_t **wm, int wiimotes)
{
	return 0;
}

void wiiuse_disconnect(struct wiimote_t *wm)
{
	return;
}

int wiiuse_io_read(struct wiimote_t *wm)
{
	return 0;
}

int wiiuse_io_write(struct wiimote_t *wm, byte *buf, int len)
{
	return 0;
}
