// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/GLUtil.h"

#include "VideoCommon/VideoConfig.h"

static GLuint s_bbox_buffer_id;

namespace OGL
{

void BoundingBox::Init()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox)
	{
		int initial_values[4] = {0,0,0,0};
		glGenBuffers(1, &s_bbox_buffer_id);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 4 * sizeof(s32), initial_values, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, s_bbox_buffer_id);
	}
}

void BoundingBox::Shutdown()
{
	if (g_ActiveConfig.backend_info.bSupportsBBox)
		glDeleteBuffers(1, &s_bbox_buffer_id);
}

void BoundingBox::Set(int index, int value)
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(int), sizeof(int), &value);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

int BoundingBox::Get(int index)
{
	int data = 0;
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
	void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, index * sizeof(int), sizeof(int), GL_MAP_READ_BIT);
	if (ptr)
	{
		data = *(int*)ptr;
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	return data;
}

};
