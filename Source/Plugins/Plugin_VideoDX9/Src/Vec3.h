// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _VEC3_H
#define _VEC3_H

#include <stdlib.h>
#include <math.h>

class Vec3
{
public:
	float x,y,z;
	Vec3() { }
	explicit Vec3(float f) {x=y=z=f;}
	explicit Vec3(const float *f) {x=f[0]; y=f[1]; z=f[2];}
	Vec3(const float _x, const float _y, const float _z) {
		x=_x; y=_y; z=_z;
	}
	void set(const float _x, const float _y, const float _z) {
		x=_x; y=_y; z=_z;
	}
	Vec3 operator + (const Vec3 &other) const {
		return Vec3(x+other.x, y+other.y, z+other.z);
	}
	void operator += (const Vec3 &other) {
		x+=other.x; y+=other.y; z+=other.z;
	}
	Vec3 operator -(const Vec3 &v) const {
		return Vec3(x-v.x,y-v.y,z-v.z);
	}
	void operator -= (const Vec3 &other)
	{
		x-=other.x; y-=other.y; z-=other.z;
	}
	Vec3 operator -() const {
		return Vec3(-x,-y,-z);
	}

	Vec3 operator * (const float f) const {
		return Vec3(x*f,y*f,z*f);
	}
	Vec3 operator / (const float f) const {
		float invf = (1.0f/f);
		return Vec3(x*invf,y*invf,z*invf);
	}
	void operator /= (const float f)
	{
		*this = *this / f;
	}
	float operator * (const Vec3 &other) const {
		return x*other.x + y*other.y + z*other.z;
	}
	void operator *= (const float f) {
		*this = *this * f;
	}
	Vec3 scaled_by(const Vec3 &other) const {
		return Vec3(x*other.x, y*other.y, z*other.z);
	}

	Vec3 operator %(const Vec3 &v) const {
		return Vec3(y*v.z-z*v.y, z*v.x-x*v.z, x*v.y-y*v.x);
	}  
	float length2() const {
		return x*x+y*y+z*z;
	}
	float length() const {
		return sqrtf(length2());
	}
	float distance2_to(Vec3 &other)
	{
		return (other-(*this)).length2();
	}
	Vec3 normalized() const {
		return (*this) / length();

	}
	void normalize() {
		(*this) /= length();
	}
	float &operator [] (int i)
	{
		return *((&x) + i);
	}
	const float operator [] (const int i) const
	{
		return *((&x) + i);
	}
	bool operator == (const Vec3 &other) const 
	{
		if (x==other.x && y==other.y && z==other.z)
			return true;
		else 
			return false;
	}
	void setZero()
	{
		memset((void *)this,0,sizeof(float)*3);
	}
};

#endif