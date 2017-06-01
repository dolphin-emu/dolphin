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
#include <SFML/Window/WindowImpl.hpp> // included first to avoid a warning about macro redefinition
#include <SFML/OpenGL.hpp> // included second to avoid an error in WglExtensions.hpp
#include <SFML/Window/Win32/WglContext.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Err.hpp>
#include <sstream>
#include <vector>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
void ensureExtensionsInit(HDC deviceContext)
{
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;

        // We don't check the return value since the extension
        // flags are cleared even if loading fails
        sfwgl_LoadFunctions(deviceContext);
    }
}


////////////////////////////////////////////////////////////
String getErrorString(DWORD errorCode)
{
    std::basic_ostringstream<TCHAR, std::char_traits<TCHAR> > ss;
    TCHAR errBuff[256];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorCode, 0, errBuff, sizeof(errBuff), NULL);
    ss << errBuff;
    String errMsg(ss.str());

    return errMsg;
}


////////////////////////////////////////////////////////////
WglContext::WglContext(WglContext* shared) :
m_window       (NULL),
m_pbuffer      (NULL),
m_deviceContext(NULL),
m_context      (NULL),
m_ownsWindow   (false)
{
    // Save the creation settings
    m_settings = ContextSettings();

    // Make sure that extensions are initialized if this is not the shared context
    // The shared context is the context used to initialize the extensions
    if (shared && shared->m_deviceContext)
        ensureExtensionsInit(shared->m_deviceContext);

    // Create the rendering surface (window or pbuffer if supported)
    createSurface(shared, 1, 1, VideoMode::getDesktopMode().bitsPerPixel);

    // Create the context
    if (m_deviceContext)
        createContext(shared);
}


////////////////////////////////////////////////////////////
WglContext::WglContext(WglContext* shared, const ContextSettings& settings, const WindowImpl* owner, unsigned int bitsPerPixel) :
m_window       (NULL),
m_pbuffer      (NULL),
m_deviceContext(NULL),
m_context      (NULL),
m_ownsWindow   (false)
{
    // Save the creation settings
    m_settings = settings;

    // Make sure that extensions are initialized if this is not the shared context
    // The shared context is the context used to initialize the extensions
    if (shared && shared->m_deviceContext)
        ensureExtensionsInit(shared->m_deviceContext);

    // Create the rendering surface from the owner window
    createSurface(owner->getSystemHandle(), bitsPerPixel);

    // Create the context
    if (m_deviceContext)
        createContext(shared);
}


////////////////////////////////////////////////////////////
WglContext::WglContext(WglContext* shared, const ContextSettings& settings, unsigned int width, unsigned int height) :
m_window       (NULL),
m_pbuffer      (NULL),
m_deviceContext(NULL),
m_context      (NULL),
m_ownsWindow   (false)
{
    // Save the creation settings
    m_settings = settings;

    // Make sure that extensions are initialized if this is not the shared context
    // The shared context is the context used to initialize the extensions
    if (shared && shared->m_deviceContext)
        ensureExtensionsInit(shared->m_deviceContext);

    // Create the rendering surface (window or pbuffer if supported)
    createSurface(shared, width, height, VideoMode::getDesktopMode().bitsPerPixel);

    // Create the context
    if (m_deviceContext)
        createContext(shared);
}


////////////////////////////////////////////////////////////
WglContext::~WglContext()
{
    // Destroy the OpenGL context
    if (m_context)
    {
        if (wglGetCurrentContext() == m_context)
            wglMakeCurrent(NULL, NULL);
        wglDeleteContext(m_context);
    }

    // Destroy the device context
    if (m_deviceContext)
    {
        if (m_pbuffer)
        {
            wglReleasePbufferDCARB(m_pbuffer, m_deviceContext);
            wglDestroyPbufferARB(m_pbuffer);
        }
        else
        {
            ReleaseDC(m_window, m_deviceContext);
        }
    }

    // Destroy the window if we own it
    if (m_window && m_ownsWindow)
        DestroyWindow(m_window);
}


////////////////////////////////////////////////////////////
GlFunctionPointer WglContext::getFunction(const char* name)
{
    GlFunctionPointer address = reinterpret_cast<GlFunctionPointer>(wglGetProcAddress(reinterpret_cast<LPCSTR>(name)));

    if (address)
    {
        // Test whether the returned value is a valid error code
        ptrdiff_t errorCode = reinterpret_cast<ptrdiff_t>(address);

        if ((errorCode != -1) && (errorCode != 1) && (errorCode != 2) && (errorCode != 3))
            return address;
    }

    static HMODULE module = NULL;

    if (!module)
        module = GetModuleHandleA("OpenGL32.dll");

    if (module)
        return reinterpret_cast<GlFunctionPointer>(GetProcAddress(module, reinterpret_cast<LPCSTR>(name)));

    return 0;
}


////////////////////////////////////////////////////////////
bool WglContext::makeCurrent()
{
    return m_deviceContext && m_context && wglMakeCurrent(m_deviceContext, m_context);
}


////////////////////////////////////////////////////////////
void WglContext::display()
{
    if (m_deviceContext && m_context)
        SwapBuffers(m_deviceContext);
}


////////////////////////////////////////////////////////////
void WglContext::setVerticalSyncEnabled(bool enabled)
{
    // Make sure that extensions are initialized
    ensureExtensionsInit(m_deviceContext);

    if (sfwgl_ext_EXT_swap_control == sfwgl_LOAD_SUCCEEDED)
    {
        if (wglSwapIntervalEXT(enabled ? 1 : 0) == FALSE)
            err() << "Setting vertical sync failed" << std::endl;
    }
    else
    {
        static bool warned = false;

        if (!warned)
        {
            // wglSwapIntervalEXT not supported
            err() << "Setting vertical sync not supported" << std::endl;

            warned = true;
        }
    }
}


////////////////////////////////////////////////////////////
int WglContext::selectBestPixelFormat(HDC deviceContext, unsigned int bitsPerPixel, const ContextSettings& settings, bool pbuffer)
{
    // Let's find a suitable pixel format -- first try with wglChoosePixelFormatARB
    int bestFormat = 0;
    if (sfwgl_ext_ARB_pixel_format == sfwgl_LOAD_SUCCEEDED)
    {
        // Define the basic attributes we want for our window
        int intAttributes[] =
        {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            0,                      0
        };

        // Let's check how many formats are supporting our requirements
        int   formats[512];
        UINT  nbFormats;
        bool  isValid = wglChoosePixelFormatARB(deviceContext, intAttributes, NULL, 512, formats, &nbFormats) != 0;

        // Get the best format among the returned ones
        if (isValid && (nbFormats > 0))
        {
            int bestScore = 0x7FFFFFFF;
            for (UINT i = 0; i < nbFormats; ++i)
            {
                // Extract the components of the current format
                int values[7];
                const int attributes[] =
                {
                    WGL_RED_BITS_ARB,
                    WGL_GREEN_BITS_ARB,
                    WGL_BLUE_BITS_ARB,
                    WGL_ALPHA_BITS_ARB,
                    WGL_DEPTH_BITS_ARB,
                    WGL_STENCIL_BITS_ARB,
                    WGL_ACCELERATION_ARB
                };

                if (!wglGetPixelFormatAttribivARB(deviceContext, formats[i], PFD_MAIN_PLANE, 7, attributes, values))
                {
                    err() << "Failed to retrieve pixel format information: " << getErrorString(GetLastError()).toAnsiString() << std::endl;
                    break;
                }

                int sampleValues[2] = {0, 0};
                if (sfwgl_ext_ARB_multisample == sfwgl_LOAD_SUCCEEDED)
                {
                    const int sampleAttributes[] =
                    {
                        WGL_SAMPLE_BUFFERS_ARB,
                        WGL_SAMPLES_ARB
                    };

                    if (!wglGetPixelFormatAttribivARB(deviceContext, formats[i], PFD_MAIN_PLANE, 2, sampleAttributes, sampleValues))
                    {
                        err() << "Failed to retrieve pixel format multisampling information: " << getErrorString(GetLastError()).toAnsiString() << std::endl;
                        break;
                    }
                }

                int sRgbCapableValue = 0;
                if ((sfwgl_ext_ARB_framebuffer_sRGB == sfwgl_LOAD_SUCCEEDED) || (sfwgl_ext_EXT_framebuffer_sRGB == sfwgl_LOAD_SUCCEEDED))
                {
                    const int sRgbCapableAttribute = WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;

                    if (!wglGetPixelFormatAttribivARB(deviceContext, formats[i], PFD_MAIN_PLANE, 1, &sRgbCapableAttribute, &sRgbCapableValue))
                    {
                        err() << "Failed to retrieve pixel format sRGB capability information: " << getErrorString(GetLastError()).toAnsiString() << std::endl;
                        break;
                    }
                }

                if (pbuffer)
                {
                    const int pbufferAttributes[] =
                    {
                        WGL_DRAW_TO_PBUFFER_ARB
                    };

                    int pbufferValue = 0;

                    if (!wglGetPixelFormatAttribivARB(deviceContext, formats[i], PFD_MAIN_PLANE, 1, pbufferAttributes, &pbufferValue))
                    {
                        err() << "Failed to retrieve pixel format pbuffer information: " << getErrorString(GetLastError()).toAnsiString() << std::endl;
                        break;
                    }

                    if (pbufferValue != GL_TRUE)
                        continue;
                }

                // Evaluate the current configuration
                int color = values[0] + values[1] + values[2] + values[3];
                int score = evaluateFormat(bitsPerPixel, settings, color, values[4], values[5], sampleValues[0] ? sampleValues[1] : 0, values[6] == WGL_FULL_ACCELERATION_ARB, sRgbCapableValue == TRUE);

                // Keep it if it's better than the current best
                if (score < bestScore)
                {
                    bestScore  = score;
                    bestFormat = formats[i];
                }
            }
        }
    }

    // ChoosePixelFormat doesn't support pbuffers
    if (pbuffer)
        return bestFormat;

    // Find a pixel format with ChoosePixelFormat, if wglChoosePixelFormatARB is not supported
    if (bestFormat == 0)
    {
        // Setup a pixel format descriptor from the rendering settings
        PIXELFORMATDESCRIPTOR descriptor;
        ZeroMemory(&descriptor, sizeof(descriptor));
        descriptor.nSize        = sizeof(descriptor);
        descriptor.nVersion     = 1;
        descriptor.iLayerType   = PFD_MAIN_PLANE;
        descriptor.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        descriptor.iPixelType   = PFD_TYPE_RGBA;
        descriptor.cColorBits   = static_cast<BYTE>(bitsPerPixel);
        descriptor.cDepthBits   = static_cast<BYTE>(settings.depthBits);
        descriptor.cStencilBits = static_cast<BYTE>(settings.stencilBits);
        descriptor.cAlphaBits   = bitsPerPixel == 32 ? 8 : 0;

        // Get the pixel format that best matches our requirements
        bestFormat = ChoosePixelFormat(deviceContext, &descriptor);
    }

    return bestFormat;
}


////////////////////////////////////////////////////////////
void WglContext::setDevicePixelFormat(unsigned int bitsPerPixel)
{
    int bestFormat = selectBestPixelFormat(m_deviceContext, bitsPerPixel, m_settings);

    if (bestFormat == 0)
    {
        err() << "Failed to find a suitable pixel format for device context: " << getErrorString(GetLastError()).toAnsiString() << std::endl
              << "Cannot create OpenGL context" << std::endl;
        return;
    }

    // Extract the depth and stencil bits from the chosen format
    PIXELFORMATDESCRIPTOR actualFormat;
    actualFormat.nSize    = sizeof(actualFormat);
    actualFormat.nVersion = 1;
    DescribePixelFormat(m_deviceContext, bestFormat, sizeof(actualFormat), &actualFormat);

    // Set the chosen pixel format
    if (!SetPixelFormat(m_deviceContext, bestFormat, &actualFormat))
    {
        err() << "Failed to set pixel format for device context: " << getErrorString(GetLastError()).toAnsiString() << std::endl
              << "Cannot create OpenGL context" << std::endl;
        return;
    }
}


////////////////////////////////////////////////////////////
void WglContext::updateSettingsFromPixelFormat()
{
    int format = GetPixelFormat(m_deviceContext);

    PIXELFORMATDESCRIPTOR actualFormat;
    actualFormat.nSize    = sizeof(actualFormat);
    actualFormat.nVersion = 1;
    DescribePixelFormat(m_deviceContext, format, sizeof(actualFormat), &actualFormat);

    if (format == 0)
    {
        err() << "Failed to get selected pixel format" << std::endl;
        return;
    }

    if (sfwgl_ext_ARB_pixel_format == sfwgl_LOAD_SUCCEEDED)
    {
        const int attributes[] = {WGL_DEPTH_BITS_ARB, WGL_STENCIL_BITS_ARB};
        int values[2];

        if (wglGetPixelFormatAttribivARB(m_deviceContext, format, PFD_MAIN_PLANE, 2, attributes, values))
        {
            m_settings.depthBits   = values[0];
            m_settings.stencilBits = values[1];
        }
        else
        {
            err() << "Failed to retrieve pixel format information: " << getErrorString(GetLastError()).toAnsiString() << std::endl;
            m_settings.depthBits   = actualFormat.cDepthBits;
            m_settings.stencilBits = actualFormat.cStencilBits;
        }

        if (sfwgl_ext_ARB_multisample == sfwgl_LOAD_SUCCEEDED)
        {
            const int sampleAttributes[] = {WGL_SAMPLE_BUFFERS_ARB, WGL_SAMPLES_ARB};
            int sampleValues[2];

            if (wglGetPixelFormatAttribivARB(m_deviceContext, format, PFD_MAIN_PLANE, 2, sampleAttributes, sampleValues))
            {
                m_settings.antialiasingLevel = sampleValues[0] ? sampleValues[1] : 0;
            }
            else
            {
                err() << "Failed to retrieve pixel format multisampling information: " << getErrorString(GetLastError()).toAnsiString() << std::endl;
                m_settings.antialiasingLevel = 0;
            }
        }
        else
        {
            m_settings.antialiasingLevel = 0;
        }

        if ((sfwgl_ext_ARB_framebuffer_sRGB == sfwgl_LOAD_SUCCEEDED) || (sfwgl_ext_EXT_framebuffer_sRGB == sfwgl_LOAD_SUCCEEDED))
        {
            const int sRgbCapableAttribute = WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB;
            int sRgbCapableValue = 0;

            if (wglGetPixelFormatAttribivARB(m_deviceContext, format, PFD_MAIN_PLANE, 1, &sRgbCapableAttribute, &sRgbCapableValue))
            {
                m_settings.sRgbCapable = (sRgbCapableValue == TRUE);
            }
            else
            {
                err() << "Failed to retrieve pixel format sRGB capability information: " << getErrorString(GetLastError()).toAnsiString() << std::endl;
                m_settings.sRgbCapable = false;
            }
        }
        else
        {
            m_settings.sRgbCapable = false;
        }
    }
    else
    {
        m_settings.depthBits   = actualFormat.cDepthBits;
        m_settings.stencilBits = actualFormat.cStencilBits;
        m_settings.antialiasingLevel = 0;
    }
}


////////////////////////////////////////////////////////////
void WglContext::createSurface(WglContext* shared, unsigned int width, unsigned int height, unsigned int bitsPerPixel)
{
    // Check if the shared context already exists and pbuffers are supported
    if (shared && shared->m_deviceContext && (sfwgl_ext_ARB_pbuffer == sfwgl_LOAD_SUCCEEDED))
    {
        int bestFormat = selectBestPixelFormat(shared->m_deviceContext, bitsPerPixel, m_settings, true);

        if (bestFormat > 0)
        {
            int attributes[] = {0, 0};

            m_pbuffer = wglCreatePbufferARB(shared->m_deviceContext, bestFormat, width, height, attributes);

            if (m_pbuffer)
            {
                m_window = shared->m_window;
                m_deviceContext = wglGetPbufferDCARB(m_pbuffer);

                if (!m_deviceContext)
                {
                    wglDestroyPbufferARB(m_pbuffer);
                    m_pbuffer = NULL;
                }
            }
        }
    }

    // If pbuffers are not available we use a hidden window as the off-screen surface to draw to
    if (!m_deviceContext)
    {
        // We can't create a memory DC, the resulting context wouldn't be compatible
        // with other contexts and thus wglShareLists would always fail

        // Create the hidden window
        m_window = CreateWindowA("STATIC", "", WS_POPUP | WS_DISABLED, 0, 0, width, height, NULL, NULL, GetModuleHandle(NULL), NULL);
        ShowWindow(m_window, SW_HIDE);
        m_deviceContext = GetDC(m_window);

        m_ownsWindow = true;

        // Set the pixel format of the device context
        setDevicePixelFormat(bitsPerPixel);
    }

    // Update context settings from the selected pixel format
    updateSettingsFromPixelFormat();
}


////////////////////////////////////////////////////////////
void WglContext::createSurface(HWND window, unsigned int bitsPerPixel)
{
    m_window = window;
    m_deviceContext = GetDC(window);

    // Set the pixel format of the device context
    setDevicePixelFormat(bitsPerPixel);

    // Update context settings from the selected pixel format
    updateSettingsFromPixelFormat();
}


////////////////////////////////////////////////////////////
void WglContext::createContext(WglContext* shared)
{
    // Get a working copy of the context settings
    ContextSettings settings = m_settings;

    // Get the context to share display lists with
    HGLRC sharedContext = shared ? shared->m_context : NULL;

    // Create the OpenGL context -- first try using wglCreateContextAttribsARB
    while (!m_context && m_settings.majorVersion)
    {
        if (sfwgl_ext_ARB_create_context == sfwgl_LOAD_SUCCEEDED)
        {
            std::vector<int> attributes;

            // Check if the user requested a specific context version (anything > 1.1)
            if ((m_settings.majorVersion > 1) || ((m_settings.majorVersion == 1) && (m_settings.minorVersion > 1)))
            {
                attributes.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
                attributes.push_back(m_settings.majorVersion);
                attributes.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
                attributes.push_back(m_settings.minorVersion);
            }

            // Check if setting the profile is supported
            if (sfwgl_ext_ARB_create_context_profile == sfwgl_LOAD_SUCCEEDED)
            {
                int profile = (m_settings.attributeFlags & ContextSettings::Core) ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB : WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
                int debug = (m_settings.attributeFlags & ContextSettings::Debug) ? WGL_CONTEXT_DEBUG_BIT_ARB : 0;

                attributes.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
                attributes.push_back(profile);
                attributes.push_back(WGL_CONTEXT_FLAGS_ARB);
                attributes.push_back(debug);
            }
            else
            {
                if ((m_settings.attributeFlags & ContextSettings::Core) || (m_settings.attributeFlags & ContextSettings::Debug))
                    err() << "Selecting a profile during context creation is not supported,"
                          << "disabling comptibility and debug" << std::endl;

                m_settings.attributeFlags = ContextSettings::Default;
            }

            // Append the terminating 0
            attributes.push_back(0);
            attributes.push_back(0);

            // Create the context
            m_context = wglCreateContextAttribsARB(m_deviceContext, sharedContext, &attributes[0]);
        }
        else
        {
            // If wglCreateContextAttribsARB is not supported, there is no need to keep trying
            break;
        }

        // If we couldn't create the context, first try disabling flags,
        // then lower the version number and try again -- stop at 0.0
        // Invalid version numbers will be generated by this algorithm (like 3.9), but we really don't care
        if (!m_context)
        {
            if (m_settings.attributeFlags != ContextSettings::Default)
            {
                m_settings.attributeFlags = ContextSettings::Default;
            }
            else if (m_settings.minorVersion > 0)
            {
                // If the minor version is not 0, we decrease it and try again
                m_settings.minorVersion--;

                m_settings.attributeFlags = settings.attributeFlags;
            }
            else
            {
                // If the minor version is 0, we decrease the major version
                m_settings.majorVersion--;
                m_settings.minorVersion = 9;

                m_settings.attributeFlags = settings.attributeFlags;
            }
        }
    }

    // If wglCreateContextAttribsARB failed, use wglCreateContext
    if (!m_context)
    {
        // set the context version to 1.1 (arbitrary) and disable flags
        m_settings.majorVersion = 1;
        m_settings.minorVersion = 1;
        m_settings.attributeFlags = ContextSettings::Default;

        m_context = wglCreateContext(m_deviceContext);
        if (!m_context)
        {
            err() << "Failed to create an OpenGL context for this window: " << getErrorString(GetLastError()).toAnsiString() << std::endl;
            return;
        }

        // Share this context with others
        if (sharedContext)
        {
            // wglShareLists doesn't seem to be thread-safe
            static Mutex mutex;
            Lock lock(mutex);

            if (!wglShareLists(sharedContext, m_context))
                err() << "Failed to share the OpenGL context: " << getErrorString(GetLastError()).toAnsiString() << std::endl;
        }
    }
}

} // namespace priv

} // namespace sf
