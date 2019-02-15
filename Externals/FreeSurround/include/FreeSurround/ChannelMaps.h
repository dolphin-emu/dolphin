/*
Copyright (C) 2010 Christian Kothe

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef CHANNELMAPS_H
#define CHANNELMAPS_H
#include "FreeSurroundDecoder.h"
#include <map>
#include <vector>

const int grid_res = 21; // resolution of the lookup grid

// channel allocation maps (per setup)
typedef std::vector<std::vector<float *>> alloc_lut;
extern std::map<unsigned, alloc_lut> chn_alloc;
// channel metadata maps (per setup)
extern std::map<unsigned, std::vector<float>> chn_angle;
extern std::map<unsigned, std::vector<float>> chn_xsf;
extern std::map<unsigned, std::vector<float>> chn_ysf;
extern std::map<unsigned, std::vector<channel_id>> chn_id;

#endif
