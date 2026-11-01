#pragma once
#include <cstdint>

namespace b4 {

constexpr int kGlyphW = 5, kGlyphH = 7, kGlyphAdvance = 6;

// 7 rows, bit 4 = leftmost pixel. Lower case maps to upper case; '\x01' is a star.
const uint8_t* GlyphRows(char c);

// Width of a string in font pixels (no trailing gap).
int TextPixelWidth(const char* s);

}  // namespace b4
