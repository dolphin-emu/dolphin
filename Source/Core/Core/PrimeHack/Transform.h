#pragma once

#include "Common/CommonTypes.h"

namespace prime {

struct vec3 {
  vec3() : vec3(0, 0, 0) {}
  vec3(float x, float y, float z) : x(x), y(y), z(z) {}
  vec3(u32 address) { read_from(address); }

  union {
    float arr[3];
    struct {
      float x, y, z;
    };
  };

  vec3 operator*(float s) const {
    return vec3(x * s, y * s, z * s);
  }

  vec3 operator+(vec3 const& rhs) const {
    return vec3(x + rhs.x, y + rhs.y, z + rhs.z);
  }

  vec3 operator-(vec3 const& rhs) const {
    return *this + -rhs;
  }

  vec3 operator-() const {
    return vec3(-x, -y, -z);
  }

  float dot(vec3 other) {
    return x * other.x + y * other.y + z * other.z;
  }

  vec3 cross(vec3 const& rhs) const {
    return vec3(y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x);
  }

  void read_from(u32 address);
  void write_to(u32 address);
};

struct Transform {
  float m[3][4];
  Transform();
  Transform(float yaw) { build_rotation(yaw); }
  Transform(u32 address) { read_from(address); }

  vec3 right() const {
    return vec3(m[0][0], m[1][0], m[2][0]);
  }

  vec3 fwd() const {
    return vec3(m[0][1], m[1][1], m[2][1]);
  }

  vec3 up() const {
    return vec3(m[0][2], m[1][2], m[2][2]);
  }

  vec3 loc() const {
    return vec3(m[0][3], m[1][3], m[2][3]);
  }

  void set_loc(vec3 const& l) {
    m[0][3] = l.x;
    m[1][3] = l.y;
    m[2][3] = l.z;
  }

  Transform& operator=(Transform const &rhs);
  vec3 operator*(vec3 const& rhs) const;
  Transform operator*(Transform const &rhs) const;
  Transform& operator*=(Transform const &rhs);
  void build_rotation(float yaw);
  
  void read_from(u32 address);
  void write_to(u32 address);
};
}
