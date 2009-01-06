#ifndef _GLWINDOW_H
#define _GLWINDOW_H

#include "Common.h"
#include "EventHandler.h"
#include "Globals.h"
#include "Config.h"
#include "pluginspecs_video.h"

#include <GL/glew.h>

#if defined(__APPLE__) 
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
enum OGL_Props {
    OGL_FULLSCREEN,
    OGL_STRETCHTOFIT,
    OGL_KEEPRATIO,
    OGL_PROP_COUNT
};

class GLWindow {
 private:
    u32 width, height;
    int yOffset, xOffset;
    float xMax, yMax;
    bool properties[OGL_PROP_COUNT];
public:
 
    virtual void SwapBuffers() {};
    virtual void SetWindowText(const char *text) {};
    virtual bool PeekMessages() {return false;};
    virtual void Update() {};;
    virtual bool MakeCurrent() {return false;};

    bool getProperty(OGL_Props prop) {return properties[prop];}
    virtual bool setProperty(OGL_Props prop, bool value) 
    {return properties[prop] = value;}
    u32 GetWidth() {return width;}
    u32 GetHeight() {return height;}
    void SetSize(u32 newWidth, u32 newHeight) {
	width = newWidth; 
	height = newHeight; 
    }

    int GetYoff() {return yOffset;}
    int GetXoff() {return xOffset;}
    void SetOffset(int x, int y) {
	yOffset = y;
	xOffset = x;
    }

    void SetMax(float x, float y) {
	yMax = y;
	xMax = x;
    }
    float GetXmax() {return xMax;}
    float GetYmax() {return yMax;}
 
    static bool valid() { return false; }
    //   bool GLwindow(SVideoInitialize &_VideoInitialize, int _iwidth, int _iheight) {};
    // setResolution
    // resolution iter
};

#endif
