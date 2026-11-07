#include "watchanimationcontroller.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>
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
    const auto neutral = scene.neutralGyro();
    const auto tilted = scene.gyroTarget(1, -1, 1.25);
    const auto floor = scene.projectPlane(0, true, neutral, 0, 0, identity);
    for (double height : levels) {
        const auto compensated = scene.projectPlane(height, true, neutral, 0, 0, identity);
        require(std::abs(compensated.determinant()) > 0.01, "Projection must remain invertible for Qt curve rendering and hit testing");
        for (const auto &p : points) {
            require((floor.map(p) - compensated.map(p)).length() < 0.002,
                    "Rest-view depth compensation changed the authored layout");
        }
    }
    const auto front = scene.projectPlane(50, false, neutral, 0, 0, identity).map(points[0]);
    const auto back = scene.projectPlane(-130, false, neutral, 0, 0, identity).map(points[0]);
    require(std::abs(front.y()-back.y()) > 50, "The dial planes lost their visible depth separation");
    const auto frontTilt = scene.projectPlane(72, true, tilted, 0, 0, identity).map(points[2]);
    const auto backTilt = scene.projectPlane(0, true, tilted, 0, 0, identity).map(points[2]);
    require((frontTilt-backTilt).length() > 3, "Mouse parallax must separate the compensated layers");
    for (int phase : {1, 2}) {
        const double duration = phase == 1 ? scene.openingDuration() : scene.closingDuration();
        for (int step = 0; step <= 60; ++step) {
            const double t = duration*step/60;
            for (double height : levels) {
                const auto matrix = scene.projectPlane(height, false, tilted, phase, t, identity);
                require(std::abs(matrix.determinant()) > 0.001, "An animation frame made the visual transform singular");
                for (const auto &p : points) {
                    const auto q = matrix.map(p);
                    require(std::isfinite(q.x()) && std::isfinite(q.y()), "An animation frame produced invalid screen coordinates");
                }
            }
        }
    }
    const auto settled = scene.projectPlane(0, false, neutral, 1, scene.openingDuration(), identity);
    require(settled.map(points[2]).distanceToPoint(floor.map(points[2])) < 0.002,
            "The opening clip jumps when switching to its settled pose");
    QFile source(":/assets/watch/scene.json");
    require(source.open(QIODevice::ReadOnly), "Missing source scene fixture");
    const auto nodes = QJsonDocument::fromJson(source.readAll()).object().value("nodes").toArray();
    const QString prefix = "WatchPanel_PC/Content/WatchNode/canvas_watch/";
    const auto idFor = [&](const QString &path) {
        for (const auto &value : nodes) {
            const auto node = value.toObject();
            if (node.value("path").toString() == prefix + path) return node.value("id").toString();
        }
        return QString();
    };
    const auto ringID = idFor("MiddleDecoNode/OutRingNode/OutRing");
    const auto leftID = idFor("MiddleDecoNode/DecoOut/Left");
    const auto rightID = idFor("MiddleDecoNode/DecoOut/Right");
    const auto storageID = idFor("HudBgShdow/TechtreeNode/TechtreeBtn/Icon");
    const auto reportID = idFor("HudBgShdow/ReportNode/ReportBtn/Icon");
    for (const auto &id : {ringID, leftID, rightID, storageID, reportID}) {
        require(!id.isEmpty(), "Missing authored surface node");
        const auto matrix = scene.projectSourceNode(id, 0, 0, scene.neutralGyro());
        require(std::abs(matrix.determinant()) > 1e-5, "Original surface is not invertible");
        const QVector3D p(42, 63, 0);
        require((matrix.inverted().map(matrix.map(p)) - p).length() < .002,
                "Original surface pointer mapping differs from its drawing");
        require((scene.projectSourceNode(id, 1, scene.openingDuration(), scene.neutralGyro()).map(p)
                - matrix.map(p)).length() < .002, "Source surface jumps after opening");
        for (int phase : {1, 2}) for (int step = 0; step <= 60; ++step) {
            const double duration = phase == 1 ? scene.openingDuration() : scene.closingDuration();
            const auto q = scene.projectSourceNode(id, phase, duration * step / 60, scene.gyroTarget(.8, -.6)).map(p);
            require(std::isfinite(q.x()) && std::isfinite(q.y()), "Invalid source animation projection");
        }
    }
    // Independent settled reference: original Content TRS, canvas Z=-200,
    // WorldUIRoot Z=30, FOV=15.3817997, reference canvas=2400x1350.
    const float half = float(std::tan(double(qDegreesToRadians(15.381799697875977f * .5f)))) * 30.f;
    const float unit = (half + half) / 1350.f;
    const QQuaternion content(.9812970161437988f, .19096055626869202f, .003568660467863083f, .024030504748225212f);
    const auto expected = [&](QVector3D p) {
        p = (content.normalized().rotatedVector(p) + QVector3D(30, -13, -296)) * unit + QVector3D(0, 0, 30);
        const double focal = 540.0 / std::tan(qDegreesToRadians(15.381799697875977) / 2);
        return QVector3D(960 + focal * p.x() / p.z(), 540 - focal * p.y() / p.z(), 0);
    };
    const auto ring = scene.projectSourceNode(ringID, 0, 0, scene.neutralGyro());
    auto center = ring.map(QVector3D(512, 512, 0)); center.setZ(0);
    require((center - expected({0,0,-200})).length() < .003, "Source camera or canvas initialization differs from EndfieldHUD");
    auto stageCenter = floor.map(QVector3D(960,540,0)); stageCenter.setZ(0);
    require((center - stageCenter).length() < .003, "Imported components and original sprites use different cameras");
    const auto left = scene.projectSourceNode(leftID, 0, 0, scene.neutralGyro());
    const auto right = scene.projectSourceNode(rightID, 0, 0, scene.neutralGyro());
    require(left.map(QVector3D(174,232.5,0)).x() < left.map(QVector3D(0,232.5,0)).x(), "Left curved frame lost its original mirror");
    require(right.map(QVector3D(174,232.5,0)).x() > right.map(QVector3D(0,232.5,0)).x(), "Right curved frame is incorrectly mirrored");
    std::cout << "Projection, depth compensation, parallax and 122 animation samples passed.\n";
    std::cout << "Original camera, outer-ring centre, mirrored frames, bottom hit mapping and 610 source animation samples passed.\n";
}
