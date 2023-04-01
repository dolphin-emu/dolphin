// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cmath>

#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif

typedef double Matrix[4][4];
struct Vertex
{
	double x, y, z;
};

inline void MatrixIdentity(Matrix & m)
{
	m[0][0]=1; m[0][1]=0; m[0][2]=0; m[0][3]=0;
	m[1][0]=0; m[1][1]=1; m[1][2]=0; m[1][3]=0;
	m[2][0]=0; m[2][1]=0; m[2][2]=1; m[2][3]=0;
	m[3][0]=0; m[3][1]=0; m[3][2]=0; m[3][3]=1;
}

inline void MatrixFrustum(Matrix &m, double l, double r, double b, double t, double n, double f)
{
	m[0][0]=2*n/(r-l);    m[0][1]=0;            m[0][2]=0;            m[0][3]=0;
	m[1][0]=0;            m[1][1]=2*n/(t-b);    m[1][2]=0;            m[1][3]=0;
	m[2][0]=(r+l)/(r-l);  m[2][1]=(t+b)/(t-b);  m[2][2]=(f+n)/(f-n);  m[2][3]=-1;
	m[3][0]=0;            m[3][1]=0;            m[3][2]=2*f*n/(f-n);  m[3][3]=0;
}

inline void MatrixPerspective(Matrix & m, double fovy, double aspect, double nplane, double fplane)
{
	double xmin, xmax, ymin, ymax;

	ymax = nplane * tan(fovy * M_PI / 360.0);
	ymin = -ymax;
	xmin = ymin * aspect;
	xmax = ymax * aspect;

	MatrixFrustum(m,xmin,xmax,ymin,ymax,nplane,fplane);
}

inline void MatrixRotationByZ(Matrix &m, double sin, double cos)
{
	m[0][0]=cos; m[0][1]=-sin; m[0][2]=0; m[0][3]=0;
	m[1][0]=sin; m[1][1]=cos;  m[1][2]=0; m[1][3]=0;
	m[2][0]=0;   m[2][1]=0;    m[2][2]=1; m[2][3]=0;
	m[3][0]=0;   m[3][1]=0;    m[3][2]=0; m[3][3]=1;
}

inline void MatrixScale(Matrix &m, double xfact, double yfact, double zfact)
{
	m[0][0]=xfact; m[0][1]=0;     m[0][2]=0;     m[0][3]=0;
	m[1][0]=0;     m[1][1]=yfact; m[1][2]=0;     m[1][3]=0;
	m[2][0]=0;     m[2][1]=0;     m[2][2]=zfact; m[2][3]=0;
	m[3][0]=0;     m[3][1]=0;     m[3][2]=0;     m[3][3]=1;
}

inline void MatrixMultiply(Matrix &r, const Matrix &a, const Matrix &b)
{
	for (int i=0; i<16; i++)
		r[i>>2][i&3]=0.0f;

	for (int i=0; i<4; i++)
		for (int j=0; j<4; j++)
			for (int k=0; k<4; k++)
				r[i][j]+=a[i][k]*b[k][j];
}

inline void MatrixTransformVertex(Matrix const & m, Vertex & v)
{
	Vertex ov;
	double w;
	ov.x=v.x;
	ov.y=v.y;
	ov.z=v.z;
	v.x = m[0][0] * ov.x + m[0][1] * ov.y + m[0][2] * ov.z + m[0][3];
	v.y = m[1][0] * ov.x + m[1][1] * ov.y + m[1][2] * ov.z + m[1][3];
	v.z = m[2][0] * ov.x + m[2][1] * ov.y + m[2][2] * ov.z + m[2][3];
	w   = m[3][0] * ov.x + m[3][1] * ov.y + m[3][2] * ov.z + m[3][3];
	if (w!=0)
	{
		v.x/=w;
		v.y/=w;
		v.z/=w;
	}
}
