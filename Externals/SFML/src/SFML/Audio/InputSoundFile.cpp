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
#include <SFML/Audio/InputSoundFile.hpp>
#include <SFML/Audio/SoundFileReader.hpp>
#include <SFML/Audio/SoundFileFactory.hpp>
#include <SFML/System/InputStream.hpp>
#include <SFML/System/FileInputStream.hpp>
#include <SFML/System/MemoryInputStream.hpp>
#include <SFML/System/Err.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
InputSoundFile::InputSoundFile() :
m_reader      (NULL),
m_stream      (NULL),
m_streamOwned (false),
m_sampleCount (0),
m_channelCount(0),
m_sampleRate  (0)
{
}


////////////////////////////////////////////////////////////
InputSoundFile::~InputSoundFile()
{
    // Close the file in case it was open
    close();
}


////////////////////////////////////////////////////////////
bool InputSoundFile::openFromFile(const std::string& filename)
{
    // If the file is already open, first close it
    close();

    // Find a suitable reader for the file type
    m_reader = SoundFileFactory::createReaderFromFilename(filename);
    if (!m_reader)
        return false;

    // Wrap the file into a stream
    FileInputStream* file = new FileInputStream;
    m_stream = file;
    m_streamOwned = true;

    // Open it
    if (!file->open(filename))
    {
        close();
        return false;
    }

    // Pass the stream to the reader
    SoundFileReader::Info info;
    if (!m_reader->open(*file, info))
    {
        close();
        return false;
    }

    // Retrieve the attributes of the open sound file
    m_sampleCount = info.sampleCount;
    m_channelCount = info.channelCount;
    m_sampleRate = info.sampleRate;

    return true;
}


////////////////////////////////////////////////////////////
bool InputSoundFile::openFromMemory(const void* data, std::size_t sizeInBytes)
{
    // If the file is already open, first close it
    close();

    // Find a suitable reader for the file type
    m_reader = SoundFileFactory::createReaderFromMemory(data, sizeInBytes);
    if (!m_reader)
        return false;

    // Wrap the memory file into a stream
    MemoryInputStream* memory = new MemoryInputStream;
    m_stream = memory;
    m_streamOwned = true;

    // Open it
    memory->open(data, sizeInBytes);

    // Pass the stream to the reader
    SoundFileReader::Info info;
    if (!m_reader->open(*memory, info))
    {
        close();
        return false;
    }

    // Retrieve the attributes of the open sound file
    m_sampleCount = info.sampleCount;
    m_channelCount = info.channelCount;
    m_sampleRate = info.sampleRate;

    return true;
}


////////////////////////////////////////////////////////////
bool InputSoundFile::openFromStream(InputStream& stream)
{
    // If the file is already open, first close it
    close();

    // Find a suitable reader for the file type
    m_reader = SoundFileFactory::createReaderFromStream(stream);
    if (!m_reader)
        return false;

    // store the stream
    m_stream = &stream;
    m_streamOwned = false;

    // Don't forget to reset the stream to its beginning before re-opening it
    if (stream.seek(0) != 0)
    {
        err() << "Failed to open sound file from stream (cannot restart stream)" << std::endl;
        return false;
    }

    // Pass the stream to the reader
    SoundFileReader::Info info;
    if (!m_reader->open(stream, info))
    {
        close();
        return false;
    }

    // Retrieve the attributes of the open sound file
    m_sampleCount = info.sampleCount;
    m_channelCount = info.channelCount;
    m_sampleRate = info.sampleRate;

    return true;
}


////////////////////////////////////////////////////////////
Uint64 InputSoundFile::getSampleCount() const
{
    return m_sampleCount;
}


////////////////////////////////////////////////////////////
unsigned int InputSoundFile::getChannelCount() const
{
    return m_channelCount;
}


////////////////////////////////////////////////////////////
unsigned int InputSoundFile::getSampleRate() const
{
    return m_sampleRate;
}


////////////////////////////////////////////////////////////
Time InputSoundFile::getDuration() const
{
    return seconds(static_cast<float>(m_sampleCount) / m_channelCount / m_sampleRate);
}


////////////////////////////////////////////////////////////
void InputSoundFile::seek(Uint64 sampleOffset)
{
    if (m_reader)
        m_reader->seek(sampleOffset);
}


////////////////////////////////////////////////////////////
void InputSoundFile::seek(Time timeOffset)
{
    seek(static_cast<Uint64>(timeOffset.asSeconds() * m_sampleRate * m_channelCount));
}


////////////////////////////////////////////////////////////
Uint64 InputSoundFile::read(Int16* samples, Uint64 maxCount)
{
    if (m_reader && samples && maxCount)
        return m_reader->read(samples, maxCount);
    else
        return 0;
}


////////////////////////////////////////////////////////////
void InputSoundFile::close()
{
    // Destroy the reader
    delete m_reader;
    m_reader = NULL;

    // Destroy the stream if we own it
    if (m_streamOwned)
    {
        delete m_stream;
        m_streamOwned = false;
    }
    m_stream = NULL;

    // Reset the sound file attributes
    m_sampleCount = 0;
    m_channelCount = 0;
    m_sampleRate = 0;
}

} // namespace sf
