// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class SWOGLWindow
{
public:
	static void Init(void* window_handle);
	static void Shutdown();

	// Will be printed on the *next* image
	void PrintText(const std::string& text, int x, int y, u32 color);

	// Image to show, will be swapped immediately
	void ShowImage(u8* data, int stride, int width, int height, float aspect);

	int PeekMessages();

	static std::unique_ptr<SWOGLWindow> s_instance;

private:
	SWOGLWindow() {}

	void Prepare();

	struct TextData
	{
		std::string text;
		int x, y;
		u32 color;
	};
	std::vector<TextData> m_text;

	bool m_init {false};

	u32 m_image_program, m_image_texture, m_image_vao;
};
