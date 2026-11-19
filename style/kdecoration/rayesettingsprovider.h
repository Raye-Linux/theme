/*
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "raye.h"
#include "rayedecoration.h"
#include "rayesettings.h"

#include <KConfigWatcher>
#include <KSharedConfig>

#include <QObject>

namespace Raye
{
class SettingsProvider : public QObject
{
    Q_OBJECT

public:
    //* destructor
    ~SettingsProvider();

    //* singleton
    static SettingsProvider *self();

    //* internal settings for given decoration
    InternalSettingsPtr internalSettings(Decoration *) const;

    //* [Appearance] Look of rayeshellrc is the Windows 10 look
    bool windows10() const;

Q_SIGNALS:
    //* the look changed: every decoration reconfigures
    void lookChanged();

public Q_SLOTS:

    //* reconfigure
    void reconfigure();

private:
    //* constructor
    SettingsProvider();

    //* default configuration
    InternalSettingsPtr m_defaultSettings;

    //* exceptions
    InternalSettingsList m_exceptions;

    //* config object
    KSharedConfigPtr m_config;

    //* the shell's options (rayeshellrc), for the look
    KSharedConfigPtr m_shellConfig;
    KConfigWatcher::Ptr m_shellWatcher;
    bool m_windows10 = false;
    void readLook();

    //* singleton
    static SettingsProvider *s_self;
};

}
