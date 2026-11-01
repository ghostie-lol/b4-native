#pragma once
#include <cmath>

namespace b4 {

constexpr float kDeg2Rad = 0.01745329251994329577f;

struct Color { float r, g, b; };
struct Vec3 { float x, y, z; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };
struct Mat4 { float m[16]; };  // column-major

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }

template <class T> inline T Clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Clamp01(float v) { return Clamp(v, 0.0f, 1.0f); }
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Vec3 Lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
inline float SmoothStep(float t) { t = Clamp01(t); return t * t * (3.0f - 2.0f * t); }

inline Quat operator*(Quat a, Quat b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
          a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
          a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Quat Conjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }
inline Quat Normalize(Quat q) {
  const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (n < 1e-8f) return Quat{};
  const float i = 1.0f / n;
  return {q.x * i, q.y * i, q.z * i, q.w * i};
}
inline Vec3 Rotate(Quat q, Vec3 v) {
  const Vec3 u{q.x, q.y, q.z};
  const Vec3 t = Cross(u, v) * 2.0f;
  return v + t * q.w + Cross(u, t);
}
inline Quat AxisAngle(Vec3 axis, float rad) {
  const float l = Length(axis);
  if (l < 1e-8f) return Quat{};
  const float s = std::sin(rad * 0.5f) / l;
  return {axis.x * s, axis.y * s, axis.z * s, std::cos(rad * 0.5f)};
}
// Unity convention: rotate about Z, then X, then Y.
inline Quat EulerDeg(Vec3 d) {
  const Quat qx = AxisAngle({1, 0, 0}, d.x * kDeg2Rad);
  const Quat qy = AxisAngle({0, 1, 0}, d.y * kDeg2Rad);
  const Quat qz = AxisAngle({0, 0, 1}, d.z * kDeg2Rad);
  return qy * qx * qz;
}
inline Mat4 TRS(Vec3 t, Quat q, float s) {
  const Vec3 ax = Rotate(q, {1, 0, 0}) * s, ay = Rotate(q, {0, 1, 0}) * s, az = Rotate(q, {0, 0, 1}) * s;
  return {{ax.x, ax.y, ax.z, 0, ay.x, ay.y, ay.z, 0, az.x, az.y, az.z, 0, t.x, t.y, t.z, 1}};
}

}  // namespace b4
