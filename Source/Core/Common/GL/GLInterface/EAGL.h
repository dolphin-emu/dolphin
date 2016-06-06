// Copyright 2016 Dolphin Emulator Project
// Copyright 2016 Will Cobb
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#import <GLKit/GLKit.h>

#include "Common/GL/GLInterfaceBase.h"

class cInterfaceEAGL : public cInterfaceBase
{
public:
	void Swap() override;
	void SetMode(GLInterfaceMode mode) override { s_opengl_mode = mode; }
	GLInterfaceMode GetMode() override;
	bool Create(void* window_handle, bool core) override;
	bool Create(cInterfaceBase* main_context) override;
	bool MakeCurrent() override;
	bool ClearCurrent() override;
	void Shutdown() override;
	void UpdateHandle(void* window_handle) override;
	void Prepare() override;
	int GetDefaultFramebuffer() override;

protected:
	void DetectMode();
	virtual void ShutdownPlatform() {}

private:
	EAGLContext *m_context;
	GLKView		*m_glkView;
	bool		m_bufferStorageCreated;
};
