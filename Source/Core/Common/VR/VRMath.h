#pragma once

#include <math.h>
#include <openxr.h>

#ifndef EPSILON
#define EPSILON 0.001f
#endif

float ToDegrees(float rad);
float ToRadians(float deg);
bool IsMatrixIdentity(float* matrix);

// XrPosef
XrPosef XrPosef_Identity();
XrPosef XrPosef_Inverse(const XrPosef a);

// XrQuaternionf
XrQuaternionf XrQuaternionf_CreateFromVectorAngle(const XrVector3f axis, const float angle);
XrQuaternionf XrQuaternionf_Inverse(const XrQuaternionf q);
XrQuaternionf XrQuaternionf_Multiply(const XrQuaternionf a, const XrQuaternionf b);
XrVector3f XrQuaternionf_Rotate(const XrQuaternionf a, const XrVector3f v);
XrVector3f XrQuaternionf_ToEulerAngles(const XrQuaternionf q);
void XrQuaternionf_ToMatrix4f(const XrQuaternionf* q, float* m);

// XrVector3f, XrVector4f
float XrVector3f_LengthSquared(const XrVector3f v);
XrVector3f XrVector3f_GetAnglesFromVectors(const XrVector3f forward, const XrVector3f right, const XrVector3f up);
XrVector3f XrVector3f_Normalized(const XrVector3f v);
XrVector3f XrVector3f_ScalarMultiply(const XrVector3f v, float scale);
XrVector4f XrVector4f_MultiplyMatrix4f(const float* m, const XrVector4f* v);
