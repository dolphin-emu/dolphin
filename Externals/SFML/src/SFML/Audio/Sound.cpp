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
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Audio/ALCheck.hpp>


namespace sf
{
////////////////////////////////////////////////////////////
Sound::Sound() :
m_buffer(NULL)
{
}


////////////////////////////////////////////////////////////
Sound::Sound(const SoundBuffer& buffer) :
m_buffer(NULL)
{
    setBuffer(buffer);
}


////////////////////////////////////////////////////////////
Sound::Sound(const Sound& copy) :
SoundSource(copy),
m_buffer   (NULL)
{
    if (copy.m_buffer)
        setBuffer(*copy.m_buffer);
    setLoop(copy.getLoop());
}


////////////////////////////////////////////////////////////
Sound::~Sound()
{
    stop();
    if (m_buffer)
        m_buffer->detachSound(this);
}


////////////////////////////////////////////////////////////
void Sound::play()
{
    alCheck(alSourcePlay(m_source));
}


////////////////////////////////////////////////////////////
void Sound::pause()
{
    alCheck(alSourcePause(m_source));
}


////////////////////////////////////////////////////////////
void Sound::stop()
{
    alCheck(alSourceStop(m_source));
}


////////////////////////////////////////////////////////////
void Sound::setBuffer(const SoundBuffer& buffer)
{
    // First detach from the previous buffer
    if (m_buffer)
    {
        stop();
        m_buffer->detachSound(this);
    }

    // Assign and use the new buffer
    m_buffer = &buffer;
    m_buffer->attachSound(this);
    alCheck(alSourcei(m_source, AL_BUFFER, m_buffer->m_buffer));
}


////////////////////////////////////////////////////////////
void Sound::setLoop(bool loop)
{
    alCheck(alSourcei(m_source, AL_LOOPING, loop));
}


////////////////////////////////////////////////////////////
void Sound::setPlayingOffset(Time timeOffset)
{
    alCheck(alSourcef(m_source, AL_SEC_OFFSET, timeOffset.asSeconds()));
}


////////////////////////////////////////////////////////////
const SoundBuffer* Sound::getBuffer() const
{
    return m_buffer;
}


////////////////////////////////////////////////////////////
bool Sound::getLoop() const
{
    ALint loop;
    alCheck(alGetSourcei(m_source, AL_LOOPING, &loop));

    return loop != 0;
}


////////////////////////////////////////////////////////////
Time Sound::getPlayingOffset() const
{
    ALfloat secs = 0.f;
    alCheck(alGetSourcef(m_source, AL_SEC_OFFSET, &secs));

    return seconds(secs);
}


////////////////////////////////////////////////////////////
Sound::Status Sound::getStatus() const
{
    return SoundSource::getStatus();
}


////////////////////////////////////////////////////////////
Sound& Sound::operator =(const Sound& right)
{
    // Here we don't use the copy-and-swap idiom, because it would mess up
    // the list of sound instances contained in the buffers and unnecessarily
    // destroy/create OpenAL sound sources

    // Delegate to base class, which copies all the sound attributes
    SoundSource::operator=(right);

    // Detach the sound instance from the previous buffer (if any)
    if (m_buffer)
    {
        stop();
        m_buffer->detachSound(this);
        m_buffer = NULL;
    }

    // Copy the remaining sound attributes
    if (right.m_buffer)
        setBuffer(*right.m_buffer);
    setLoop(right.getLoop());

    return *this;
}


////////////////////////////////////////////////////////////
void Sound::resetBuffer()
{
    // First stop the sound in case it is playing
    stop();

    // Detach the buffer
    if (m_buffer)
    {
        alCheck(alSourcei(m_source, AL_BUFFER, 0));
        m_buffer->detachSound(this);
        m_buffer = NULL;
    }
}

} // namespace sf
