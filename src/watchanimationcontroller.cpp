#include "watchanimationcontroller.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QLoggingCategory>
#include <QVector>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
constexpr int OpeningPhase = 1;
constexpr int ClosingPhase = 2;
constexpr double ContentFadeStart = 0.20;
constexpr double ContentFadeDuration = 0.24;

double sourceNumber(const QJsonValue &value)
{
    if (value.isDouble()) {
        return value.toDouble();
    }
    const QString token = value.toString();
    if (token == QStringLiteral("Infinity")) {
        return std::numeric_limits<double>::infinity();
    }
    if (token == QStringLiteral("-Infinity")) {
        return -std::numeric_limits<double>::infinity();
    }
    return std::numeric_limits<double>::quiet_NaN();
}

double cubicBezier(double u, double a, double b, double c, double d)
{
    const double v = 1.0 - u;
    return v * v * v * a + 3.0 * v * v * u * b
        + 3.0 * v * u * u * c + u * u * u * d;
}

double bezierParameter(double time, double outgoing, double incoming)
{
    double lower = 0.0;
    double upper = 1.0;
    double u = time;
    for (int iteration = 0; iteration < 8; ++iteration) {
        const double difference = cubicBezier(u, 0.0, outgoing, 1.0 - incoming, 1.0) - time;
        if (std::abs(difference) <= 1e-14) {
            return u;
        }
        if (difference < 0.0) {
            lower = u;
        } else {
            upper = u;
        }
        const double v = 1.0 - u;
        const double derivative = 3.0 * v * v * outgoing
            + 6.0 * v * u * (1.0 - incoming - outgoing)
            + 3.0 * u * u * incoming;
        const double candidate = derivative > 1e-14
            ? u - difference / derivative
            : std::numeric_limits<double>::quiet_NaN();
        u = std::isfinite(candidate) && candidate > lower && candidate < upper
            ? candidate
            : (lower + upper) * 0.5;
    }
    for (int iteration = 0; iteration < 40; ++iteration) {
        u = (lower + upper) * 0.5;
        const double difference = cubicBezier(u, 0.0, outgoing, 1.0 - incoming, 1.0) - time;
        if (std::abs(difference) <= 1e-14) {
            break;
        }
        if (difference < 0.0) {
            lower = u;
        } else {
            upper = u;
        }
    }
    return u;
}

double timingCurve(double progress, double x1, double y1, double x2, double y2)
{
    const double u = bezierParameter(std::clamp(progress, 0.0, 1.0), x1, 1.0 - x2);
    return cubicBezier(u, 0.0, y1, y2, 1.0);
}

QString curveKey(const QString &path, const QString &attribute)
{
    return path + QChar(0x1f) + attribute;
}

QJsonDocument readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Unable to open source Watch data" << path;
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Unable to parse source Watch data" << path << error.errorString();
        return {};
    }
    return document;
}

QQuaternion quaternionFromJson(const QJsonObject &object)
{
    return QQuaternion(float(sourceNumber(object.value(QStringLiteral("w")))),
                       float(sourceNumber(object.value(QStringLiteral("x")))),
                       float(sourceNumber(object.value(QStringLiteral("y")))),
                       float(sourceNumber(object.value(QStringLiteral("z"))))).normalized();
}
}

struct WatchAnimationController::Data
{
    struct ScalarKey
    {
        double time = 0.0;
        double value = 0.0;
        double inSlope = 0.0;
        double outSlope = 0.0;
        int weightedMode = 0;
        double inWeight = 1.0 / 3.0;
        double outWeight = 1.0 / 3.0;
    };

    struct ScalarCurve
    {
        QVector<ScalarKey> keys;

        [[nodiscard]] double sample(double time) const
        {
            if (keys.isEmpty() || !std::isfinite(time)) {
                return 0.0;
            }
            if (time <= keys.constFirst().time) {
                return keys.constFirst().value;
            }
            if (time >= keys.constLast().time) {
                return keys.constLast().value;
            }
            qsizetype lower = 0;
            qsizetype upper = keys.size() - 1;
            while (upper - lower > 1) {
                const qsizetype middle = (lower + upper) / 2;
                if (keys.at(middle).time <= time) {
                    lower = middle;
                } else {
                    upper = middle;
                }
            }
            const ScalarKey &a = keys.at(lower);
            const ScalarKey &b = keys.at(upper);
            if (time == a.time || std::isinf(a.outSlope) || std::isinf(b.inSlope)) {
                return a.value;
            }
            const double duration = b.time - a.time;
            const double fraction = (time - a.time) / duration;
            if (!(a.weightedMode & 2) && !(b.weightedMode & 1)) {
                const double u2 = fraction * fraction;
                const double u3 = u2 * fraction;
                return (2.0 * u3 - 3.0 * u2 + 1.0) * a.value
                    + (u3 - 2.0 * u2 + fraction) * duration * a.outSlope
                    + (-2.0 * u3 + 3.0 * u2) * b.value
                    + (u3 - u2) * duration * b.inSlope;
            }
            const double outgoing = (a.weightedMode & 2) ? a.outWeight : 1.0 / 3.0;
            const double incoming = (b.weightedMode & 1) ? b.inWeight : 1.0 / 3.0;
            const double u = bezierParameter(fraction, outgoing, incoming);
            return cubicBezier(u, a.value,
                               a.value + duration * outgoing * a.outSlope,
                               b.value - duration * incoming * b.inSlope,
                               b.value);
        }
    };

    struct Curve
    {
        QVector<ScalarCurve> channels;
    };

    struct Clip
    {
        double duration = 0.0;
        QHash<QString, Curve> curves;
    };

    Clip entrance;
    Clip exit;
    ScalarCurve blurEntrance;
    ScalarCurve blurExit;
    ScalarCurve gyroPitch;
    ScalarCurve gyroYaw;
    double maximumPitch = -2.0;
    double maximumYaw = 3.0;
    QQuaternion neutralGyro;
    QQuaternion contentRotation;
    bool valid = false;

    static QVector<double> components(const QJsonValue &value, int count)
    {
        QVector<double> result;
        result.reserve(count);
        if (count == 1) {
            result.append(sourceNumber(value));
            return result;
        }
        const QJsonObject object = value.toObject();
        result.append(sourceNumber(object.value(QStringLiteral("x"))));
        result.append(sourceNumber(object.value(QStringLiteral("y"))));
        result.append(sourceNumber(object.value(QStringLiteral("z"))));
        if (count == 4) {
            result.append(sourceNumber(object.value(QStringLiteral("w"))));
        }
        return result;
    }

    static Curve parseCurve(const QJsonObject &curveObject)
    {
        const QString group = curveObject.value(QStringLiteral("group")).toString();
        const int count = group == QStringLiteral("m_FloatCurves")
            ? 1
            : group == QStringLiteral("m_RotationCurves") ? 4 : 3;
        Curve result;
        result.channels.resize(count);
        const QJsonArray keys = curveObject.value(QStringLiteral("raw")).toObject()
                                    .value(QStringLiteral("curve")).toObject()
                                    .value(QStringLiteral("m_Curve")).toArray();
        for (const QJsonValue &keyValue : keys) {
            const QJsonObject key = keyValue.toObject();
            const QVector<double> values = components(key.value(QStringLiteral("value")), count);
            const QVector<double> incomingSlopes = components(key.value(QStringLiteral("inSlope")), count);
            const QVector<double> outgoingSlopes = components(key.value(QStringLiteral("outSlope")), count);
            const QVector<double> incomingWeights = components(key.value(QStringLiteral("inWeight")), count);
            const QVector<double> outgoingWeights = components(key.value(QStringLiteral("outWeight")), count);
            for (int component = 0; component < count; ++component) {
                result.channels[component].keys.append({
                    sourceNumber(key.value(QStringLiteral("time"))),
                    values.value(component),
                    incomingSlopes.value(component),
                    outgoingSlopes.value(component),
                    key.value(QStringLiteral("weightedMode")).toInt(),
                    incomingWeights.value(component, 1.0 / 3.0),
                    outgoingWeights.value(component, 1.0 / 3.0)
                });
            }
        }
        return result;
    }

    static ScalarCurve parseScalarBody(const QJsonObject &body)
    {
        ScalarCurve result;
        for (const QJsonValue &keyValue : body.value(QStringLiteral("m_Curve")).toArray()) {
            const QJsonObject key = keyValue.toObject();
            result.keys.append({
                sourceNumber(key.value(QStringLiteral("time"))),
                sourceNumber(key.value(QStringLiteral("value"))),
                sourceNumber(key.value(QStringLiteral("inSlope"))),
                sourceNumber(key.value(QStringLiteral("outSlope"))),
                key.value(QStringLiteral("weightedMode")).toInt(),
                sourceNumber(key.value(QStringLiteral("inWeight"))),
                sourceNumber(key.value(QStringLiteral("outWeight")))
            });
        }
        return result;
    }

    static Clip parseClip(const QJsonObject &object)
    {
        Clip result;
        result.duration = object.value(QStringLiteral("last_key_time")).toDouble();
        for (const QJsonValue &curveValue : object.value(QStringLiteral("curves")).toArray()) {
            const QJsonObject curve = curveValue.toObject();
            result.curves.insert(curveKey(curve.value(QStringLiteral("path")).toString(),
                                          curve.value(QStringLiteral("attribute")).toString()),
                                 parseCurve(curve));
        }
        return result;
    }

    [[nodiscard]] const Curve *curve(const Clip &clip, const QString &path,
                                     const QString &attribute) const
    {
        const auto iterator = clip.curves.constFind(curveKey(path, attribute));
        return iterator == clip.curves.constEnd() ? nullptr : &iterator.value();
    }

    [[nodiscard]] QVector<double> sampled(const Curve *curve, double time) const
    {
        QVector<double> result;
        if (!curve) {
            return result;
        }
        result.reserve(curve->channels.size());
        for (const ScalarCurve &channel : curve->channels) {
            result.append(channel.sample(time));
        }
        return result;
    }

    [[nodiscard]] QVector<double> value(int phase, double time, const QString &path,
                                        const QString &attribute) const
    {
        const Curve *entranceCurve = curve(entrance, path, attribute);
        if (phase == OpeningPhase) {
            return sampled(entranceCurve, std::clamp(time, 0.0, entrance.duration));
        }
        if (phase == ClosingPhase) {
            if (const Curve *exitCurve = curve(exit, path, attribute)) {
                return sampled(exitCurve, std::clamp(time, 0.0, exit.duration));
            }
        }
        return sampled(entranceCurve, entrance.duration);
    }

    [[nodiscard]] double scalar(int phase, double time, const QString &path,
                                const QString &attribute, int component,
                                double fallback) const
    {
        const QVector<double> values = value(phase, time, path, attribute);
        const double result = values.value(component, fallback);
        return std::isfinite(result) ? result : fallback;
    }

    [[nodiscard]] double normalizedScalar(int phase, double time, const QString &path,
                                          const QString &attribute, int component,
                                          double fallback = 1.0) const
    {
        const QVector<double> stableValues = value(0, entrance.duration, path, attribute);
        const double stable = stableValues.value(component, fallback);
        if (!std::isfinite(stable) || std::abs(stable) < 1e-8) {
            return fallback;
        }
        return scalar(phase, time, path, attribute, component, stable) / stable;
    }

    [[nodiscard]] QQuaternion rotation(int phase, double time, const QString &path) const
    {
        const QVector<double> values = value(phase, time, path, QStringLiteral("m_RotationCurves"));
        if (values.size() != 4) {
            return {};
        }
        return QQuaternion(float(values.at(3)), float(values.at(0)),
                           float(values.at(1)), float(values.at(2))).normalized();
    }

    [[nodiscard]] double zRotation(int phase, double time, const QString &path) const
    {
        const QQuaternion value = rotation(phase, time, path);
        return qRadiansToDegrees(2.0 * std::atan2(double(value.z()), double(value.scalar())));
    }

    [[nodiscard]] double nativeOpacity(int phase, double time) const
    {
        if (phase == ClosingPhase) {
            if (time <= 0.0) {
                return 1.0;
            }
            if (time >= ContentFadeDuration) {
                return 0.0;
            }
            return timingCurve(1.0 - time / ContentFadeDuration,
                               0.20, 0.72, 0.22, 1.0);
        }
        if (phase != OpeningPhase) {
            return 1.0;
        }
        if (time <= ContentFadeStart) {
            return 0.0;
        }
        if (time >= ContentFadeStart + ContentFadeDuration) {
            return 1.0;
        }
        return timingCurve((time - ContentFadeStart) / ContentFadeDuration,
                           0.20, 0.72, 0.22, 1.0);
    }

    void load()
    {
        const QJsonDocument clipsDocument = readJson(QStringLiteral(":/assets/watch/clips.json"));
        for (const QJsonValue &clipValue : clipsDocument.object().value(QStringLiteral("clips")).toArray()) {
            const QJsonObject clipObject = clipValue.toObject();
            const QString name = clipObject.value(QStringLiteral("name")).toString();
            if (name == QStringLiteral("watch_in01")) {
                entrance = parseClip(clipObject);
            } else if (name == QStringLiteral("watch_out")) {
                exit = parseClip(clipObject);
            }
        }

        const QJsonObject blur = readJson(QStringLiteral(":/assets/watch/watch-blur.json")).object();
        const auto blurBody = [](const QJsonObject &root, const QString &name) {
            return root.value(name).toObject().value(QStringLiteral("curve")).toObject()
                .value(QStringLiteral("raw")).toObject().value(QStringLiteral("curve")).toObject();
        };
        blurEntrance = parseScalarBody(blurBody(blur, QStringLiteral("entrance")));
        blurExit = parseScalarBody(blurBody(blur, QStringLiteral("exit")));

        const QJsonArray runtime = readJson(QStringLiteral(":/assets/watch/runtime-root-camera.json")).array();
        QString gyroCab;
        QString gyroGameObject;
        for (const QJsonValue &objectValue : runtime) {
            const QJsonObject object = objectValue.toObject();
            if (object.value(QStringLiteral("script")).toObject()
                    .value(QStringLiteral("m_ClassName")).toString() != QStringLiteral("UIGyroscopeEffect")) {
                continue;
            }
            gyroCab = object.value(QStringLiteral("cab")).toString();
            const QJsonObject data = object.value(QStringLiteral("data")).toObject();
            gyroGameObject = data.value(QStringLiteral("m_GameObject")).toObject()
                                 .value(QStringLiteral("m_PathID")).toString();
            const QJsonObject pitch = data.value(QStringLiteral("x")).toObject();
            const QJsonObject yaw = data.value(QStringLiteral("y")).toObject();
            maximumPitch = pitch.value(QStringLiteral("maxAngle")).toDouble();
            maximumYaw = yaw.value(QStringLiteral("maxAngle")).toDouble();
            gyroPitch = parseScalarBody(pitch.value(QStringLiteral("valueCurve")).toObject());
            gyroYaw = parseScalarBody(yaw.value(QStringLiteral("valueCurve")).toObject());
            break;
        }
        for (const QJsonValue &objectValue : runtime) {
            const QJsonObject object = objectValue.toObject();
            const QJsonObject data = object.value(QStringLiteral("data")).toObject();
            if (object.value(QStringLiteral("cab")).toString() == gyroCab
                && object.value(QStringLiteral("type")).toString() == QStringLiteral("RectTransform")
                && data.value(QStringLiteral("m_GameObject")).toObject()
                       .value(QStringLiteral("m_PathID")).toString() == gyroGameObject) {
                neutralGyro = quaternionFromJson(data.value(QStringLiteral("m_LocalRotation")).toObject());
                break;
            }
        }

        const QJsonArray nodes = readJson(QStringLiteral(":/assets/watch/scene.json"))
                                     .object().value(QStringLiteral("nodes")).toArray();
        for (const QJsonValue &nodeValue : nodes) {
            const QJsonObject node = nodeValue.toObject();
            if (node.value(QStringLiteral("path")).toString() != QStringLiteral("WatchPanel_PC/Content")) {
                continue;
            }
            contentRotation = quaternionFromJson(node.value(QStringLiteral("transform")).toObject()
                                                     .value(QStringLiteral("raw")).toObject()
                                                     .value(QStringLiteral("m_LocalRotation")).toObject());
            break;
        }
        valid = entrance.duration > 0.0 && exit.duration > 0.0
            && !blurEntrance.keys.isEmpty() && !blurExit.keys.isEmpty();
    }
};

WatchAnimationController::WatchAnimationController(QObject *parent)
    : QObject(parent)
    , m_data(std::make_unique<Data>())
{
    m_data->load();
}

WatchAnimationController::~WatchAnimationController() = default;

double WatchAnimationController::openingDuration() const
{
    return m_data->entrance.duration > 0.0 ? m_data->entrance.duration : 0.75;
}

double WatchAnimationController::closingDuration() const
{
    return m_data->exit.duration > 0.0 ? m_data->exit.duration : 1.0 / 3.0;
}

double WatchAnimationController::contentTransitionDuration() const
{
    return ContentFadeDuration;
}

QQuaternion WatchAnimationController::neutralGyro() const
{
    return m_data->neutralGyro.isNull() ? QQuaternion() : m_data->neutralGyro;
}

QVariantMap WatchAnimationController::frame(int phase, double time) const
{
    QVariantMap frame;
    if (!m_data->valid) {
        return frame;
    }

    const auto scale = [this, phase, time](const QString &path) {
        return m_data->normalizedScalar(phase, time, path,
                                        QStringLiteral("m_ScaleCurves"), 0, 1.0);
    };
    const auto alpha = [this, phase, time](const QString &path) {
        return std::clamp(m_data->normalizedScalar(phase, time, path,
                                                   QStringLiteral("m_Alpha"), 0, 1.0),
                          0.0, 1.0);
    };
    const auto property = [this, phase, time](const QString &path,
                                               const QString &attribute,
                                               double fallback = 1.0) {
        return std::clamp(m_data->scalar(phase, time, path, attribute, 0, fallback),
                          0.0, 1.0);
    };
    const QString middle = QStringLiteral("Content/WatchNode/canvas_watch/MiddleDecoNode/");
    const QString right = QStringLiteral("Content/WatchNode/canvas_watch/RightBottomNode/ScrollView/Viewport/ScrollViewContent/RightBotton/");
    const QString left = QStringLiteral("Content/WatchNode/canvas_watch/LeftBottomNode/LeftBottomNode01/");

    frame.insert(QStringLiteral("rootZ"), m_data->scalar(phase, time,
        QStringLiteral("Content/WatchNode"), QStringLiteral("m_PositionCurves"), 2, 0.0));
    frame.insert(QStringLiteral("outerScale"), scale(middle + QStringLiteral("OutRingNode")));
    frame.insert(QStringLiteral("outerRotation"), m_data->zRotation(phase, time, middle + QStringLiteral("OutRingNode")));
    frame.insert(QStringLiteral("outerOpacity"), property(
        middle + QStringLiteral("OutRingNode/OutRing"), QStringLiteral("m_Color.a")));
    frame.insert(QStringLiteral("secondaryScale"), scale(middle + QStringLiteral("DecoOut")));
    frame.insert(QStringLiteral("secondaryRotation"), m_data->zRotation(phase, time, middle + QStringLiteral("DecoOut")));
    frame.insert(QStringLiteral("secondaryOpacity"), alpha(middle + QStringLiteral("DecoOut")));
    frame.insert(QStringLiteral("mainScale"), scale(middle + QStringLiteral("MainRingNode")));
    frame.insert(QStringLiteral("mainRotation"), m_data->zRotation(phase, time, middle + QStringLiteral("MainRingNode")));
    frame.insert(QStringLiteral("mainOpacity"), property(
        middle + QStringLiteral("MainRingNode/MainRing"), QStringLiteral("m_Color.a")));
    frame.insert(QStringLiteral("yellowScale"), scale(middle + QStringLiteral("YellowRingNode")));
    frame.insert(QStringLiteral("blueScale"), scale(middle + QStringLiteral("BlueRingNode")));
    frame.insert(QStringLiteral("blueRotation"), m_data->zRotation(phase, time, middle + QStringLiteral("BlueRingNode")));
    frame.insert(QStringLiteral("blueOpacity"), property(
        middle + QStringLiteral("BlueRingNode/Mid_Blue"), QStringLiteral("m_Color.a")));
    frame.insert(QStringLiteral("triangleScale"), scale(middle + QStringLiteral("TriagleNode")));
    frame.insert(QStringLiteral("triangleRotation"), m_data->zRotation(phase, time, middle + QStringLiteral("TriagleNode")));
    frame.insert(QStringLiteral("meshScale"), scale(middle + QStringLiteral("MeshNode")));
    frame.insert(QStringLiteral("meshRotation"), m_data->zRotation(phase, time, middle + QStringLiteral("MeshNode")));
    frame.insert(QStringLiteral("wordmarkOpacity"), alpha(middle + QStringLiteral("MainRingNode/MainRing/EndfieldText")));
    frame.insert(QStringLiteral("identityOpacity"), alpha(QStringLiteral("Content/WatchNode/canvas_watch/LeftBottomNode/PlayInfoPosNode")));
    frame.insert(QStringLiteral("closeOpacity"), alpha(QStringLiteral("Content/WatchNode/canvas_watch/LeftBottomNode/QuitBtnNode")));
    frame.insert(QStringLiteral("storageOpacity"), alpha(QStringLiteral("Content/WatchNode/canvas_watch/HudBgShdow/TechtreeNode")));
    frame.insert(QStringLiteral("activityOpacity"), alpha(QStringLiteral("Content/WatchNode/canvas_watch/HudBgShdow/ReportNode")));
    frame.insert(QStringLiteral("nativeOpacity"), m_data->nativeOpacity(phase, time));

    QVariantList leftAlpha;
    leftAlpha.append(alpha(left + QStringLiteral("CharInfoBtnNode")));
    leftAlpha.append(alpha(left + QStringLiteral("ActivityBtnNode")));
    leftAlpha.append(alpha(left + QStringLiteral("GachaBtnNode")));
    leftAlpha.append(alpha(left + QStringLiteral("PurchaseNode")));
    frame.insert(QStringLiteral("leftAlpha"), leftAlpha);

    QVariantList rightAlpha;
    for (int index = 0; index < 12; ++index) {
        const int group = index / 2 + 1;
        rightAlpha.append(alpha(right + QStringLiteral("Group%1").arg(group)));
    }
    frame.insert(QStringLiteral("rightAlpha"), rightAlpha);

    const double blurTime = phase == ClosingPhase
        ? std::clamp(time, 0.0, closingDuration())
        : phase == OpeningPhase ? std::clamp(time, 0.0, openingDuration()) : openingDuration();
    frame.insert(QStringLiteral("backdropOpacity"), phase == ClosingPhase
        ? m_data->blurExit.sample(blurTime)
        : m_data->blurEntrance.sample(blurTime));
    return frame;
}

QQuaternion WatchAnimationController::gyroTarget(double normalizedX, double normalizedY,
                                                  double intensity) const
{
    if (!m_data->valid || !std::isfinite(normalizedX) || !std::isfinite(normalizedY)
        || !std::isfinite(intensity)) {
        return neutralGyro();
    }
    const double clampedIntensity = std::clamp(intensity, 0.0, 5.0);
    const double pitch = m_data->gyroPitch.sample(std::clamp(normalizedY, -1.0, 1.0))
        * m_data->maximumPitch * clampedIntensity;
    const double yaw = m_data->gyroYaw.sample(std::clamp(normalizedX, -1.0, 1.0))
        * m_data->maximumYaw * clampedIntensity;
    const QQuaternion pitchRotation = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, float(pitch));
    const QQuaternion yawRotation = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, float(yaw));
    return (yawRotation * pitchRotation).normalized();
}

QQuaternion WatchAnimationController::interpolateRotation(const QQuaternion &start,
                                                           const QQuaternion &end,
                                                           double progress) const
{
    return QQuaternion::slerp(start.normalized(), end.normalized(),
                              float(std::clamp(progress, 0.0, 1.0))).normalized();
}

QQuaternion WatchAnimationController::combinedRotation(int phase, double time,
                                                       const QQuaternion &gyro) const
{
    const QQuaternion watch = m_data->rotation(phase, time, QStringLiteral("Content/WatchNode"));
    return (gyro.normalized() * m_data->contentRotation * watch).normalized();
}

QMatrix4x4 WatchAnimationController::projectPlane(double height, bool compensate,
                                                 double pitch, double yaw,
                                                 int phase, double time,
                                                 const QMatrix4x4 &local) const
{
    // endfield_menu/shared-scene.js: CSS perspective, world transform and
    // rest-view compensation. Lengths use the original 1920 x 1080 stage.
    constexpr float distance = 1600.f;
    QMatrix4x4 rest;
    rest.translate(29.f, -40.f);
    rest.rotate(-1.f, 0.f, 0.f, 1.f);
    rest.rotate(20.f, 1.f, 0.f, 0.f);
    const QVector3D eye = rest.inverted().map(QVector3D(0.f, 0.f, distance))
                           + QVector3D(960.f, 540.f, 0.f);
    QMatrix4x4 perspective;
    perspective(3, 2) = -1.f / distance;
    QMatrix4x4 projection;
    projection.translate(960.f, 540.f);
    projection.scale(1.10f);
    projection *= perspective;
    projection.translate(29.f, -40.f);
    projection.rotate(-1.f, 0.f, 0.f, 1.f);
    projection.rotate(float(pitch), 1.f, 0.f, 0.f);
    projection.rotate(float(yaw), 0.f, 1.f, 0.f);
    // Preserve the extracted Unity clip and approved timing; its resting pose
    // is represented by the camera above, so apply only the animated delta.
    const auto pose = m_data->rotation(phase, time, QStringLiteral("Content/WatchNode"));
    const auto settled = m_data->rotation(0, openingDuration(), QStringLiteral("Content/WatchNode"));
    projection.rotate((pose * settled.conjugated()).normalized());
    projection.translate(-960.f, -540.f);
    projection.translate(0.f, 0.f, float(height));
    if (compensate) {
        const float k = (eye.z() - float(height)) / eye.z();
        projection.translate((1.f - k) * eye.x(), (1.f - k) * eye.y());
        projection.scale(k, k, 1.f);
    }
    projection *= local;
    // QQuickItem hit testing uses this same homography. Retain W for perspective,
    // flatten clip-space Z and paint the separate planes in elevation order.
    for (int column = 0; column < 4; ++column) projection(2, column) = 0.f;
    // Keep the transform invertible. The curve renderer and pointer unprojection
    // both need its inverse, even though all source vertices have local Z = 0.
    projection(2, 2) = 1.f;
    return projection;
}
