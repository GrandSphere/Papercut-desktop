#pragma once

#include <algorithm>

namespace papercut {

inline constexpr float kReflowWidth = 450.f;
inline constexpr float kReflowHeight = 600.f;
inline constexpr float kSpreadsheetWidth = 1191.f;
inline constexpr float kSpreadsheetHeight = 842.f;
inline constexpr long long kMaxRenderPixels = 8000000LL;
inline constexpr unsigned kOpaque = 0xFFu << 24;
inline constexpr unsigned kOpaqueWhite = 0xFFFFFFFFu;

inline float layoutEm(float fontScale) {
    return 12.f * std::clamp(fontScale, 0.3f, 3.f);
}

} // namespace papercut
