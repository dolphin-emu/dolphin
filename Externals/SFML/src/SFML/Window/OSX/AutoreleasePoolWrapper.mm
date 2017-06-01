////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Marco Antognini (antognini.marco@gmail.com),
//                         Laurent Gomila (laurent@sfml-dev.org)
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
#include <cassert>
#include <pthread.h>

#import <SFML/Window/OSX/AutoreleasePoolWrapper.h>
#import <Foundation/Foundation.h>


////////////////////////////////////////////////////////////
/// Here we manage one and only one pool by thread. This prevents draining one
/// pool and making other pools invalid which can lead to a crash on 10.5 and an
/// annoying message on 10.6 (*** attempt to pop an unknown autorelease pool).
///
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// Private data
////////////////////////////////////////////////////////////
static pthread_key_t  poolKey;
static pthread_once_t initOnceToken = PTHREAD_ONCE_INIT;


////////////////////////////////////////////////////////////
/// \brief (local function) Drain one more time the pool
///        but this time don't create a new one.
///
////////////////////////////////////////////////////////////
static void destroyPool(void* data)
{
    NSAutoreleasePool* pool = (NSAutoreleasePool*)data;
    [pool drain];
}


////////////////////////////////////////////////////////////
/// \brief (local function) Init the pthread key for the pool
///
////////////////////////////////////////////////////////////
static void createPoolKey(void)
{
    pthread_key_create(&poolKey, destroyPool);
}


////////////////////////////////////////////////////////////
/// \brief (local function) Store a new pool for this thread
///
////////////////////////////////////////////////////////////
static void createNewPool(void)
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    pthread_setspecific(poolKey, pool);
}


////////////////////////////////////////////////////////////
void ensureThreadHasPool(void)
{
    pthread_once(&initOnceToken, createPoolKey);
    if (pthread_getspecific(poolKey) == NULL)
    {
        createNewPool();
    }
}


////////////////////////////////////////////////////////////
void drainThreadPool(void)
{
    void* data = pthread_getspecific(poolKey);
    assert(data != NULL);

    // Drain the pool but keep it alive by creating a new one
    destroyPool(data);
    createNewPool();
}

