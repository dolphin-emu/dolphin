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


namespace sf
{
namespace priv
{
    template <typename T> SoundFileReader* createReader() {return new T;}
    template <typename T> SoundFileWriter* createWriter() {return new T;}
}

////////////////////////////////////////////////////////////
template <typename T>
void SoundFileFactory::registerReader()
{
    // Make sure the same class won't be registered twice
    unregisterReader<T>();

    // Create a new factory with the functions provided by the class
    ReaderFactory factory;
    factory.check = &T::check;
    factory.create = &priv::createReader<T>;

    // Add it
    s_readers.push_back(factory);
}


////////////////////////////////////////////////////////////
template <typename T>
void SoundFileFactory::unregisterReader()
{
    // Remove the instance(s) of the reader from the array of factories
    for (ReaderFactoryArray::iterator it = s_readers.begin(); it != s_readers.end(); )
    {
        if (it->create == &priv::createReader<T>)
            it = s_readers.erase(it);
        else
            ++it;
    }
}

////////////////////////////////////////////////////////////
template <typename T>
void SoundFileFactory::registerWriter()
{
    // Make sure the same class won't be registered twice
    unregisterWriter<T>();

    // Create a new factory with the functions provided by the class
    WriterFactory factory;
    factory.check = &T::check;
    factory.create = &priv::createWriter<T>;

    // Add it
    s_writers.push_back(factory);
}


////////////////////////////////////////////////////////////
template <typename T>
void SoundFileFactory::unregisterWriter()
{
    // Remove the instance(s) of the writer from the array of factories
    for (WriterFactoryArray::iterator it = s_writers.begin(); it != s_writers.end(); )
    {
        if (it->create == &priv::createWriter<T>)
            it = s_writers.erase(it);
        else
            ++it;
    }
}

} // namespace sf
