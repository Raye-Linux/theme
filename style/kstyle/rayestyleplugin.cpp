/*
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rayestyleplugin.h"
#include "rayestyle.h"

#include <QApplication>

namespace Raye
{
//_________________________________________________
QStyle *StylePlugin::create(const QString &key)
{
    if (key.compare(QLatin1String("raye"), Qt::CaseInsensitive) == 0) {
        return new Style;
    }
    return nullptr;
}

//_________________________________________________
QStringList StylePlugin::keys() const
{
    return QStringList(QStringLiteral("Raye"));
}

}
