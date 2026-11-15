#include "ringframes.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSvgRenderer>

static const QString s_ring = QStringLiteral("cursors_scalable/raye-launch-ring");

QStringList ringDirs(const QString &theme, const QStringList &iconRoots)
{
    // the chosen theme first; a theme without the ring (one that is not
    // Raye's) still gets Raye's ring rather than nothing
    QStringList names{theme};
    if (theme != QLatin1String("Raye")) {
        names << QStringLiteral("Raye");
    }
    QStringList out;
    for (const QString &name : std::as_const(names)) {
        for (const QString &root : iconRoots) {
            const QString d = root + QLatin1Char('/') + name + QLatin1Char('/') + s_ring;
            if (QFile::exists(d + QStringLiteral("/metadata.json"))) {
                out << d;
            }
        }
    }
    return out;
}

RingFrames loadRing(const QString &dir, int cursorSize, qreal scale)
{
    RingFrames ring;
    QFile f(dir + QStringLiteral("/metadata.json"));
    if (cursorSize <= 0 || scale <= 0 || !f.open(QIODevice::ReadOnly)) {
        return ring;
    }
    const QJsonArray meta = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue &v : meta) {
        const QJsonObject e = v.toObject();
        QSvgRenderer svg(dir + QLatin1Char('/') + e.value(QStringLiteral("filename")).toString());
        if (!svg.isValid()) {
            return RingFrames();
        }
        // the drawing is a canvas of viewBox units, nominal_size of which
        // make one pointer size, exactly like the scalable pointers
        const double nominal = e.value(QStringLiteral("nominal_size")).toDouble(24);
        const double units = cursorSize / nominal;
        if (ring.images.isEmpty()) {
            ring.size = svg.viewBoxF().width() * units;
            ring.hotspot = QPointF(e.value(QStringLiteral("hotspot_x")).toDouble() * units,
                                   e.value(QStringLiteral("hotspot_y")).toDouble() * units);
        }
        const int px = qRound(ring.size * scale);
        QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        svg.render(&p, QRectF(0, 0, px, px));
        p.end();
        ring.images << img;
        ring.delays << qMax(10, e.value(QStringLiteral("delay")).toInt(40));
    }
    return ring;
}
