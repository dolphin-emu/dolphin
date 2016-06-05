// Copyright 2008 Dolphin Emulator Project
// Copyright 2016 Will Cobb
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/GL/GLInterfaceBase.h"

class cInterfaceIGL : public cInterfaceBase
{
private:
	bool m_has_handle;
	bool m_supports_surfaceless = false;
    

	bool CreateWindowSurface();
	void DestroyWindowSurface();

protected:
	void DetectMode();
	virtual void ShutdownPlatform() {}

public:
	void Swap() override;
	void SwapInterval(int interval) override;
	void SetMode(GLInterfaceMode mode) override { s_opengl_mode = mode; }
    GLInterfaceMode GetMode() override;
	bool Create(void* window_handle, bool core) override;
	bool Create(cInterfaceBase* main_context) override;
	bool MakeCurrent() override;
	bool ClearCurrent() override;
	void Shutdown() override;
	void UpdateHandle(void* window_handle) override;
	void UpdateSurface() override;
    void Update() override;
    void Prepare() override;
};
