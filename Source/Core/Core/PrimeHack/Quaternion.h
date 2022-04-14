#pragma once

#include "Core/PrimeHack/Transform.h"

namespace prime {
struct quat {
  float x, y, z, w;
  quat(float x = 0, float y = 0, float z = 0, float w = 1.f);
  quat(vec3 const& axis, float angle) { build_aa(axis, angle); }
  quat(u32 address) { read_from(address); }

  void build_pyr(float pitch, float yaw, float roll);
  vec3 axis() const { return vec3(x, y, z); }

  quat& operator*=(quat const& rhs);
  void rotate_x(float t);
  void rotate_y(float t);
  void rotate_z(float t);

  void build_aa(vec3 const& axis, float angle);
  void read_from(u32 address);
  void write_to(u32 address);
};
}
