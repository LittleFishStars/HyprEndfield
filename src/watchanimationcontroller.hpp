#pragma once

#include <QObject>
#include <QQuaternion>
#include <QVariantMap>

#include <memory>

class WatchAnimationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double openingDuration READ openingDuration CONSTANT)
    Q_PROPERTY(double closingDuration READ closingDuration CONSTANT)
    Q_PROPERTY(QQuaternion neutralGyro READ neutralGyro CONSTANT)

public:
    explicit WatchAnimationController(QObject *parent = nullptr);
    ~WatchAnimationController() override;

    [[nodiscard]] double openingDuration() const;
    [[nodiscard]] double closingDuration() const;
    [[nodiscard]] QQuaternion neutralGyro() const;

    Q_INVOKABLE QVariantMap frame(int phase, double time) const;
    Q_INVOKABLE QQuaternion gyroTarget(double normalizedX, double normalizedY) const;
    Q_INVOKABLE QQuaternion interpolateRotation(const QQuaternion &start,
                                                const QQuaternion &end,
                                                double progress) const;
    Q_INVOKABLE QQuaternion combinedRotation(int phase, double time,
                                             const QQuaternion &gyro) const;

private:
    struct Data;
    std::unique_ptr<Data> m_data;
};
