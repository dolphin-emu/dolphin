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

#ifndef SFML_SOUNDFILEWRITER_HPP
#define SFML_SOUNDFILEWRITER_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Audio/Export.hpp>
#include <string>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Abstract base class for sound file encoding
///
////////////////////////////////////////////////////////////
class SFML_AUDIO_API SoundFileWriter
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Virtual destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~SoundFileWriter() {}

    ////////////////////////////////////////////////////////////
    /// \brief Open a sound file for writing
    ///
    /// \param filename     Path of the file to open
    /// \param sampleRate   Sample rate of the sound
    /// \param channelCount Number of channels of the sound
    ///
    /// \return True if the file was successfully opened
    ///
    ////////////////////////////////////////////////////////////
    virtual bool open(const std::string& filename, unsigned int sampleRate, unsigned int channelCount) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Write audio samples to the open file
    ///
    /// \param samples Pointer to the sample array to write
    /// \param count   Number of samples to write
    ///
    ////////////////////////////////////////////////////////////
    virtual void write(const Int16* samples, Uint64 count) = 0;
};

} // namespace sf


#endif // SFML_SOUNDFILEWRITER_HPP


////////////////////////////////////////////////////////////
/// \class sf::SoundFileWriter
/// \ingroup audio
///
/// This class allows users to write audio file formats not natively
/// supported by SFML, and thus extend the set of supported writable
/// audio formats.
///
/// A valid sound file writer must override the open and write functions,
/// as well as providing a static check function; the latter is used by
/// SFML to find a suitable writer for a given filename.
///
/// To register a new writer, use the sf::SoundFileFactory::registerWriter
/// template function.
///
/// Usage example:
/// \code
/// class MySoundFileWriter : public sf::SoundFileWriter
/// {
/// public:
///
///     static bool check(const std::string& filename)
///     {
///         // typically, check the extension
///         // return true if the writer can handle the format
///     }
///
///     virtual bool open(const std::string& filename, unsigned int sampleRate, unsigned int channelCount)
///     {
///         // open the file 'filename' for writing,
///         // write the given sample rate and channel count to the file header
///         // return true on success
///     }
///
///     virtual void write(const sf::Int16* samples, sf::Uint64 count)
///     {
///         // write 'count' samples stored at address 'samples',
///         // convert them (for example to normalized float) if the format requires it
///     }
/// };
///
/// sf::SoundFileFactory::registerWriter<MySoundFileWriter>();
/// \endcode
///
/// \see sf::OutputSoundFile, sf::SoundFileFactory, sf::SoundFileReader
///
////////////////////////////////////////////////////////////
