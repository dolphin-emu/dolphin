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

#ifndef SFML_OUTPUTSOUNDFILE_HPP
#define SFML_OUTPUTSOUNDFILE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Audio/Export.hpp>
#include <SFML/System/NonCopyable.hpp>
#include <string>


namespace sf
{
class SoundFileWriter;

////////////////////////////////////////////////////////////
/// \brief Provide write access to sound files
///
////////////////////////////////////////////////////////////
class SFML_AUDIO_API OutputSoundFile : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    OutputSoundFile();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    /// Closes the file if it was still open.
    ///
    ////////////////////////////////////////////////////////////
    ~OutputSoundFile();

    ////////////////////////////////////////////////////////////
    /// \brief Open the sound file from the disk for writing
    ///
    /// The supported audio formats are: WAV, OGG/Vorbis, FLAC.
    ///
    /// \param filename     Path of the sound file to write
    /// \param sampleRate   Sample rate of the sound
    /// \param channelCount Number of channels in the sound
    ///
    /// \return True if the file was successfully opened
    ///
    ////////////////////////////////////////////////////////////
    bool openFromFile(const std::string& filename, unsigned int sampleRate, unsigned int channelCount);

    ////////////////////////////////////////////////////////////
    /// \brief Write audio samples to the file
    ///
    /// \param samples     Pointer to the sample array to write
    /// \param count       Number of samples to write
    ///
    ////////////////////////////////////////////////////////////
    void write(const Int16* samples, Uint64 count);

private:

    ////////////////////////////////////////////////////////////
    /// \brief Close the current file
    ///
    ////////////////////////////////////////////////////////////
    void close();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    SoundFileWriter* m_writer; ///< Writer that handles I/O on the file's format
};

} // namespace sf


#endif // SFML_OUTPUTSOUNDFILE_HPP


////////////////////////////////////////////////////////////
/// \class sf::OutputSoundFile
/// \ingroup audio
///
/// This class encodes audio samples to a sound file. It is
/// used internally by higher-level classes such as sf::SoundBuffer,
/// but can also be useful if you want to create audio files from
/// custom data sources, like generated audio samples.
///
/// Usage example:
/// \code
/// // Create a sound file, ogg/vorbis format, 44100 Hz, stereo
/// sf::OutputSoundFile file;
/// if (!file.openFromFile("music.ogg", 44100, 2))
///     /* error */;
///
/// while (...)
/// {
///     // Read or generate audio samples from your custom source
///     std::vector<sf::Int16> samples = ...;
///
///     // Write them to the file
///     file.write(samples.data(), samples.size());
/// }
/// \endcode
///
/// \see sf::SoundFileWriter, sf::InputSoundFile
///
////////////////////////////////////////////////////////////
