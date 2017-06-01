////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2013 Jonathan De Wachter (dewachter.jonathan@gmail.com)
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
#include <SFML/Window/EGLCheck.hpp>
#include <SFML/System/Err.hpp>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
void eglCheckError(const char* file, unsigned int line)
{
    // Obtain information about the success or failure of the most recent EGL
    // function called in the current thread
    EGLint errorCode = eglGetError();

    if (errorCode != EGL_SUCCESS)
    {
        std::string fileString(file);
        std::string error = "unknown error";
        std::string description  = "no description";

        // Decode the error code returned
        switch (errorCode)
        {
            case EGL_NOT_INITIALIZED:
            {
                error = "EGL_NOT_INITIALIZED";
                description = "EGL is not initialized, or could not be initialized, for the specified display";
                break;
            }

            case EGL_BAD_ACCESS:
            {
                error = "EGL_BAD_ACCESS";
                description = "EGL cannot access a requested resource (for example, a context is bound in another thread)";
                break;
            }

            case EGL_BAD_ALLOC:
            {
                error = "EGL_BAD_ALLOC";
                description = "EGL failed to allocate resources for the requested operation";
                break;
            }
            case EGL_BAD_ATTRIBUTE:
            {
                error = "EGL_BAD_ATTRIBUTE";
                description = "an unrecognized attribute or attribute value was passed in an attribute list";
                break;
            }

            case EGL_BAD_CONTEXT:
            {
                error = "EGL_BAD_CONTEXT";
                description = "an EGLContext argument does not name a valid EGLContext";
                break;
            }

            case EGL_BAD_CONFIG:
            {
                error = "EGL_BAD_CONFIG";
                description = "an EGLConfig argument does not name a valid EGLConfig";
                break;
            }

            case EGL_BAD_CURRENT_SURFACE:
            {
                error = "EGL_BAD_CURRENT_SURFACE";
                description = "the current surface of the calling thread is a window, pbuffer, or pixmap that is no longer valid";
                break;
            }

            case EGL_BAD_DISPLAY:
            {
                error = "EGL_BAD_DISPLAY";
                description = "an EGLDisplay argument does not name a valid EGLDisplay; or, EGL is not initialized on the specified EGLDisplay";
                break;
            }


            case EGL_BAD_SURFACE:
            {
                error = "EGL_BAD_SURFACE";
                description = "an EGLSurface argument does not name a valid surface (window, pbuffer, or pixmap) configured for rendering";
                break;
            }

            case EGL_BAD_MATCH:
            {
                error = "EGL_BAD_MATCH";
                description = "arguments are inconsistent; for example, an otherwise valid context requires buffers (e.g. depth or stencil) not allocated by an otherwise valid surface";
                break;
            }

            case EGL_BAD_PARAMETER:
            {
                error = "EGL_BAD_PARAMETER";
                description = "one or more argument values are invalid";
                break;
            }

            case EGL_BAD_NATIVE_PIXMAP:
            {
                error = "EGL_BAD_NATIVE_PIXMAP";
                description = "an EGLNativePixmapType argument does not refer to a valid native pixmap";
                break;
            }

            case EGL_BAD_NATIVE_WINDOW:
            {
                error = "EGL_BAD_NATIVE_WINDOW";
                description = "an EGLNativeWindowType argument does not refer to a valid native window";
                break;
            }

            case EGL_CONTEXT_LOST:
            {
                error = "EGL_CONTEXT_LOST";
                description = "a power management event has occurred. The application must destroy all contexts and reinitialize client API state and objects to continue rendering";
                break;
            }
        }

        // Log the error
        err() << "An internal EGL call failed in "
              << fileString.substr(fileString.find_last_of("\\/") + 1) << " (" << line << ") : "
              << error << ", " << description
              << std::endl;
    }
}

} // namespace priv
} // namespace sf
