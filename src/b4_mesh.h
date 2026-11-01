#pragma once
#include <atomic>
#include <cmath>
#include <cstring>
#include <cstdint>

#include "b4_font.h"
#include "b4_math.h"

namespace b4 {

constexpr int kMaxVerts = 60000;    // must stay < 65536 (16-bit indices)
constexpr int kMaxIndices = 240000;

struct Vertex {
  float px, py, pz;  // position (menu space)
  float nx, ny, nz;  // normal
  float r, g, b;     // colour (linear if the builder was told so)
  float emis;        // 0 = lit, 1 = fully emissive
};

// Appends triangles to caller-owned arrays. Front faces: the raw cross product (p1-p0)x(p2-p0)
// points along the outward normal (Unity's winding for left-handed space).
class MeshBuilder {
 public:
  MeshBuilder(Vertex* v, int maxV, uint16_t* idx, int maxI, bool linear)
      : v_(v), idx_(idx), maxV_(maxV), maxI_(maxI), linear_(linear) {}

  int VertexCount() const { return nv_; }
  int IndexCount() const { return ni_; }
  bool Overflowed() const { return overflow_; }

  void AddBox(Vec3 mn, Vec3 mx, Color c, float emis) {
    AddOrientedBox((mn + mx) * 0.5f, (mx - mn) * 0.5f, Quat{}, c, emis);
  }

  void AddOrientedBox(Vec3 center, Vec3 half, Quat rot, Color c, float emis) {
    const Vec3 ax[3] = {Rotate(rot, {1, 0, 0}), Rotate(rot, {0, 1, 0}), Rotate(rot, {0, 0, 1})};
    const float h[3] = {half.x, half.y, half.z};
    for (int a = 0; a < 3; ++a) {
      const int b = (a + 1) % 3, d = (a + 2) % 3;
      for (int s = -1; s <= 1; s += 2) {
        const Vec3 n = ax[a] * static_cast<float>(s);
        const Vec3 fc = center + n * h[a];
        const Vec3 u = ax[b] * h[b], w = ax[d] * h[d];
        AddQuad(fc - u - w, fc + u - w, fc + u + w, fc - u + w, n, c, emis);
      }
    }
  }

  // Centred on (cx, cy) at depth z, facing -Z (towards the viewer). px = size of one font pixel.
  void AddText(const char* text, float cx, float cy, float z, float px, Color c, float emis) {
    if (!text || !text[0] || px <= 0.0f) return;
    const int n = static_cast<int>(std::strlen(text));
    const float totalW = static_cast<float>(TextPixelWidth(text)) * px;
    const float xStart = cx - totalW * 0.5f;
    const float yTop = cy + static_cast<float>(kGlyphH) * px * 0.5f;
    for (int i = 0; i < n; ++i) {
      const uint8_t* rows = GlyphRows(text[i]);
      const float gx = xStart + static_cast<float>(i * kGlyphAdvance) * px;
      for (int r = 0; r < kGlyphH; ++r) {
        int col = 0;
        while (col < kGlyphW) {
          if (!(rows[r] & (1 << (kGlyphW - 1 - col)))) { ++col; continue; }
          int end = col;
          while (end < kGlyphW && (rows[r] & (1 << (kGlyphW - 1 - end)))) ++end;
          const float x0 = gx + static_cast<float>(col) * px, x1 = gx + static_cast<float>(end) * px;
          const float y1 = yTop - static_cast<float>(r) * px, y0 = y1 - px;
          AddQuad({x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}, {0, 0, -1}, c, emis);
          col = end;
        }
      }
    }
  }

  // Radius-1 sphere at the origin (scale it with the model matrix).
  void AddUnitSphere(int lon, int lat, Color c, float emis) {
    if (lon < 3) lon = 3;
    if (lat < 2) lat = 2;
    const float pi = 3.14159265358979f;
    for (int j = 0; j < lat; ++j) {
      const float t0 = pi * static_cast<float>(j) / static_cast<float>(lat);
      const float t1 = pi * static_cast<float>(j + 1) / static_cast<float>(lat);
      for (int i = 0; i < lon; ++i) {
        const float p0 = 2.0f * pi * static_cast<float>(i) / static_cast<float>(lon);
        const float p1 = 2.0f * pi * static_cast<float>(i + 1) / static_cast<float>(lon);
        const Vec3 a = Sph(t0, p0), b = Sph(t0, p1), d = Sph(t1, p1), e = Sph(t1, p0);
        AddQuad(a, b, d, e, Normalized(a + b + d + e), c, emis);
      }
    }
  }

 private:
  static Vec3 Sph(float theta, float phi) {
    return {std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
  }
  static Vec3 Normalized(Vec3 v) {
    const float l = Length(v);
    return l < 1e-8f ? Vec3{0, 1, 0} : v * (1.0f / l);
  }
  float Lin(float v) const { return linear_ ? std::pow(v < 0 ? 0 : v, 2.2f) : v; }

  void AddQuad(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n, Color c, float emis) {
    if (nv_ + 4 > maxV_ || ni_ + 6 > maxI_) { overflow_ = true; return; }
    if (Dot(Cross(p1 - p0, p2 - p0), n) < 0.0f) { const Vec3 t = p1; p1 = p3; p3 = t; }  // fix winding
    const Vec3 pts[4] = {p0, p1, p2, p3};
    const int base = nv_;
    for (int k = 0; k < 4; ++k)
      v_[nv_++] = Vertex{pts[k].x, pts[k].y, pts[k].z, n.x, n.y, n.z, Lin(c.r), Lin(c.g), Lin(c.b), emis};
    const int tri[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; ++k) idx_[ni_++] = static_cast<uint16_t>(base + tri[k]);
  }

  Vertex* v_;
  uint16_t* idx_;
  int maxV_, maxI_;
  bool linear_;
  int nv_ = 0, ni_ = 0;
  bool overflow_ = false;
};

// Filled by Menu::BuildPacket on one thread, drawn by the renderer on another (see `valid`).
struct RenderPacket {
  std::atomic<int> valid{0};
  Vertex verts[kMaxVerts];
  uint16_t indices[kMaxIndices];
  int vertCount = 0, indexCount = 0;
  int menuFirstIndex = 0, menuIndexCount = 0;
  int pointerFirstIndex = 0, pointerIndexCount = 0;
  Mat4 menuModel{}, pointerModel{};
  float viewProj[32] = {};  // two 4x4 matrices (left eye, right eye)
  int width = 0, height = 0, msaa = 1, reversedZ = 0;
};

}  // namespace b4
