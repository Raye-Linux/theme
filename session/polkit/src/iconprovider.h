// image://icon/<name>: an icon of the current theme, or a file when the
// desktop file gives a path.
#pragma once

#include <QIcon>
#include <QQuickImageProvider>

class IconProvider : public QQuickImageProvider
{
public:
    IconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        const int side = requestedSize.isValid() ? qMax(requestedSize.width(), requestedSize.height()) : 64;
        QIcon icon = id.startsWith(QLatin1Char('/')) ? QIcon(id) : QIcon::fromTheme(id);
        if (icon.isNull())
            icon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
        const QPixmap pixmap = icon.pixmap(QSize(side, side));
        if (size)
            *size = pixmap.size();
        return pixmap;
    }
};
