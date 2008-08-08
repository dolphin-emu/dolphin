// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef RasterFont_Header
#define RasterFont_Header

class RasterFont {
protected:
    int	fontOffset;
    
public:
    RasterFont();
    ~RasterFont(void);
    static int debug;
    
    // some useful constants
    enum	{char_width = 10};
    enum	{char_height = 15};
    
    // and the happy helper functions
    void printString(const char *s, double x, double y, double z=0.0);
    void printCenteredString(const char *s, double y, int screen_width, double z=0.0);

	void printStuff(const char *text, double x, double y, double z, int bbWidth, int bbHeight);
};

#endif
