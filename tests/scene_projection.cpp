#include "watchanimationcontroller.hpp"

#include <QCoreApplication>
#include <QVector3D>
#include <cmath>
#include <cstdlib>
#include <iostream>

static void require(bool condition, const char *message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    WatchAnimationController scene;
    const QMatrix4x4 identity;
    const QVector3D points[] = {{960,540,0}, {271,56,0}, {1306,58,0}, {289,850,0}, {1750,950,0}};
    const double levels[] = {-130, -112, -60, -37.7, 0, 10, 22, 26, 28, 34, 50, 64, 72, 144};
    const auto floor = scene.projectPlane(0, true, 20, 0, 0, 0, identity);
    for (double height : levels) {
        const auto compensated = scene.projectPlane(height, true, 20, 0, 0, 0, identity);
        require(std::abs(compensated.determinant()) > 0.01, "Projection must remain invertible for Qt curve rendering and hit testing");
        for (const auto &p : points) {
            require((floor.map(p) - compensated.map(p)).length() < 0.002,
                    "Rest-view depth compensation changed the authored layout");
        }
    }
    const auto front = scene.projectPlane(50, false, 20, 0, 0, 0, identity).map(points[0]);
    const auto back = scene.projectPlane(-130, false, 20, 0, 0, 0, identity).map(points[0]);
    require(std::abs(front.y()-back.y()) > 50, "The dial planes lost their visible depth separation");
    const auto frontTilt = scene.projectPlane(72, true, 22.5, -4, 0, 0, identity).map(points[2]);
    const auto backTilt = scene.projectPlane(0, true, 22.5, -4, 0, 0, identity).map(points[2]);
    require((frontTilt-backTilt).length() > 3, "Mouse parallax must separate the compensated layers");
    for (int phase : {1, 2}) {
        const double duration = phase == 1 ? scene.openingDuration() : scene.closingDuration();
        for (int step = 0; step <= 60; ++step) {
            const double t = duration*step/60;
            for (double height : levels) {
                const auto matrix = scene.projectPlane(height, false, 22.5, -4, phase, t, identity);
                require(std::abs(matrix.determinant()) > 0.001, "An animation frame made the visual transform singular");
                for (const auto &p : points) {
                    const auto q = matrix.map(p);
                    require(std::isfinite(q.x()) && std::isfinite(q.y()), "An animation frame produced invalid screen coordinates");
                }
            }
        }
    }
    const auto settled = scene.projectPlane(0, false, 20, 0, 1, scene.openingDuration(), identity);
    require(settled.map(points[2]).distanceToPoint(floor.map(points[2])) < 0.002,
            "The opening clip jumps when switching to its settled pose");
    std::cout << "Projection, depth compensation, parallax and 122 animation samples passed.\n";
}
