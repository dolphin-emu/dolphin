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
#include <SFML/Audio/ALCheck.hpp>
#include <SFML/System/Err.hpp>
#include <string>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
void alCheckError(const char* file, unsigned int line, const char* expression)
{
    // Get the last error
    ALenum errorCode = alGetError();

    if (errorCode != AL_NO_ERROR)
    {
        std::string fileString = file;
        std::string error = "Unknown error";
        std::string description = "No description";

        // Decode the error code
        switch (errorCode)
        {
            case AL_INVALID_NAME:
            {
                error = "AL_INVALID_NAME";
                description = "A bad name (ID) has been specified.";
                break;
            }

            case AL_INVALID_ENUM:
            {
                error = "AL_INVALID_ENUM";
                description = "An unacceptable value has been specified for an enumerated argument.";
                break;
            }

            case AL_INVALID_VALUE:
            {
                error = "AL_INVALID_VALUE";
                description = "A numeric argument is out of range.";
                break;
            }

            case AL_INVALID_OPERATION:
            {
                error = "AL_INVALID_OPERATION";
                description = "The specified operation is not allowed in the current state.";
                break;
            }

            case AL_OUT_OF_MEMORY:
            {
                error = "AL_OUT_OF_MEMORY";
                description = "There is not enough memory left to execute the command.";
                break;
            }
        }

        // Log the error
        err() << "An internal OpenAL call failed in "
              << fileString.substr(fileString.find_last_of("\\/") + 1) << "(" << line << ")."
              << "\nExpression:\n   " << expression
              << "\nError description:\n   " << error << "\n   " << description << "\n"
              << std::endl;
    }
}

} // namespace priv

} // namespace sf
