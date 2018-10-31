// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoader.h"

class VertexLoader_Normal
{
public:
  // Init
  static void Init();

  // GetSize
  static unsigned int GetSize(u64 _type, unsigned int _format, unsigned int _elements,
                              unsigned int _index3);

  // GetFunction
  static TPipelineFunction GetFunction(u64 _type, unsigned int _format, unsigned int _elements,
                                       unsigned int _index3);

private:
  enum ENormalType
  {
    NRM_NOT_PRESENT = 0,
    NRM_DIRECT = 1,
    NRM_INDEX8 = 2,
    NRM_INDEX16 = 3,
    NUM_NRM_TYPE
  };

  enum ENormalFormat
  {
    FORMAT_UBYTE = 0,
    FORMAT_BYTE = 1,
    FORMAT_USHORT = 2,
    FORMAT_SHORT = 3,
    FORMAT_FLOAT = 4,
    NUM_NRM_FORMAT
  };

  enum ENormalElements
  {
    NRM_NBT = 0,
    NRM_NBT3 = 1,
    NUM_NRM_ELEMENTS
  };

  enum ENormalIndices
  {
    NRM_INDICES1 = 0,
    NRM_INDICES3 = 1,
    NUM_NRM_INDICES
  };

  struct Set
  {
    template <typename T>
    void operator=(const T&)
    {
      gc_size = T::size;
      function = T::function;
    }

    int gc_size;
    TPipelineFunction function;
  };

  static Set m_Table[NUM_NRM_TYPE][NUM_NRM_INDICES][NUM_NRM_ELEMENTS][NUM_NRM_FORMAT];
};
