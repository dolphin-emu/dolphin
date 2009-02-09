#ifndef WINAMP_GEN_H
#define WINAMP_GEN_H



#define GPPHDR_VER 0x10



typedef struct {
	int version;
	char *description;
	int (*init)();
	void (*config)();
	void (*quit)();
	HWND hwndParent;
	HINSTANCE hDllInstance;
} winampGeneralPurposePlugin;

typedef winampGeneralPurposePlugin * ( * winampGeneralPurposePluginGetter )();



// extern winampGeneralPurposePlugin * gen_plugins[ 256 ];



#endif // WINAMP_GEN_H
