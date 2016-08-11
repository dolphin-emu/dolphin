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
#include <SFML/Audio/SoundFileFactory.hpp>
#include <SFML/Audio/SoundFileReaderFlac.hpp>
#include <SFML/Audio/SoundFileWriterFlac.hpp>
#include <SFML/Audio/SoundFileReaderOgg.hpp>
#include <SFML/Audio/SoundFileWriterOgg.hpp>
#include <SFML/Audio/SoundFileReaderWav.hpp>
#include <SFML/Audio/SoundFileWriterWav.hpp>
#include <SFML/System/FileInputStream.hpp>
#include <SFML/System/MemoryInputStream.hpp>
#include <SFML/System/Err.hpp>


namespace
{
    // Register all the built-in readers and writers if not already done
    void ensureDefaultReadersWritersRegistered()
    {
        static bool registered = false;
        if (!registered)
        {
            sf::SoundFileFactory::registerReader<sf::priv::SoundFileReaderFlac>();
            sf::SoundFileFactory::registerWriter<sf::priv::SoundFileWriterFlac>();
            sf::SoundFileFactory::registerReader<sf::priv::SoundFileReaderOgg>();
            sf::SoundFileFactory::registerWriter<sf::priv::SoundFileWriterOgg>();
            sf::SoundFileFactory::registerReader<sf::priv::SoundFileReaderWav>();
            sf::SoundFileFactory::registerWriter<sf::priv::SoundFileWriterWav>();
            registered = true;
        }
    }
}

namespace sf
{
SoundFileFactory::ReaderFactoryArray SoundFileFactory::s_readers;
SoundFileFactory::WriterFactoryArray SoundFileFactory::s_writers;


////////////////////////////////////////////////////////////
SoundFileReader* SoundFileFactory::createReaderFromFilename(const std::string& filename)
{
    // Register the built-in readers/writers on first call
    ensureDefaultReadersWritersRegistered();

    // Wrap the input file into a file stream
    FileInputStream stream;
    if (!stream.open(filename)) {
        err() << "Failed to open sound file \"" << filename << "\" (couldn't open stream)" << std::endl;
        return NULL;
    }

    // Test the filename in all the registered factories
    for (ReaderFactoryArray::const_iterator it = s_readers.begin(); it != s_readers.end(); ++it)
    {
        stream.seek(0);
        if (it->check(stream))
            return it->create();
    }

    // No suitable reader found
    err() << "Failed to open sound file \"" << filename << "\" (format not supported)" << std::endl;
    return NULL;
}


////////////////////////////////////////////////////////////
SoundFileReader* SoundFileFactory::createReaderFromMemory(const void* data, std::size_t sizeInBytes)
{
    // Register the built-in readers/writers on first call
    ensureDefaultReadersWritersRegistered();

    // Wrap the memory file into a file stream
    MemoryInputStream stream;
    stream.open(data, sizeInBytes);

    // Test the stream for all the registered factories
    for (ReaderFactoryArray::const_iterator it = s_readers.begin(); it != s_readers.end(); ++it)
    {
        stream.seek(0);
        if (it->check(stream))
            return it->create();
    }

    // No suitable reader found
    err() << "Failed to open sound file from memory (format not supported)" << std::endl;
    return NULL;
}


////////////////////////////////////////////////////////////
SoundFileReader* SoundFileFactory::createReaderFromStream(InputStream& stream)
{
    // Register the built-in readers/writers on first call
    ensureDefaultReadersWritersRegistered();

    // Test the stream for all the registered factories
    for (ReaderFactoryArray::const_iterator it = s_readers.begin(); it != s_readers.end(); ++it)
    {
        stream.seek(0);
        if (it->check(stream))
            return it->create();
    }

    // No suitable reader found
    err() << "Failed to open sound file from stream (format not supported)" << std::endl;
    return NULL;
}


////////////////////////////////////////////////////////////
SoundFileWriter* SoundFileFactory::createWriterFromFilename(const std::string& filename)
{
    // Register the built-in readers/writers on first call
    ensureDefaultReadersWritersRegistered();

    // Test the filename in all the registered factories
    for (WriterFactoryArray::const_iterator it = s_writers.begin(); it != s_writers.end(); ++it)
    {
        if (it->check(filename))
            return it->create();
    }

    // No suitable writer found
    err() << "Failed to open sound file \"" << filename << "\" (format not supported)" << std::endl;
    return NULL;
}

} // namespace sf
