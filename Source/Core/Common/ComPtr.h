// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32

// Disable warning C4265 in wrl/client.h:
//   'Microsoft::WRL::Details::RemoveIUnknownBase<T>': class has virtual functions, but destructor
//   is not virtual
#pragma warning(push)
#pragma warning(disable : 4265)
#include <wrl/client.h>
#pragma warning(pop)

namespace Common
{
// Smart pointer class for a COM object.
//
// BEWARE: Taking a pointer to a ComPtr, i.e. `&my_pointer`, causes the object to be released and
//         the pointer to be set to null. This is meant for object creation functions, such as:
//
//           `device->CreateTexture2D(&params, &my_pointer); // <-- THIS IS OK`
//
//         Do NOT take a pointer to a ComPtr when passing the object as a parameter. For example:
//
//           `device->OMSetRenderTargets(1, &my_pointer); // <-- DON'T DO THIS`
//
using Microsoft::WRL::ComPtr;
}

#endif
