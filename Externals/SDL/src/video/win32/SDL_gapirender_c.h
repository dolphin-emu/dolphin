/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
    
    Stefan Klug
    klug.stefan@gmx.de
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* hi res definitions */
typedef struct _RawFrameBufferInfo
{
    WORD wFormat;
    WORD wBPP;
    VOID *pFramePointer;
    int cxStride;
    int cyStride;
    int cxPixels;
    int cyPixels;
} RawFrameBufferInfo;

#define GETRAWFRAMEBUFFER   0x00020001

#define FORMAT_565 1
#define FORMAT_555 2
#define FORMAT_OTHER 3


/* From gx.h, since it's not really C compliant */

struct GXDisplayProperties
{
    DWORD cxWidth;
    DWORD cyHeight;             // notice lack of 'th' in the word height.
    long cbxPitch;              // number of bytes to move right one x pixel - can be negative.
    long cbyPitch;              // number of bytes to move down one y pixel - can be negative.
    long cBPP;                  // # of bits in each pixel
    DWORD ffFormat;             // format flags.
};

struct GXKeyList
{
    short vkUp;                 // key for up
    POINT ptUp;                 // x,y position of key/button.  Not on screen but in screen coordinates.
    short vkDown;
    POINT ptDown;
    short vkLeft;
    POINT ptLeft;
    short vkRight;
    POINT ptRight;
    short vkA;
    POINT ptA;
    short vkB;
    POINT ptB;
    short vkC;
    POINT ptC;
    short vkStart;
    POINT ptStart;
};

typedef int (*PFNGXOpenDisplay) (HWND hWnd, DWORD dwFlags);
typedef int (*PFNGXCloseDisplay) ();
typedef void *(*PFNGXBeginDraw) ();
typedef int (*PFNGXEndDraw) ();
typedef int (*PFNGXOpenInput) ();
typedef int (*PFNGXCloseInput) ();
typedef struct GXDisplayProperties (*PFNGXGetDisplayProperties) ();
typedef struct GXKeyList (*PFNGXGetDefaultKeys) (int iOptions);
typedef int (*PFNGXSuspend) ();
typedef int (*PFNGXResume) ();
typedef int (*PFNGXSetViewport) (DWORD dwTop, DWORD dwHeight,
                                 DWORD dwReserved1, DWORD dwReserved2);
typedef BOOL(*PFNGXIsDisplayDRAMBuffer) ();

struct GapiFunc
{
    PFNGXOpenDisplay GXOpenDisplay;
    PFNGXCloseDisplay GXCloseDisplay;
    PFNGXBeginDraw GXBeginDraw;
    PFNGXEndDraw GXEndDraw;
    PFNGXOpenInput GXOpenInput;
    PFNGXCloseInput GXCloseInput;
    PFNGXGetDisplayProperties GXGetDisplayProperties;
    PFNGXGetDefaultKeys GXGetDefaultKeys;
    PFNGXSuspend GXSuspend;
    PFNGXResume GXResume;
    PFNGXSetViewport GXSetViewport;
    PFNGXIsDisplayDRAMBuffer GXIsDisplayDRAMBuffer;
} gx;

#define kfLandscape      0x8    // Screen is rotated 270 degrees
#define kfPalette        0x10   // Pixel values are indexes into a palette
#define kfDirect         0x20   // Pixel values contain actual level information
#define kfDirect555      0x40   // 5 bits each for red, green and blue values in a pixel.
#define kfDirect565      0x80   // 5 red bits, 6 green bits and 5 blue bits per pixel
#define kfDirect888      0x100  // 8 bits each for red, green and blue values in a pixel.
#define kfDirect444      0x200  // 4 red, 4 green, 4 blue
#define kfDirectInverted 0x400

#define GX_FULLSCREEN    0x01   // for OpenDisplay()
#define GX_NORMALKEYS    0x02
#define GX_LANDSCAPEKEYS        0x03
