#include "Core/PrimeHack/Transform.h"
#include "Core/PrimeHack/PrimeUtils.h"
#include "Core/PowerPC/PowerPC.h"

#include <cstring>
#include <cmath>

namespace prime {

void vec3::read_from(u32 address) {
  for (int i = 0; i < 3; i++) {
    arr[i] = readf32(address + i * 4);
  }
}

void vec3::write_to(u32 address) {
  for (int i = 0; i < 3; i++) {
    writef32(arr[i], address + i * 4);
  }
}

Transform::Transform() {
  memset(m, 0, sizeof(Transform::m));
}

Transform& Transform::operator=(Transform const& rhs) {
  memcpy(m, rhs.m, sizeof(Transform::m));
  return *this;
}

vec3 Transform::operator*(vec3 const& rhs) const {
  // extend rhs to a homog vec4
  return vec3(m[0][0] * rhs.x + m[0][1] * rhs.y + m[0][2] * rhs.z + m[0][3],
              m[1][0] * rhs.x + m[1][1] * rhs.y + m[1][2] * rhs.z + m[1][3],
              m[2][0] * rhs.x + m[2][1] * rhs.y + m[2][2] * rhs.z + m[2][3]);
}

Transform Transform::operator*(Transform const& rhs) const {
  Transform ret;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      ret.m[i][j] = m[i][0] * rhs.m[0][j] +
                    m[i][1] * rhs.m[1][j] +
                    m[i][2] * rhs.m[2][j];
    }
  }
  // almost like it's a 4x4 :)
  ret.set_loc(loc() + rhs.loc());
  return ret;
}

Transform& Transform::operator*=(Transform const& rhs) {
  *this = *this * rhs;
  return *this;
}

void Transform::build_rotation(float yaw) {
  yaw -= (3.141592654f / 2.f);
  const float sy = sin(yaw), cy = cos(yaw);
  
  m[0][0] = cy;
  m[1][0] = sy;
  m[2][0] = 0;
  
  m[0][1] = -sy;
  m[1][1] = cy;
  m[2][1] = 0;
  
  m[0][2] = 0;
  m[1][2] = 0;
  m[2][2] = 1.f;
}

void Transform::read_from(u32 address) {
  for (int i = 0; i < sizeof(Transform) / 4; i++) {
    m[i / 4][i % 4] = readf32(address + i * 4);
  }
}

void Transform::write_to(u32 address) {
  for (int i = 0; i < sizeof(Transform) / 4; i++) {
    writef32(m[i / 4][i % 4], address + i * 4);
  }
}
}
