//
// Copyright (C) 2016 Google, Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of Google Inc. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef GLSLANG_GTESTS_INITIALIZER_H
#define GLSLANG_GTESTS_INITIALIZER_H

#include "glslang/Public/ShaderLang.h"

namespace glslangtest {

// Initializes glslang on creation, and destroys it on completion.
// And provides .Acquire() as a way to reinitialize glslang if semantics change.
// This object is expected to be a singleton, so that internal glslang state
// can be correctly handled.
//
// TODO(antiagainst): It's a known bug that some of the internal states need to
// be reset if semantics change:
//   https://github.com/KhronosGroup/glslang/issues/166
// Therefore, the following mechanism is needed. Remove this once the above bug
// gets fixed.
class GlslangInitializer {
public:
    GlslangInitializer() : lastMessages(EShMsgDefault)
    {
        glslang::InitializeProcess();
    }

    ~GlslangInitializer() { glslang::FinalizeProcess(); }

    // A token indicates that the glslang is reinitialized (if necessary) to the
    // required semantics. And that won't change until the token is destroyed.
    class InitializationToken {
    };

    // Re-initializes glsl state iff the previous messages and the current
    // messages are incompatible.  We assume external synchronization, i.e.
    // there is at most one acquired token at any one time.
    InitializationToken acquire(EShMessages new_messages)
    {
        if ((lastMessages ^ new_messages) &
            (EShMsgVulkanRules | EShMsgSpvRules)) {
            glslang::FinalizeProcess();
            glslang::InitializeProcess();
        }
        lastMessages = new_messages;
        return InitializationToken();
    }

private:

    EShMessages lastMessages;
};

}  // namespace glslangtest

#endif  // GLSLANG_GTESTS_INITIALIZER_H
