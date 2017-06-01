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

#ifndef SFML_SOUNDFILEREADERWAV_HPP
#define SFML_SOUNDFILEREADERWAV_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Audio/SoundFileReader.hpp>
#include <string>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Implementation of sound file reader that handles wav files
///
////////////////////////////////////////////////////////////
class SoundFileReaderWav : public SoundFileReader
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Check if this reader can handle a file given by an input stream
    ///
    /// \param stream Source stream to check
    ///
    /// \return True if the file is supported by this reader
    ///
    ////////////////////////////////////////////////////////////
    static bool check(InputStream& stream);

public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    SoundFileReaderWav();

    ////////////////////////////////////////////////////////////
    /// \brief Open a sound file for reading
    ///
    /// \param stream Stream to open
    /// \param info   Structure to fill with the attributes of the loaded sound
    ///
    ////////////////////////////////////////////////////////////
    virtual bool open(sf::InputStream& stream, Info& info);

    ////////////////////////////////////////////////////////////
    /// \brief Change the current read position to the given sample offset
    ///
    /// The sample offset takes the channels into account.
    /// Offsets can be calculated like this:
    /// `sampleNumber * sampleRate * channelCount`
    /// If the given offset exceeds to total number of samples,
    /// this function must jump to the end of the file.
    ///
    /// \param sampleOffset Index of the sample to jump to, relative to the beginning
    ///
    ////////////////////////////////////////////////////////////
    virtual void seek(Uint64 sampleOffset);

    ////////////////////////////////////////////////////////////
    /// \brief Read audio samples from the open file
    ///
    /// \param samples  Pointer to the sample array to fill
    /// \param maxCount Maximum number of samples to read
    ///
    /// \return Number of samples actually read (may be less than \a maxCount)
    ///
    ////////////////////////////////////////////////////////////
    virtual Uint64 read(Int16* samples, Uint64 maxCount);

private:

    ////////////////////////////////////////////////////////////
    /// \brief Read the header of the open file
    ///
    /// \param info Attributes of the sound file
    ///
    /// \return True on success, false on error
    ///
    ////////////////////////////////////////////////////////////
    bool parseHeader(Info& info);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    InputStream* m_stream;         ///< Source stream to read from
    unsigned int m_bytesPerSample; ///< Size of a sample, in bytes
    Uint64       m_dataStart;      ///< Starting position of the audio data in the open file
    Uint64       m_dataEnd;        ///< Position one byte past the end of the audio data in the open file
};

} // namespace priv

} // namespace sf


#endif // SFML_SOUNDFILEREADERWAV_HPP
