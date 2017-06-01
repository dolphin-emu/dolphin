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

#ifndef SFML_INPUTSOUNDFILE_HPP
#define SFML_INPUTSOUNDFILE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Audio/Export.hpp>
#include <SFML/System/NonCopyable.hpp>
#include <SFML/System/Time.hpp>
#include <string>


namespace sf
{
class InputStream;
class SoundFileReader;

////////////////////////////////////////////////////////////
/// \brief Provide read access to sound files
///
////////////////////////////////////////////////////////////
class SFML_AUDIO_API InputSoundFile : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    InputSoundFile();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~InputSoundFile();

    ////////////////////////////////////////////////////////////
    /// \brief Open a sound file from the disk for reading
    ///
    /// The supported audio formats are: WAV (PCM only), OGG/Vorbis, FLAC.
    /// The supported sample sizes for FLAC and WAV are 8, 16, 24 and 32 bit.
    ///
    /// \param filename Path of the sound file to load
    ///
    /// \return True if the file was successfully opened
    ///
    ////////////////////////////////////////////////////////////
    bool openFromFile(const std::string& filename);

    ////////////////////////////////////////////////////////////
    /// \brief Open a sound file in memory for reading
    ///
    /// The supported audio formats are: WAV (PCM only), OGG/Vorbis, FLAC.
    /// The supported sample sizes for FLAC and WAV are 8, 16, 24 and 32 bit.
    ///
    /// \param data        Pointer to the file data in memory
    /// \param sizeInBytes Size of the data to load, in bytes
    ///
    /// \return True if the file was successfully opened
    ///
    ////////////////////////////////////////////////////////////
    bool openFromMemory(const void* data, std::size_t sizeInBytes);

    ////////////////////////////////////////////////////////////
    /// \brief Open a sound file from a custom stream for reading
    ///
    /// The supported audio formats are: WAV (PCM only), OGG/Vorbis, FLAC.
    /// The supported sample sizes for FLAC and WAV are 8, 16, 24 and 32 bit.
    ///
    /// \param stream Source stream to read from
    ///
    /// \return True if the file was successfully opened
    ///
    ////////////////////////////////////////////////////////////
    bool openFromStream(InputStream& stream);

    ////////////////////////////////////////////////////////////
    /// \brief Open the sound file from the disk for writing
    ///
    /// \param filename     Path of the sound file to write
    /// \param channelCount Number of channels in the sound
    /// \param sampleRate   Sample rate of the sound
    ///
    /// \return True if the file was successfully opened
    ///
    ////////////////////////////////////////////////////////////
    bool openForWriting(const std::string& filename, unsigned int channelCount, unsigned int sampleRate);

    ////////////////////////////////////////////////////////////
    /// \brief Get the total number of audio samples in the file
    ///
    /// \return Number of samples
    ///
    ////////////////////////////////////////////////////////////
    Uint64 getSampleCount() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the number of channels used by the sound
    ///
    /// \return Number of channels (1 = mono, 2 = stereo)
    ///
    ////////////////////////////////////////////////////////////
    unsigned int getChannelCount() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the sample rate of the sound
    ///
    /// \return Sample rate, in samples per second
    ///
    ////////////////////////////////////////////////////////////
    unsigned int getSampleRate() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the total duration of the sound file
    ///
    /// This function is provided for convenience, the duration is
    /// deduced from the other sound file attributes.
    ///
    /// \return Duration of the sound file
    ///
    ////////////////////////////////////////////////////////////
    Time getDuration() const;

    ////////////////////////////////////////////////////////////
    /// \brief Change the current read position to the given sample offset
    ///
    /// This function takes a sample offset to provide maximum
    /// precision. If you need to jump to a given time, use the
    /// other overload.
    ///
    /// The sample offset takes the channels into account.
    /// Offsets can be calculated like this:
    /// `sampleNumber * sampleRate * channelCount`
    /// If the given offset exceeds to total number of samples,
    /// this function jumps to the end of the sound file.
    ///
    /// \param sampleOffset Index of the sample to jump to, relative to the beginning
    ///
    ////////////////////////////////////////////////////////////
    void seek(Uint64 sampleOffset);

    ////////////////////////////////////////////////////////////
    /// \brief Change the current read position to the given time offset
    ///
    /// Using a time offset is handy but imprecise. If you need an accurate
    /// result, consider using the overload which takes a sample offset.
    ///
    /// If the given time exceeds to total duration, this function jumps
    /// to the end of the sound file.
    ///
    /// \param timeOffset Time to jump to, relative to the beginning
    ///
    ////////////////////////////////////////////////////////////
    void seek(Time timeOffset);

    ////////////////////////////////////////////////////////////
    /// \brief Read audio samples from the open file
    ///
    /// \param samples  Pointer to the sample array to fill
    /// \param maxCount Maximum number of samples to read
    ///
    /// \return Number of samples actually read (may be less than \a maxCount)
    ///
    ////////////////////////////////////////////////////////////
    Uint64 read(Int16* samples, Uint64 maxCount);

private:

    ////////////////////////////////////////////////////////////
    /// \brief Close the current file
    ///
    ////////////////////////////////////////////////////////////
    void close();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    SoundFileReader* m_reader;       ///< Reader that handles I/O on the file's format
    InputStream*     m_stream;       ///< Input stream used to access the file's data
    bool             m_streamOwned;  ///< Is the stream internal or external?
    Uint64           m_sampleCount;  ///< Total number of samples in the file
    unsigned int     m_channelCount; ///< Number of channels of the sound
    unsigned int     m_sampleRate;   ///< Number of samples per second
};

} // namespace sf


#endif // SFML_INPUTSOUNDFILE_HPP


////////////////////////////////////////////////////////////
/// \class sf::InputSoundFile
/// \ingroup audio
///
/// This class decodes audio samples from a sound file. It is
/// used internally by higher-level classes such as sf::SoundBuffer
/// and sf::Music, but can also be useful if you want to process
/// or analyze audio files without playing them, or if you want to
/// implement your own version of sf::Music with more specific
/// features.
///
/// Usage example:
/// \code
/// // Open a sound file
/// sf::InputSoundFile file;
/// if (!file.openFromFile("music.ogg"))
///     /* error */;
///
/// // Print the sound attributes
/// std::cout << "duration: " << file.getDuration().asSeconds() << std::endl;
/// std::cout << "channels: " << file.getChannelCount() << std::endl;
/// std::cout << "sample rate: " << file.getSampleRate() << std::endl;
/// std::cout << "sample count: " << file.getSampleCount() << std::endl;
///
/// // Read and process batches of samples until the end of file is reached
/// sf::Int16 samples[1024];
/// sf::Uint64 count;
/// do
/// {
///     count = file.read(samples, 1024);
///
///     // process, analyze, play, convert, or whatever
///     // you want to do with the samples...
/// }
/// while (count > 0);
/// \endcode
///
/// \see sf::SoundFileReader, sf::OutputSoundFile
///
////////////////////////////////////////////////////////////
