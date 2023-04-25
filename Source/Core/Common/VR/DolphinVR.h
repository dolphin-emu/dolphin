#pragma once

// controller mapping
void SetVRCallbacks(void (*callback)(int controller_index, int control, double state));

// VR app flow integration
bool IsVREnabled();
#ifdef ANDROID
void InitVROnAndroid(void* vm, void* activity, const char* vendor, int version, const char* name);
#endif
void EnterVR(bool firstStart);
void GetVRResolutionPerEye(int* width, int* height);

// VR rendering integration
void BindVRFramebuffer();
bool StartVRRender();
void FinishVRRender();
void PreVRFrameRender(int fboIndex);
void PostVRFrameRender();
int GetVRFBOIndex();
