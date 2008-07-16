#ifndef _JITBACKPATCH_H
#define _JITBACKPATCH_H

#include "Common.h"

namespace Jit64 {
void BackPatch(u8 *codePtr, int accessType, u32 emAddress);
}

#endif
