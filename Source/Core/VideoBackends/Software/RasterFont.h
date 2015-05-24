// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class RasterFont
{
public:
	RasterFont();
	~RasterFont();
	static int debug;

	// and the happy helper functions
	void printString(const char *s, double x, double y, double z=0.0);
	void printCenteredString(const char *s, double y, int screen_width, double z=0.0);

	void printMultilineText(const char *text, double x, double y, double z, int bbWidth, int bbHeight);

private:
	int fontOffset;
	char *temp_buffer;

	static const int TEMP_BUFFER_SIZE = 64 * 1024;
	static const int CHAR_WIDTH = 10;
	static const int CHAR_HEIGHT = 15;
};
