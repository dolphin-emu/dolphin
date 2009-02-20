#ifndef _PLUGINVIDEO_H
#define _PLUGINVIDEO_H

#include "pluginspecs_video.h"
#include "Plugin.h"

namespace Common {
    typedef void (__cdecl* TVideo_Prepare)();
    typedef void (__cdecl* TVideo_SendFifoData)(u8*,u32);
    typedef void (__cdecl* TVideo_UpdateXFB)(u8*, u32, u32, s32, bool);
    typedef bool (__cdecl* TVideo_Screenshot)(const char* filename);
    typedef void (__cdecl* TVideo_EnterLoop)();
    typedef void (__cdecl* TVideo_ExitLoop)();
    typedef void (__cdecl* TVideo_AddMessage)(const char* pstr, unsigned int milliseconds);

    class PluginVideo : public CPlugin
	{
    public:
		PluginVideo(const char *_Filename);
		~PluginVideo();
		virtual bool IsValid() {return validVideo;};

		TVideo_Prepare      Video_Prepare;
		TVideo_SendFifoData Video_SendFifoData;
		TVideo_EnterLoop    Video_EnterLoop;
		TVideo_ExitLoop     Video_ExitLoop;
		TVideo_UpdateXFB    Video_UpdateXFB;

		TVideo_AddMessage   Video_AddMessage;
		TVideo_Screenshot   Video_Screenshot;

    private:
		bool validVideo;
    };
}

#endif
