#pragma once
#include <QImage>

// EndfieldHUD windows/modules/display_center_logo.cpp: crop alpha > 8,
// Core Graphics-matched white shadow and original 65-unit visible height.
QImage centerLogoArtwork(const QImage &source);
