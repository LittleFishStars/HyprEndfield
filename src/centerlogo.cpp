#include "centerlogo.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

QImage centerLogoArtwork(const QImage &source) {
    const auto image = source.convertToFormat(QImage::Format_RGBA8888);
    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.constScanLine(y)[x * 4 + 3] > 8) {
                left = std::min(left, x); right = std::max(right, x);
                top = std::min(top, y); bottom = std::max(bottom, y);
            }
    if (right < left || bottom < top) return {};
    const int w = right - left + 1, h = bottom - top + 1;
    const size_t count = size_t(w) * h;
    std::vector<double> alpha(count), horizontal(count), shadow(count);
    std::vector<std::array<double, 3>> premultiplied(count);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const auto *p = image.constScanLine(y + top) + (x + left) * 4;
        const size_t i = size_t(y) * w + x;
        alpha[i] = p[3] / 255.;
        for (int c = 0; c < 3; ++c) premultiplied[i][c] = std::round(p[c] * alpha[i]) / 255.;
    }
    const double blur = h * 3.5 / 65., sigma = std::sqrt(blur * blur / 4 + .5);
    const int radius = int(std::ceil(3 * sigma));
    std::vector<double> kernel(size_t(radius) * 2 + 1);
    double total = 0;
    for (int k = -radius; k <= radius; ++k) {
        const double v = std::exp(-double(k) * k / (2 * sigma * sigma));
        kernel[size_t(k + radius)] = v; total += v;
    }
    for (auto &v : kernel) v /= total;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        for (int k = -radius; k <= radius; ++k)
            if (x + k >= 0 && x + k < w) horizontal[size_t(y) * w + x] += kernel[size_t(k + radius)] * alpha[size_t(y) * w + x + k];
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        for (int k = -radius; k <= radius; ++k)
            if (y + k >= 0 && y + k < h) shadow[size_t(y) * w + x] += .7 * kernel[size_t(k + radius)] * horizontal[size_t(y + k) * w + x];
    QImage result(w, h, QImage::Format_RGBA8888);
    const auto quantize = [](double v) { return unsigned(std::lround(std::clamp(v, 0., 1.) * 255)); };
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        const double a = alpha[i], s = shadow[i] * (1 - a);
        const unsigned outA = quantize(a + s);
        auto *p = result.scanLine(y) + x * 4;
        for (int c = 0; c < 3; ++c) {
            const unsigned v = std::min(quantize(premultiplied[i][c] + s), outA);
            p[c] = outA ? std::min(255u, (v * 255u + outA / 2) / outA) : 0;
        }
        p[3] = outA;
    }
    return result;
}
