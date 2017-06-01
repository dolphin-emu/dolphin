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
#include <SFML/Window/Context.hpp>
#include <SFML/Window/GlContext.hpp>
#include <SFML/System/ThreadLocalPtr.hpp>
#include <SFML/OpenGL.hpp>
#include <algorithm>
#include <vector>
#include <string>

#if defined(SFML_SYSTEM_WINDOWS)

    typedef const GLubyte* (APIENTRY *glGetStringiFuncType)(GLenum, GLuint);

#else

    typedef const GLubyte* (*glGetStringiFuncType)(GLenum, GLuint);

#endif

#if !defined(GL_NUM_EXTENSIONS)
    #define GL_NUM_EXTENSIONS 0x821D
#endif


namespace
{
    // This per-thread variable holds the current context for each thread
    sf::ThreadLocalPtr<sf::Context> currentContext(NULL);
}

namespace sf
{
////////////////////////////////////////////////////////////
Context::Context()
{
    m_context = priv::GlContext::create();
    setActive(true);
}


////////////////////////////////////////////////////////////
Context::~Context()
{
    setActive(false);
    delete m_context;
}


////////////////////////////////////////////////////////////
bool Context::setActive(bool active)
{
    bool result = m_context->setActive(active);

    if (result)
        currentContext = (active ? this : NULL);

    return result;
}


////////////////////////////////////////////////////////////
const ContextSettings& Context::getSettings() const
{
    return m_context->getSettings();
}


////////////////////////////////////////////////////////////
const Context* Context::getActiveContext()
{
    return currentContext;
}


////////////////////////////////////////////////////////////
GlFunctionPointer Context::getFunction(const char* name)
{
    return priv::GlContext::getFunction(name);
}


////////////////////////////////////////////////////////////
bool Context::isExtensionAvailable(const char* name)
{
    static std::vector<std::string> extensions;
    static bool loaded = false;

    if (!loaded)
    {
        const Context* context = getActiveContext();

        if (!context)
            return false;

        const char* extensionString = NULL;

        if(context->getSettings().majorVersion < 3)
        {
            // Try to load the < 3.0 way
            extensionString = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

            do
            {
                const char* extension = extensionString;

                while(*extensionString && (*extensionString != ' '))
                    extensionString++;

                extensions.push_back(std::string(extension, extensionString));
            }
            while (*extensionString++);
        }
        else
        {
            // Try to load the >= 3.0 way
            glGetStringiFuncType glGetStringiFunc = NULL;
            glGetStringiFunc = reinterpret_cast<glGetStringiFuncType>(getFunction("glGetStringi"));

            if (glGetStringiFunc)
            {
                int numExtensions = 0;
                glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);

                if (numExtensions)
                {
                    for (unsigned int i = 0; i < static_cast<unsigned int>(numExtensions); ++i)
                    {
                        extensionString = reinterpret_cast<const char*>(glGetStringiFunc(GL_EXTENSIONS, i));

                        extensions.push_back(extensionString);
                    }
                }
            }
        }

        loaded = true;
    }

    return std::find(extensions.begin(), extensions.end(), name) != extensions.end();
}


////////////////////////////////////////////////////////////
Context::Context(const ContextSettings& settings, unsigned int width, unsigned int height)
{
    m_context = priv::GlContext::create(settings, width, height);
    setActive(true);
}

} // namespace sf
