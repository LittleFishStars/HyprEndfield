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
    for (int phase : {1, 2}) {
        const double length = phase == 1 ? scene.openingDuration() : scene.closingDuration();
        require(std::abs(scene.clipTime(phase, length / 2) - length * .75) < 1e-10,
                "Finite clip playback omitted the original OutQuad wrapper");
        require(scene.clipTime(phase, -1) == 0 && scene.clipTime(phase, length + 1) == length,
                "Finite wrapper clock must clamp to the original clip endpoints");
    }
    const auto tilted = scene.gyroTarget(1, -1, 1.25);
    const auto floor = scene.projectPlane(0, true, neutral, 0, 0, identity);
    for (double height : levels) {
        const auto compensated = scene.projectPlane(height, true, neutral, 0, 0, identity);
        require(compensated(2, 2) == 1,
                "Unused Z scale must not change Qt curve text antialiasing");
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
                require(matrix(2, 2) == 1,
                        "Animation reintroduced camera depth into curve antialiasing");
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
    const auto meshID = idFor("MiddleDecoNode/MeshNode/RingFoMesh");
    const auto meshProjection = scene.projectSourceGeometry(meshID,0,0,neutral,-1);
    const auto elevatedVertex = meshProjection.map(QVector3D(-.04838806f,-.6761424f,.13230038f));
    const auto flattenedVertex = meshProjection.map(QVector3D(-.04838806f,-.6761424f,0));
    require((elevatedVertex-flattenedVertex).length()>5,
            "The original Equipring mesh was flattened and lost its vertex depth");
    for (const auto &value:nodes) {
        const auto node=value.toObject();
        if (!node.value("path").toString().contains("/MiddleDecoNode/")) continue;
        const auto id=node.value("id").toString();
        for(int phase:{1,2}) for(int step=0;step<=30;++step) {
            const double t=(phase==1?scene.openingDuration():scene.closingDuration())*step/30;
            const auto c=scene.sourceGraphicColor(id,phase,t,false);
            for(int i=0;i<4;++i) require(std::isfinite(c[i]) && c[i]>=0 && c[i]<=1,
                "An original dial colour/CanvasGroup curve produced invalid opacity");
        }
    }
    const auto mainID=idFor("MiddleDecoNode/MainRingNode/MainRing");
    const auto sourceRect=scene.projectSourceNode(mainID,0,0,neutral);
    const auto sourceGeometry=scene.projectSourceGeometry(mainID,0,0,neutral,-1);
    require((sourceRect.map(QVector3D(0,0,0)).toVector2D()
            - sourceGeometry.map(QVector3D(-512,512,0)).toVector2D()).length()<.003,
            "The original mesh renderer and interactive plane camera disagree");
    // The exported DecoOut rotation accelerates toward its final raw key.
    // The source wrapper must slow the sampling clock before watch_loop starts.
    for (const auto &id : {leftID, rightID}) {
        const double end = scene.openingDuration();
        const QVector3D corner(174, 232.5, 0);
        const auto before = scene.projectSourceNode(id, 1, scene.clipTime(1, end - .01), neutral).map(corner);
        const auto endpoint = scene.projectSourceNode(id, 1, scene.clipTime(1, end), neutral).map(corner);
        const auto rawBefore = scene.projectSourceNode(id, 1, end - .01, neutral).map(corner);
        require((before - endpoint).length() < (rawBefore - endpoint).length() * .08,
                "Curved frames still accelerate abruptly at the entrance endpoint");
        require((scene.projectSourceNode(id, 0, 0, neutral, 0).map(corner) - endpoint).length() < .003,
                "Curved frames jump when the ambient loop begins");
    }
    for (const auto &id : {ringID, leftID, rightID, storageID, reportID}) {
        require(!id.isEmpty(), "Missing authored surface node");
        const auto matrix = scene.projectSourceNode(id, 0, 0, scene.neutralGyro());
        require(matrix(2, 2) == 1,
                "Source label projection must keep the unused Z scale normalized");
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

    QFile clipsFile(":/assets/watch/clips.json");
    require(clipsFile.open(QIODevice::ReadOnly), "Missing source loop fixture");
    QJsonObject loop;
    for (const auto &v : QJsonDocument::fromJson(clipsFile.readAll()).object().value("clips").toArray())
        if (v.toObject().value("binding").toString() == "_animationLoop") loop = v.toObject();
    const double loopDuration = loop.value("last_key_time").toDouble();
    require(loopDuration > 0, "Original watch_loop was not found");
    // Independent SplitMix64 fixture from HUDSourceDesktopAmbientMotion.
    // Cover both directions, different parent speeds and reopening seeds.
    const quint64 seeds[] = {Q_UINT64_C(0x454e444649454c45), Q_UINT64_C(0x454e444649454c46)};
    const double rates[][2] = {{.9442545409639657, -1.0098051785812545},
                              {-.9835934303242009, -1.1565648057489737}};
    const QString parents[] = {"MiddleDecoNode/OutRingNode", "MiddleDecoNode/DecoOut"};
    int ambientSamples = 0;
    for (int seed = 0; seed < 2; ++seed) {
        scene.restartAmbientMotion(seeds[seed]);
        for (int channel = 0; channel < 2; ++channel) {
            const auto id = idFor(parents[channel]);
            const auto rest = scene.projectSourceNode(id, 0, 0, neutral);
            const double rate = rates[seed][channel];
            QJsonArray keys;
            for (const auto &v : loop.value("curves").toArray())
                if (v.toObject().value("path").toString() == prefix.mid(14) + parents[channel])
                    keys = v.toObject().value("raw").toObject().value("curve").toObject().value("m_Curve").toArray();
            require(keys.size() == 821, "Missing exact source decorative rotation keys");
            const QVector3D point(600, 30, 0);
            for (const auto &v : keys) {
                const auto key = v.toObject();
                const double t = key.value("time").toDouble();
                const double elapsed = rate > 0 ? t / rate : (loopDuration - t) / -rate;
                const auto q = key.value("value").toObject();
                QMatrix4x4 spin;
                spin.rotate(QQuaternion(float(q.value("w").toDouble()), 0, 0, -float(q.value("z").toDouble())).normalized());
                const auto actual = scene.projectSourceNode(id, 0, 0, neutral, elapsed);
                require((actual.map(point) - rest.map(spin.map(point))).length() < .005,
                        "Ambient rotation differs from original keys, speed or playback direction");
                require((actual.map(QVector3D()) - rest.map(QVector3D())).length() < .003,
                        "Ambient rotation changed the original parent pivot");
                ++ambientSamples;
            }
            const auto moving = scene.projectSourceNode(id, 0, 0, neutral, 3);
            const auto repeated = scene.projectSourceNode(id, 0, 0, neutral, 3 + loopDuration / std::abs(rate));
            require((moving.map(point) - repeated.map(point)).length() < .003, "Ambient cycle failed to repeat");
            require((moving.map(point) - rest.map(point)).length() > 10, "Decorations remained stationary");
            require((scene.projectSourceNode(id, 1, .25, neutral, 3).map(point)
                     - scene.projectSourceNode(id, 1, .25, neutral).map(point)).length() < .003,
                    "Ambient motion started before the dial entrance finished");
            require((scene.projectSourceNode(id, 2, 0, neutral, 3).map(point) - moving.map(point)).length() < .003,
                    "Closing reset the ambient rotation");
            const auto children = channel == 0 ? QList<QString>{idFor("MiddleDecoNode/OutRingNode/Dot")}
                                              : QList<QString>{leftID, rightID};
            for (const auto &child : children) {
                const auto childRest = scene.projectSourceNode(child, 0, 0, neutral);
                const auto childMoving = scene.projectSourceNode(child, 0, 0, neutral, 3);
                const auto inherited = moving * rest.inverted() * childRest;
                require((childMoving.map(points[0]) - inherited.map(points[0])).length() < .02,
                        "A dot or curved frame does not inherit its original parent rotation");
            }
        }
    }
    require((scene.projectSourceNode(storageID, 0, 0, neutral, 4).map(points[0])
             - scene.projectSourceNode(storageID, 0, 0, neutral).map(points[0])).length() < .003,
            "Decorative animation moved an unrelated bottom button");
    std::cout << "Projection, depth compensation, parallax and 122 animation samples passed.\n";
    std::cout << "Original camera, outer-ring centre, mirrored frames, bottom hit mapping and 610 source animation samples passed.\n";
    std::cout << ambientSamples << " original ambient keys, seeded speeds, reverse loops, inherited rotations and transition checks passed.\n";
}
