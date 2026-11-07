#pragma once
// Geometry adapter for unmodified EndfieldHUD e9a6bdc2 media processing.
namespace endfield::core {
struct Point { double x{}, y{}; bool operator==(const Point &) const = default; };
struct Rect {
    double x{}, y{}, width{}, height{};
    bool operator==(const Rect &) const = default;
    bool contains(Point p) const noexcept { return width > 0 && height > 0 && p.x >= x && p.y >= y && p.x < x + width && p.y < y + height; }
};
}
