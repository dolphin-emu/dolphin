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

#import "NSString+stdstring.h"
#include <SFML/System/Utf.hpp>

@implementation NSString (NSString_stdstring)

+(id)stringWithstdstring:(const std::string&)string
{
    std::string utf8;
    utf8.reserve(string.size() + 1);

    sf::Utf8::fromAnsi(string.begin(), string.end(), std::back_inserter(utf8));

    NSString* str = [NSString stringWithCString:utf8.c_str()
                                       encoding:NSUTF8StringEncoding];
    return str;
}

+(id)stringWithstdwstring:(const std::wstring&)string
{
    char* data = (char*)string.data();
    unsigned size = string.size() * sizeof(wchar_t);

    NSString* str = [[[NSString alloc] initWithBytes:data length:size
                                            encoding:NSUTF32LittleEndianStringEncoding] autorelease];
    return str;
}

-(std::string)tostdstring
{
    // Not sure about the encoding to use. Using [self UTF8String] doesn't
    // work for characters like é or à.
    const char *cstr = [self cStringUsingEncoding:NSISOLatin1StringEncoding];

    if (cstr != NULL)
        return std::string(cstr);
    else
        return "";
}

-(std::wstring)tostdwstring
{
    // According to Wikipedia, Mac OS X is Little Endian on x86 and x86-64
    // http://en.wikipedia.org/wiki/Endianness
    NSData* asData = [self dataUsingEncoding:NSUTF32LittleEndianStringEncoding];
    return std::wstring((wchar_t*)[asData bytes], [asData length] / sizeof(wchar_t));
}

@end
