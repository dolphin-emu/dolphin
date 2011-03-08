#ifdef _WIN32
#include <windows.h>
HINSTANCE library = NULL;
#else
#include <dlfcn.h>
#include <stdio.h>
void *library = NULL;
#endif
#include <string.h>

int loadLib(const char *filename) {
	if (library)
		return -1;

	if (!filename || strlen(filename) == 0) 
		return -2;

#ifdef _WIN32
	library = LoadLibraryA(filename);
#else
	library = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
#endif

	if (!library)
		return -3;

	return 0;
}

int unloadLib()
{
    int retval;
    if (!library)
		return -1;

#ifdef _WIN32
    retval = FreeLibrary(library);
#else
    retval = dlclose(library);
#endif

    library = NULL;
    return retval;
}

void *getFunction(const char *funcname) {
	void* retval;

	if (!library) {
		return NULL;
	}

#ifdef _WIN32
	retval = GetProcAddress(library, funcname);
#else
	retval = dlsym(library, funcname);
#endif

	if (!retval) 
		return NULL;

	return retval;
}
