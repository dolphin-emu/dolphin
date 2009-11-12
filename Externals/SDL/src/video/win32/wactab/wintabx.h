/* *INDENT-OFF* */
/* ------------------------------- wintabx.h -------------------------------- */
/* Combined 16 & 32-bit version. */


/*------------------------------------------------------------------------------
The text and information contained in this file may be freely used,
copied, or distributed without compensation or licensing restrictions.

This file is copyright 1991-1998 by LCS/Telegraphics.
------------------------------------------------------------------------------*/

#ifndef _INC_WINTABX     /* prevent multiple includes */
#define _INC_WINTABX

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

/* prototype for wintabx.lib. */
#define _UnlinkWinTab _UnlinkWintab	
#ifdef WIN32
	extern WINAPI _UnlinkWintab(void);
#else
	extern FAR _UnlinkWintab(void);
#endif

/****** Wintab message crackers ***********************************************/

#ifndef HANDLE_MSG
#define HANDLE_MSG(hwnd, message, fn)    \
    case (message): return HANDLE_##message((hwnd), (wParam), (lParam), (fn))
#endif

/* void Cls_OnWintabPacket(HWND hwnd, HCTX hCtx, UINT sn) */
#define HANDLE_WT_PACKET(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HCTX)(lParam), (UINT)(wParam)), 0L)
#define FORWARD__WT_PACKET(hwnd, bs, hCtx, sn, fn) \
    (void)(fn)((hwnd), _WT_PACKET(bs), (WPARAM)(UINT)(sn), (LPARAM)(HCTX)(hCtx))
#define FORWARD_WT_PACKET(hwnd, hCtx, sn, fn) \
	FORWARD__WT_PACKET(hwnd, WT_DEFBASE, hCtx, sn, fn)

/* void Cls_OnWintabCtxOpen(HWND hwnd, HCTX hCtx, UINT sf) */
#define HANDLE_WT_CTXOPEN(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HCTX)(wParam), (UINT)(lParam)), 0L)
#define FORWARD__WT_CTXOPEN(hwnd, bs, hCtx, sf, fn) \
    (void)(fn)((hwnd), _WT_CTXOPEN(bs), (WPARAM)(HCTX)(hCtx), (LPARAM)(UINT)(sf))
#define FORWARD_WT_CTXOPEN(hwnd, hCtx, sf, fn) \
	FORWARD__WT_CTXOPEN(hwnd, WT_DEFBASE, hCtx, sf, fn)

/* void Cls_OnWintabCtxClose(HWND hwnd, HCTX hCtx, UINT sf) */
#define HANDLE_WT_CTXCLOSE(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HCTX)(wParam), (UINT)(lParam)), 0L)
#define FORWARD__WT_CTXCLOSE(hwnd, bs, hCtx, sf, fn) \
    (void)(fn)((hwnd), _WT_CTXCLOSE(bs), (WPARAM)(HCTX)(hCtx), (LPARAM)(UINT)(sf))
#define FORWARD_WT_CTXCLOSE(hwnd, hCtx, sf, fn) \
	FORWARD__WT_CTXCLOSE(hwnd, WT_DEFBASE, hCtx, sf, fn)

/* void Cls_OnWintabCtxUpdate(HWND hwnd, HCTX hCtx, UINT sf) */
#define HANDLE_WT_CTXUPDATE(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HCTX)(wParam), (UINT)(lParam)), 0L)
#define FORWARD__WT_CTXUPDATE(hwnd, bs, hCtx, sf, fn) \
    (void)(fn)((hwnd), _WT_CTXUPDATE(bs), (WPARAM)(HCTX)(hCtx), (LPARAM)(UINT)(sf))
#define FORWARD_WT_CTXUPDATE(hwnd, hCtx, sf, fn) \
	FORWARD__WT_CTXUPDATE(hwnd, WT_DEFBASE, hCtx, sf, fn)

/* void Cls_OnWintabCtxOverlap(HWND hwnd, HCTX hCtx, UINT sf) */
#define HANDLE_WT_CTXOVERLAP(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HCTX)(wParam), (UINT)(lParam)), 0L)
#define FORWARD__WT_CTXOVERLAP(hwnd, bs, hCtx, sf, fn) \
    (void)(fn)((hwnd), _WT_CTXOVERLAP(bs), (WPARAM)(HCTX)(hCtx), (LPARAM)(UINT)(sf))
#define FORWARD_WT_CTXOVERLAP(hwnd, hCtx, sf, fn) \
	FORWARD__WT_CTXOVERLAP(hwnd, WT_DEFBASE, hCtx, sf, fn)

/* void Cls_OnWintabProximity(HWND hwnd, HCTX hCtx, BOOL cp, BOOL hp) */
#define HANDLE_WT_PROXIMITY(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HCTX)(wParam), (BOOL)LOWORD(lParam),  (BOOL)HIWORD(lParam)), 0L)
#define FORWARD__WT_PROXIMITY(hwnd, bs, hCtx, cp, hp, fn) \
    (void)(fn)((hwnd), _WT_PROXIMITY(bs), (WPARAM)(HCTX)(hCtx), MAKELPARAM((cp), (hp))
#define FORWARD_WT_PROXIMITY(hwnd, hCtx, sf, fn) \
	FORWARD__WT_PROXIMITY(hwnd, WT_DEFBASE, hCtx, cp, hp, fn)

/* void Cls_OnWintabInfoChange(HWND hwnd, HMGR hMgr, UINT c, UINT i) */
#define HANDLE_WT_INFOCHANGE(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HMGR)(wParam), (UINT)LOWORD(lParam),  (UINT)HIWORD(lParam)), 0L)
#define FORWARD__WT_INFOCHANGE(hwnd, bs, hMgr, cp, hp, fn) \
    (void)(fn)((hwnd), _WT_INFOCHANGE(bs), (WPARAM)(HMGR)(hMgr), MAKELPARAM((c), (i))
#define FORWARD_WT_INFOCHANGE(hwnd, hMgr, sf, fn) \
	FORWARD__WT_INFOCHANGE(hwnd, WT_DEFBASE, hMgr, cp, hp, fn)

/****** Alternate porting layer macros ****************************************/

#define GET_WT_PACKET_HCTX(wp, lp)			((HCTX)lp)
#define GET_WT_PACKET_SERIAL(wp, lp)		(wp)
#define GET_WT_PACKET_MPS(h, s)				\
		(s), (LPARAM)(h)

#define GET_WT_CTXOPEN_HCTX(wp, lp)			((HCTX)wp)
#define GET_WT_CTXOPEN_STATUS(wp, lp)		((UINT)lp)
#define GET_WT_CTXOPEN_MPS(h, s)			\
		(WPARAM)(h), (LPARAM)(s)

#define GET_WT_CTXCLOSE_HCTX(wp, lp)		((HCTX)wp)
#define GET_WT_CTXCLOSE_STATUS(wp, lp)		((UINT)lp)
#define GET_WT_CTXCLOSE_MPS(h, s)			\
		(WPARAM)(h), (LPARAM)(s)

#define GET_WT_CTXUPDATE_HCTX(wp, lp)		((HCTX)wp)
#define GET_WT_CTXUPDATE_STATUS(wp, lp)		((UINT)lp)
#define GET_WT_CTXUPDATE_MPS(h, s)			\
		(WPARAM)(h), (LPARAM)(s)

#define GET_WT_CTXOVERLAP_HCTX(wp, lp)		((HCTX)wp)
#define GET_WT_CTXOVERLAP_STATUS(wp, lp)	((UINT)lp)
#define GET_WT_CTXOVERLAP_MPS(h, s)			\
		(WPARAM)(h), (LPARAM)(s)

#define GET_WT_PROXIMITY_HCTX(wp, lp)		((HCTX)wp)
#define GET_WT_PROXIMITY_CTXPROX(wp, lp)	LOWORD(lp)
#define GET_WT_PROXIMITY_HARDPROX(wp, lp)	HIWORD(lp)
#define GET_WT_PROXIMITY_MPS(h, fc, fh)		\
		(WPARAM)(h), MAKELONG(fc, fh)

#define GET_WT_INFOCHANGE_HMGR(wp, lp)		((HMGR)wp)
#define GET_WT_INFOCHANGE_CATEGORY(wp, lp)	LOWORD(lp)
#define GET_WT_INFOCHANGE_INDEX(wp, lp)		HIWORD(lp)
#define GET_WT_INFOCHANGE_MPS(h, c, i)		\
		(WPARAM)(h), MAKELONG(c, i)


#ifdef __cplusplus
}
#endif	/* __cplusplus */

#endif /* #define _INC_WINTABX */

