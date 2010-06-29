
#ifndef __DYNAMIC_LIBRARY_H
#define __DYNAMIC_LIBRARY_H

int loadLib(const char *filename);
int unloadLib();
void *getFunction(const char *funcname);

#endif
