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
#include <SFML/Audio/SoundFileWriterWav.hpp>
#include <SFML/System/Err.hpp>
#include <algorithm>
#include <cctype>
#include <cassert>


namespace
{
    // The following functions takes integers in host byte order
    // and writes them to a stream as little endian

    void encode(std::ostream& stream, sf::Int16 value)
    {
        unsigned char bytes[] =
        {
            static_cast<unsigned char>(value & 0xFF),
            static_cast<unsigned char>(value >> 8)
        };
        stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }

    void encode(std::ostream& stream, sf::Uint16 value)
    {
        unsigned char bytes[] =
        {
            static_cast<unsigned char>(value & 0xFF),
            static_cast<unsigned char>(value >> 8)
        };
        stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }

    void encode(std::ostream& stream, sf::Uint32 value)
    {
        unsigned char bytes[] =
        {
            static_cast<unsigned char>(value & 0x000000FF),
            static_cast<unsigned char>((value & 0x0000FF00) >> 8),
            static_cast<unsigned char>((value & 0x00FF0000) >> 16),
            static_cast<unsigned char>((value & 0xFF000000) >> 24),
        };
        stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
}

namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
bool SoundFileWriterWav::check(const std::string& filename)
{
    std::string extension = filename.substr(filename.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    return extension == "wav";
}


////////////////////////////////////////////////////////////
SoundFileWriterWav::SoundFileWriterWav() :
m_file        (),
m_sampleCount (0),
m_channelCount(0)
{
}


////////////////////////////////////////////////////////////
SoundFileWriterWav::~SoundFileWriterWav()
{
    close();
}


////////////////////////////////////////////////////////////
bool SoundFileWriterWav::open(const std::string& filename, unsigned int sampleRate, unsigned int channelCount)
{
    // Open the file
    m_file.open(filename.c_str(), std::ios::binary);
    if (!m_file)
    {
        err() << "Failed to open WAV sound file \"" << filename << "\" for writing" << std::endl;
        return false;
    }

    // Write the header
    if (!writeHeader(sampleRate, channelCount))
    {
        err() << "Failed to write header of WAV sound file \"" << filename << "\"" << std::endl;
        return false;
    }

    // Save the channel count
    m_channelCount = channelCount;

    return true;
}


////////////////////////////////////////////////////////////
void SoundFileWriterWav::write(const Int16* samples, Uint64 count)
{
    assert(m_file.good());

    m_sampleCount += count;

    while (count--)
        encode(m_file, *samples++);
}


////////////////////////////////////////////////////////////
bool SoundFileWriterWav::writeHeader(unsigned int sampleRate, unsigned int channelCount)
{
    assert(m_file.good());

    // Write the main chunk ID
    char mainChunkId[4] = {'R', 'I', 'F', 'F'};
    m_file.write(mainChunkId, sizeof(mainChunkId));

    // Write the main chunk header
    Uint32 mainChunkSize = 0; // placeholder, will be written later
    encode(m_file, mainChunkSize);
    char mainChunkFormat[4] = {'W', 'A', 'V', 'E'};
    m_file.write(mainChunkFormat, sizeof(mainChunkFormat));

    // Write the sub-chunk 1 ("format") id and size
    char fmtChunkId[4] = {'f', 'm', 't', ' '};
    m_file.write(fmtChunkId, sizeof(fmtChunkId));
    Uint32 fmtChunkSize = 16;
    encode(m_file, fmtChunkSize);

    // Write the format (PCM)
    Uint16 format = 1;
    encode(m_file, format);

    // Write the sound attributes
    encode(m_file, static_cast<Uint16>(channelCount));
    encode(m_file, static_cast<Uint32>(sampleRate));
    Uint32 byteRate = sampleRate * channelCount * 2;
    encode(m_file, byteRate);
    Uint16 blockAlign = channelCount * 2;
    encode(m_file, blockAlign);
    Uint16 bitsPerSample = 16;
    encode(m_file, bitsPerSample);

    // Write the sub-chunk 2 ("data") id and size
    char dataChunkId[4] = {'d', 'a', 't', 'a'};
    m_file.write(dataChunkId, sizeof(dataChunkId));
    Uint32 dataChunkSize = 0; // placeholder, will be written later
    encode(m_file, dataChunkSize);

    return true;
}


////////////////////////////////////////////////////////////
void SoundFileWriterWav::close()
{
    // If the file is open, finalize the header and close it
    if (m_file.is_open())
    {
        m_file.flush();

        // Update the main chunk size and data sub-chunk size
        Uint32 dataChunkSize = static_cast<Uint32>(m_sampleCount * m_channelCount * 2);
        Uint32 mainChunkSize = dataChunkSize + 36;
        m_file.seekp(4);
        encode(m_file, mainChunkSize);
        m_file.seekp(40);
        encode(m_file, dataChunkSize);

        m_file.close();
    }
}

} // namespace priv

} // namespace sf
