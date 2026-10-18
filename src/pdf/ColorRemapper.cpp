#include "ColorRemapper.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace papercut {
namespace {

const std::array<float, 256> &lumR() {
    static const auto t = [] {
        std::array<float, 256> a{};
        for (int i = 0; i < 256; ++i) a[static_cast<size_t>(i)] = 0.299f * static_cast<float>(i);
        return a;
    }();
    return t;
}
const std::array<float, 256> &lumG() {
    static const auto t = [] {
        std::array<float, 256> a{};
        for (int i = 0; i < 256; ++i) a[static_cast<size_t>(i)] = 0.587f * static_cast<float>(i);
        return a;
    }();
    return t;
}
const std::array<float, 256> &lumB() {
    static const auto t = [] {
        std::array<float, 256> a{};
        for (int i = 0; i < 256; ++i) a[static_cast<size_t>(i)] = 0.114f * static_cast<float>(i);
        return a;
    }();
    return t;
}

int roundToInt(float v) {
    return static_cast<int>(std::floor(v + 0.5f));
}

int coerce(int v, int lo, int hi) {
    return std::clamp(v, lo, hi);
}

std::vector<char> buildMask(const QVector<PdfRect> &rects, int w, int h) {
    std::vector<char> mask(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    for (const PdfRect &r : rects) {
        const int left = coerce(roundToInt(r.left), 0, w);
        const int top = coerce(roundToInt(r.top), 0, h);
        const int right = coerce(roundToInt(r.right), 0, w);
        const int bottom = coerce(roundToInt(r.bottom), 0, h);
        for (int y = top; y < bottom; ++y) {
            const int row = y * w;
            for (int x = left; x < right; ++x)
                mask[static_cast<size_t>(row + x)] = 1;
        }
    }
    return mask;
}

} // namespace

void ColorRemapper::remap(QVector<int> &pixels, int w, int h, int fg, int bg,
                          float imageBlend, const QVector<PdfRect> &imageRects,
                          float imageAlpha) {
    if (w <= 0 || h <= 0) return;

    const int fgR = (fg >> 16) & 0xFF;
    const int fgG = (fg >> 8) & 0xFF;
    const int fgB = fg & 0xFF;
    const int bgR = (bg >> 16) & 0xFF;
    const int bgG = (bg >> 8) & 0xFF;
    const int bgB = bg & 0xFF;
    const int dR = bgR - fgR;
    const int dG = bgG - fgG;
    const int dB = bgB - fgB;

    const float blend = std::clamp(imageBlend, 0.f, 1.f);
    const float alpha = std::clamp(imageAlpha, 0.f, 1.f);
    const bool doBlend = blend > 0.f;
    const bool doAlpha = alpha < 0.999f;
    std::vector<char> maskStorage;
    const char *mask = nullptr;
    if ((doBlend || doAlpha) && !imageRects.isEmpty()) {
        maskStorage = buildMask(imageRects, w, h);
        mask = maskStorage.data();
    }

    int lastRgb = -1;
    bool lastInside = false;
    int lastOut = 0;
    const auto &lr = lumR();
    const auto &lg = lumG();
    const auto &lb = lumB();
    for (int i = 0; i < pixels.size(); ++i) {
        const int rgb = pixels[i] & 0xFFFFFF;
        const bool inside = mask != nullptr && mask[static_cast<size_t>(i)] != 0;
        if (rgb == lastRgb && inside == lastInside) {
            pixels[i] = lastOut;
            continue;
        }
        const int r = rgb >> 16;
        const int g = (rgb >> 8) & 0xFF;
        const int b = rgb & 0xFF;

        const float lum = (lr[static_cast<size_t>(r)] + lg[static_cast<size_t>(g)] + lb[static_cast<size_t>(b)]) / 255.f;
        int nr = static_cast<int>(fgR + dR * lum);
        int ng = static_cast<int>(fgG + dG * lum);
        int nb = static_cast<int>(fgB + dB * lum);

        if (inside) {
            if (doBlend) {
                nr = static_cast<int>(nr + (r - nr) * blend);
                ng = static_cast<int>(ng + (g - ng) * blend);
                nb = static_cast<int>(nb + (b - nb) * blend);
            }
            if (doAlpha) {
                nr = static_cast<int>(bgR + (nr - bgR) * alpha);
                ng = static_cast<int>(bgG + (ng - bgG) * alpha);
                nb = static_cast<int>(bgB + (nb - bgB) * alpha);
            }
        }

        const int out = (0xFF << 24) | (coerce(nr, 0, 255) << 16) | (coerce(ng, 0, 255) << 8) | coerce(nb, 0, 255);
        pixels[i] = out;
        lastRgb = rgb;
        lastInside = inside;
        lastOut = out;
    }
}

} // namespace papercut
