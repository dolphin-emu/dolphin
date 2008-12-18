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
 public:
    /*    int screen;
	  int x, y;
	  unsigned int depth;*/    
    
    virtual void SwapBuffers() = 0;
    virtual void SetWindowText(const char *text) = 0;
    virtual bool PeekMessages() = 0;
    virtual void Update() = 0;
    virtual bool MakeCurrent() = 0;
    virtual void SetSize(u32 newWidth, u32 newHeight) {
	width = newWidth; 
	height = newHeight; 
    };
    
    u32 GetWidth() {return width;}
    u32 GetHeight() {return height;}
    
    //   bool GLwindow(SVideoInitialize &_VideoInitialize, int _iwidth, int _iheight) {};
    // setResolution
    // resolution iter
};

#endif
