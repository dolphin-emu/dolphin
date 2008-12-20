#ifndef _GLWINDOW_H
#define _GLWINDOW_H

#include "Common.h"
#include "Globals.h"
#include "Config.h"
#include "pluginspecs_video.h"

#include <GL/glew.h>

#if defined(__APPLE__) 
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

class GLWindow {
 private:
    u32 width, height;
    int yOffset, xOffset;
 public:
    /*    int screen;
	  int x, y;
	  unsigned int depth;*/    
    
    virtual void SwapBuffers() {};
    virtual void SetWindowText(const char *text) {};
    virtual bool PeekMessages() {return false;};
    virtual void Update() {};;
    virtual bool MakeCurrent() {return false;};
    virtual void SetSize(u32 newWidth, u32 newHeight) {
	width = newWidth; 
	height = newHeight; 
    }

    void SetOffset(int x, int y) {
	yOffset = y;
	xOffset = x;
    }

    u32 GetWidth() {return width;}
    u32 GetHeight() {return height;}
    int GetYoff() {return yOffset;}
    int GetXoff() {return xOffset;}

    virtual bool valid() { return false; }
    //   bool GLwindow(SVideoInitialize &_VideoInitialize, int _iwidth, int _iheight) {};
    // setResolution
    // resolution iter
};

#endif
