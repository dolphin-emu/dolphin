// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

class RasterFont
{
public:
	RasterFont();
	~RasterFont(void);
	static int debug;

	// some useful constants
	enum {char_width = 10};
	enum {char_height = 15};

	// and the happy helper functions
	void printString(const char *s, double x, double y, double z=0.0);
	void printCenteredString(const char *s, double y, int screen_width, double z=0.0);

	void printMultilineText(const char *text, double x, double y, double z, int bbWidth, int bbHeight);
private:
	int fontOffset;
	char *temp_buffer;
	enum {TEMP_BUFFER_SIZE = 64 * 1024};
};
