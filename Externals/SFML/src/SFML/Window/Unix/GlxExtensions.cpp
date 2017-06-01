////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Unix/GlxExtensions.hpp>
#include <SFML/Window/Context.hpp>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <string>

static sf::GlFunctionPointer IntGetProcAddress(const char* name)
{
    return sf::Context::getFunction(name);
}

int sfglx_ext_EXT_swap_control = sfglx_LOAD_FAILED;
int sfglx_ext_MESA_swap_control = sfglx_LOAD_FAILED;
int sfglx_ext_SGI_swap_control = sfglx_LOAD_FAILED;
int sfglx_ext_EXT_framebuffer_sRGB = sfglx_LOAD_FAILED;
int sfglx_ext_ARB_framebuffer_sRGB = sfglx_LOAD_FAILED;
int sfglx_ext_ARB_multisample = sfglx_LOAD_FAILED;
int sfglx_ext_SGIX_pbuffer = sfglx_LOAD_FAILED;
int sfglx_ext_ARB_create_context = sfglx_LOAD_FAILED;
int sfglx_ext_ARB_create_context_profile = sfglx_LOAD_FAILED;

void (CODEGEN_FUNCPTR *sf_ptrc_glXSwapIntervalEXT)(Display*, GLXDrawable, int) = NULL;

static int Load_EXT_swap_control(void)
{
    int numFailed = 0;
    sf_ptrc_glXSwapIntervalEXT = reinterpret_cast<void (CODEGEN_FUNCPTR*)(Display*, GLXDrawable, int)>(IntGetProcAddress("glXSwapIntervalEXT"));
    if (!sf_ptrc_glXSwapIntervalEXT)
        numFailed++;
    return numFailed;
}

int (CODEGEN_FUNCPTR *sf_ptrc_glXSwapIntervalMESA)(int) = NULL;

static int Load_MESA_swap_control(void)
{
    int numFailed = 0;
    sf_ptrc_glXSwapIntervalMESA = reinterpret_cast<int (CODEGEN_FUNCPTR*)(int)>(IntGetProcAddress("glXSwapIntervalMESA"));
    if (!sf_ptrc_glXSwapIntervalMESA)
        numFailed++;
    return numFailed;
}

int (CODEGEN_FUNCPTR *sf_ptrc_glXSwapIntervalSGI)(int) = NULL;

static int Load_SGI_swap_control(void)
{
    int numFailed = 0;
    sf_ptrc_glXSwapIntervalSGI = reinterpret_cast<int (CODEGEN_FUNCPTR *)(int)>(IntGetProcAddress("glXSwapIntervalSGI"));
    if (!sf_ptrc_glXSwapIntervalSGI)
        numFailed++;
    return numFailed;
}

GLXPbufferSGIX (CODEGEN_FUNCPTR *sf_ptrc_glXCreateGLXPbufferSGIX)(Display*, GLXFBConfigSGIX, unsigned int, unsigned int, int*) = NULL;
void (CODEGEN_FUNCPTR *sf_ptrc_glXDestroyGLXPbufferSGIX)(Display*, GLXPbufferSGIX) = NULL;
void (CODEGEN_FUNCPTR *sf_ptrc_glXGetSelectedEventSGIX)(Display*, GLXDrawable, unsigned long*) = NULL;
int (CODEGEN_FUNCPTR *sf_ptrc_glXQueryGLXPbufferSGIX)(Display*, GLXPbufferSGIX, int, unsigned int*) = NULL;
void (CODEGEN_FUNCPTR *sf_ptrc_glXSelectEventSGIX)(Display*, GLXDrawable, unsigned long) = NULL;

static int Load_SGIX_pbuffer(void)
{
    int numFailed = 0;
    sf_ptrc_glXCreateGLXPbufferSGIX = reinterpret_cast<GLXPbufferSGIX (CODEGEN_FUNCPTR*)(Display*, GLXFBConfigSGIX, unsigned int, unsigned int, int*)>(IntGetProcAddress("glXCreateGLXPbufferSGIX"));
    if (!sf_ptrc_glXCreateGLXPbufferSGIX)
        numFailed++;
    sf_ptrc_glXDestroyGLXPbufferSGIX = reinterpret_cast<void (CODEGEN_FUNCPTR*)(Display*, GLXPbufferSGIX)>(IntGetProcAddress("glXDestroyGLXPbufferSGIX"));
    if (!sf_ptrc_glXDestroyGLXPbufferSGIX)
        numFailed++;
    sf_ptrc_glXGetSelectedEventSGIX = reinterpret_cast<void (CODEGEN_FUNCPTR*)(Display*, GLXDrawable, unsigned long*)>(IntGetProcAddress("glXGetSelectedEventSGIX"));
    if (!sf_ptrc_glXGetSelectedEventSGIX)
        numFailed++;
    sf_ptrc_glXQueryGLXPbufferSGIX = reinterpret_cast<int (CODEGEN_FUNCPTR*)(Display*, GLXPbufferSGIX, int, unsigned int*)>(IntGetProcAddress("glXQueryGLXPbufferSGIX"));
    if (!sf_ptrc_glXQueryGLXPbufferSGIX)
        numFailed++;
    sf_ptrc_glXSelectEventSGIX = reinterpret_cast<void (CODEGEN_FUNCPTR*)(Display*, GLXDrawable, unsigned long)>(IntGetProcAddress("glXSelectEventSGIX"));
    if (!sf_ptrc_glXSelectEventSGIX)
        numFailed++;
    return numFailed;
}

GLXContext (CODEGEN_FUNCPTR *sf_ptrc_glXCreateContextAttribsARB)(Display*, GLXFBConfig, GLXContext, Bool, const int*) = NULL;

static int Load_ARB_create_context(void)
{
    int numFailed = 0;
    sf_ptrc_glXCreateContextAttribsARB = reinterpret_cast<GLXContext (CODEGEN_FUNCPTR*)(Display*, GLXFBConfig, GLXContext, Bool, const int*)>(IntGetProcAddress("glXCreateContextAttribsARB"));
    if (!sf_ptrc_glXCreateContextAttribsARB)
        numFailed++;
    return numFailed;
}

typedef int (*PFN_LOADFUNCPOINTERS)(void);
typedef struct sfglx_StrToExtMap_s
{
    const char* extensionName;
    int* extensionVariable;
    PFN_LOADFUNCPOINTERS LoadExtension;
} sfglx_StrToExtMap;

static sfglx_StrToExtMap ExtensionMap[9] = {
    {"GLX_EXT_swap_control", &sfglx_ext_EXT_swap_control, Load_EXT_swap_control},
    {"GLX_MESA_swap_control", &sfglx_ext_MESA_swap_control, Load_MESA_swap_control},
    {"GLX_SGI_swap_control", &sfglx_ext_SGI_swap_control, Load_SGI_swap_control},
    {"GLX_EXT_framebuffer_sRGB", &sfglx_ext_EXT_framebuffer_sRGB, NULL},
    {"GLX_ARB_framebuffer_sRGB", &sfglx_ext_ARB_framebuffer_sRGB, NULL},
    {"GLX_ARB_multisample", &sfglx_ext_ARB_multisample, NULL},
    {"GLX_SGIX_pbuffer", &sfglx_ext_SGIX_pbuffer, Load_SGIX_pbuffer},
    {"GLX_ARB_create_context", &sfglx_ext_ARB_create_context, Load_ARB_create_context},
    {"GLX_ARB_create_context_profile", &sfglx_ext_ARB_create_context_profile, NULL}
};

static int g_extensionMapSize = 9;


static sfglx_StrToExtMap* FindExtEntry(const char* extensionName)
{
    sfglx_StrToExtMap* currLoc = ExtensionMap;
    for (int loop = 0; loop < g_extensionMapSize; ++loop, ++currLoc)
    {
        if (std::strcmp(extensionName, currLoc->extensionName) == 0)
            return currLoc;
    }

    return NULL;
}


static void ClearExtensionVars(void)
{
    sfglx_ext_EXT_swap_control = sfglx_LOAD_FAILED;
    sfglx_ext_MESA_swap_control = sfglx_LOAD_FAILED;
    sfglx_ext_SGI_swap_control = sfglx_LOAD_FAILED;
    sfglx_ext_EXT_framebuffer_sRGB = sfglx_LOAD_FAILED;
    sfglx_ext_ARB_framebuffer_sRGB = sfglx_LOAD_FAILED;
    sfglx_ext_ARB_multisample = sfglx_LOAD_FAILED;
    sfglx_ext_SGIX_pbuffer = sfglx_LOAD_FAILED;
    sfglx_ext_ARB_create_context = sfglx_LOAD_FAILED;
    sfglx_ext_ARB_create_context_profile = sfglx_LOAD_FAILED;
}


static void LoadExtByName(const char* extensionName)
{
    sfglx_StrToExtMap* entry = NULL;
    entry = FindExtEntry(extensionName);
    if (entry)
    {
        if (entry->LoadExtension)
        {
            int numFailed = entry->LoadExtension();
            if (numFailed == 0)
            {
                *(entry->extensionVariable) = sfglx_LOAD_SUCCEEDED;
            }
            else
            {
                *(entry->extensionVariable) = sfglx_LOAD_SUCCEEDED + numFailed;
            }
        }
        else
        {
            *(entry->extensionVariable) = sfglx_LOAD_SUCCEEDED;
        }
    }
}


static void ProcExtsFromExtString(const char* strExtList)
{
    do
    {
        const char* begin = strExtList;

        while ((*strExtList != ' ') && *strExtList)
            strExtList++;

        LoadExtByName(std::string(begin, strExtList).c_str());
    } while (*strExtList++);
}


int sfglx_LoadFunctions(Display* display, int screen)
{
    ClearExtensionVars();


    ProcExtsFromExtString(reinterpret_cast<const char*>(glXQueryExtensionsString(display, screen)));
    return sfglx_LOAD_SUCCEEDED;
}
