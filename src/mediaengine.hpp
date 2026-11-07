#pragma once
#include "endfield/modules/media_assembly_processor.hpp"
#include <QImage>
#include <QVariantMap>
#include <atomic>

QImage renderMediaFrame(const QImage &image, const endfield::modules::MediaAssemblyAdjustments &adjustments,
                       bool includeStickers, bool video = false);
QString exportMedia(const QVariantMap &metadata, const endfield::modules::MediaAssemblyAdjustments &adjustments,
                    const QString &destination, bool overwrite, std::atomic_bool &cancel, std::atomic<double> &progress);
