#ifndef __CLRUN_H
#define __CLRUN_H

/*#ifdef _WIN32
#ifdef CLRUN_COMPILE
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED __declspec(dllimport)
#endif
#else*/
#define EXPORTED
//#endif

#ifdef __cplusplus
extern "C" {
#endif
	
extern EXPORTED int clrInit();
extern EXPORTED int clrHasOpenCL();
	
#ifdef __cplusplus
};
#endif

#endif
