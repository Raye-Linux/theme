// The frames of the launch ring of a cursor theme
// (cursors_scalable/raye-launch-ring, drawn by art/cursors.py).
#pragma once

#include <QImage>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

struct RingFrames {
    QList<QImage> images; // one per frame, square, in device pixels
    QList<int> delays;    // ms
    QPointF hotspot;      // logical pixels from the top left: where the pointer tip goes
    double size = 0;      // logical pixels, width and height
    bool isEmpty() const { return images.isEmpty(); }
};

// Folders the ring is looked for in, for a cursor theme name, in order.
QStringList ringDirs(const QString &theme, const QStringList &iconRoots);

// Reads metadata.json of dir and draws every frame for a pointer of
// cursorSize logical pixels on a screen of the given scale.
RingFrames loadRing(const QString &dir, int cursorSize, qreal scale);
