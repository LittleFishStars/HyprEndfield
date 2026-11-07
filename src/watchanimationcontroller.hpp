#pragma once

#include <QObject>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QVariantMap>

#include <memory>

class WatchAnimationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double openingDuration READ openingDuration CONSTANT)
    Q_PROPERTY(double closingDuration READ closingDuration CONSTANT)
    Q_PROPERTY(double contentTransitionDuration READ contentTransitionDuration CONSTANT)
    Q_PROPERTY(QQuaternion neutralGyro READ neutralGyro CONSTANT)

public:
    explicit WatchAnimationController(QObject *parent = nullptr);
    ~WatchAnimationController() override;

    [[nodiscard]] double openingDuration() const;
    [[nodiscard]] double closingDuration() const;
    [[nodiscard]] double contentTransitionDuration() const;
    [[nodiscard]] QQuaternion neutralGyro() const;

    Q_INVOKABLE QVariantMap frame(int phase, double time) const;
    Q_INVOKABLE QQuaternion gyroTarget(double normalizedX, double normalizedY,
                                       double intensity = 1.0) const;
    Q_INVOKABLE QQuaternion interpolateRotation(const QQuaternion &start,
                                                const QQuaternion &end,
                                                double progress) const;
    Q_INVOKABLE QQuaternion combinedRotation(int phase, double time,
                                             const QQuaternion &gyro) const;
    Q_INVOKABLE QMatrix4x4 projectPlane(double height, bool compensate,
                                      const QQuaternion &gyro, int phase, double time,
                                      const QMatrix4x4 &local) const;
    Q_INVOKABLE QMatrix4x4 projectSourceNode(const QString &id, int phase, double time,
                                           const QQuaternion &gyro, double ambientTime = -1) const;
    Q_INVOKABLE void restartAmbientMotion();
    void restartAmbientMotion(quint64 seed);

private:
    struct Data;
    std::unique_ptr<Data> m_data;
};
