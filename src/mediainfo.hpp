#pragma once
#include <QVariantMap>
#include <QUrl>

// Metadata only. Run on a worker: video probes may wait for the decoder.
QVariantMap inspectLocalMedia(const QUrl &url);
