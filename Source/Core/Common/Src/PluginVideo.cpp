#include "PluginVideo.h"

namespace Common
{
    PluginVideo::PluginVideo(const char *_Filename) : CPlugin(_Filename), validVideo(false)
	{
  
	Video_Prepare = 0;
	Video_SendFifoData = 0;
	Video_UpdateXFB = 0;
	Video_EnterLoop = 0;
	Video_Screenshot = 0;
	Video_AddMessage = 0;
	Video_Stop = 0;

	Video_Prepare  = reinterpret_cast<TVideo_Prepare>      
	    (LoadSymbol("Video_Prepare"));
	Video_SendFifoData = reinterpret_cast<TVideo_SendFifoData>
	    (LoadSymbol("Video_SendFifoData"));
	Video_UpdateXFB = reinterpret_cast<TVideo_UpdateXFB>    
	    (LoadSymbol("Video_UpdateXFB"));
	Video_Screenshot = reinterpret_cast<TVideo_Screenshot>   
	    (LoadSymbol("Video_Screenshot"));
	Video_EnterLoop = reinterpret_cast<TVideo_EnterLoop>    
	    (LoadSymbol("Video_EnterLoop"));
	Video_AddMessage = reinterpret_cast<TVideo_AddMessage>   
	    (LoadSymbol("Video_AddMessage"));
        Video_Stop = reinterpret_cast<TVideo_Stop>    
	    (LoadSymbol("Video_Stop"));

	if ((Video_Prepare != 0) &&
	    (Video_SendFifoData != 0) &&
	    (Video_UpdateXFB != 0) &&
	    (Video_EnterLoop != 0) &&
	    (Video_Screenshot != 0) &&
	    (Video_AddMessage != 0) &&
            (Video_Stop != 0))
	    validVideo = true;
    }

    PluginVideo::~PluginVideo() {}
}
