// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _RASTERFONT_H_
#define _RASTERFONT_H_

class RasterFont {
public:
    RasterFont();
    ~RasterFont(void);
    static int debug;

	void printMultilineText(const char *text, double x, double y, double z, int bbWidth, int bbHeight, u32 color);
private:
	
	u32 VBO;
	u32 VAO;
	u32 texture;
	u32 shader_program;
	u32 uniform_color_id;
	u32 cached_color;
};

#endif // _RASTERFONT_H_
