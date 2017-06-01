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
#include <SFML/Audio/SoundFileWriterOgg.hpp>
#include <SFML/System/Err.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cassert>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
bool SoundFileWriterOgg::check(const std::string& filename)
{
    std::string extension = filename.substr(filename.find_last_of(".") + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    return extension == "ogg";
}


////////////////////////////////////////////////////////////
SoundFileWriterOgg::SoundFileWriterOgg() :
m_channelCount(0),
m_file        (),
m_ogg         (),
m_vorbis      (),
m_state       ()
{
}


////////////////////////////////////////////////////////////
SoundFileWriterOgg::~SoundFileWriterOgg()
{
    close();
}


////////////////////////////////////////////////////////////
bool SoundFileWriterOgg::open(const std::string& filename, unsigned int sampleRate, unsigned int channelCount)
{
    // Save the channel count
    m_channelCount = channelCount;

    // Initialize the ogg/vorbis stream
    ogg_stream_init(&m_ogg, std::rand());
    vorbis_info_init(&m_vorbis);

    // Setup the encoder: VBR, automatic bitrate management
    // Quality is in range [-1 .. 1], 0.4 gives ~128 kbps for a 44 KHz stereo sound
    int status = vorbis_encode_init_vbr(&m_vorbis, channelCount, sampleRate, 0.4f);
    if (status < 0)
    {
        err() << "Failed to write ogg/vorbis file \"" << filename << "\" (unsupported bitrate)" << std::endl;
        close();
        return false;
    }
    vorbis_analysis_init(&m_state, &m_vorbis);

    // Open the file after the vorbis setup is ok
    m_file.open(filename.c_str(), std::ios::binary);
    if (!m_file)
    {
        err() << "Failed to write ogg/vorbis file \"" << filename << "\" (cannot open file)" << std::endl;
        close();
        return false;
    }

    // Generate header metadata (leave it empty)
    vorbis_comment comment;
    vorbis_comment_init(&comment);

    // Generate the header packets
    ogg_packet header, headerComm, headerCode;
    status = vorbis_analysis_headerout(&m_state, &comment, &header, &headerComm, &headerCode);
    vorbis_comment_clear(&comment);
    if (status < 0)
    {
        err() << "Failed to write ogg/vorbis file \"" << filename << "\" (cannot generate the headers)" << std::endl;
        close();
        return false;
    }

    // Write the header packets to the ogg stream
    ogg_stream_packetin(&m_ogg, &header);
    ogg_stream_packetin(&m_ogg, &headerComm);
    ogg_stream_packetin(&m_ogg, &headerCode);

    // This ensures the actual audio data will start on a new page, as per spec
    ogg_page page;
    while (ogg_stream_flush(&m_ogg, &page) > 0)
    {
        m_file.write(reinterpret_cast<const char*>(page.header), page.header_len);
        m_file.write(reinterpret_cast<const char*>(page.body), page.body_len);
    }

    return true;
}


////////////////////////////////////////////////////////////
void SoundFileWriterOgg::write(const Int16* samples, Uint64 count)
{
    // Vorbis has issues with buffers that are too large, so we ask for 64K
    static const int bufferSize = 65536;

    // A frame contains a sample from each channel
    int frameCount = static_cast<int>(count / m_channelCount);

    while (frameCount > 0)
    {
        // Prepare a buffer to hold our samples
        float** buffer = vorbis_analysis_buffer(&m_state, bufferSize);
        assert(buffer);

        // Write the samples to the buffer, converted to float
        for (int i = 0; i < std::min(frameCount, bufferSize); ++i)
            for (unsigned int j = 0; j < m_channelCount; ++j)
                buffer[j][i] = *samples++ / 32767.0f;

        // Tell the library how many samples we've written
        vorbis_analysis_wrote(&m_state, std::min(frameCount, bufferSize));
        
        frameCount -= bufferSize;

        // Flush any produced block
        flushBlocks();
    }
}


////////////////////////////////////////////////////////////
void SoundFileWriterOgg::flushBlocks()
{
    // Let the library divide uncompressed data into blocks, and process them
    vorbis_block block;
    vorbis_block_init(&m_state, &block);
    while (vorbis_analysis_blockout(&m_state, &block) == 1)
    {
        // Let the automatic bitrate management do its job
        vorbis_analysis(&block, NULL);
        vorbis_bitrate_addblock(&block);

        // Get new packets from the bitrate management engine
        ogg_packet packet;
        while (vorbis_bitrate_flushpacket(&m_state, &packet))
        {
            // Write the packet to the ogg stream
            ogg_stream_packetin(&m_ogg, &packet);

            // If the stream produced new pages, write them to the output file
            ogg_page page;
            while (ogg_stream_flush(&m_ogg, &page) > 0)
            {
                m_file.write(reinterpret_cast<const char*>(page.header), page.header_len);
                m_file.write(reinterpret_cast<const char*>(page.body), page.body_len);
            }
        }
    }

    // Clear the allocated block
    vorbis_block_clear(&block);
}


////////////////////////////////////////////////////////////
void SoundFileWriterOgg::close()
{
    if (m_file.is_open())
    {
        // Submit an empty packet to mark the end of stream
        vorbis_analysis_wrote(&m_state, 0);
        flushBlocks();

        // Close the file
        m_file.close();
    }

    // Clear all the ogg/vorbis structures
    ogg_stream_clear(&m_ogg);
    vorbis_dsp_clear(&m_state);
    vorbis_info_clear(&m_vorbis);
}

} // namespace priv

} // namespace sf
