/*
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "rayesettingsprovider.h"

#include "rayedecorationmetrics.h"
#include "rayeexceptionlist.h"

#include <QRegularExpression>
#include <QTextStream>

namespace Raye
{
SettingsProvider *SettingsProvider::s_self = nullptr;

//__________________________________________________________________
SettingsProvider::SettingsProvider()
    : m_config(KSharedConfig::openConfig(QStringLiteral("rayerc")))
    , m_shellConfig(KSharedConfig::openConfig(QStringLiteral("rayeshellrc"), KConfig::CascadeConfig))
{
    // Raye Settings writes the look with KConfig::Notify: windows follow at once
    m_shellWatcher = KConfigWatcher::create(m_shellConfig);
    connect(m_shellWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        if (group.name() != QLatin1String("Appearance")) {
            return;
        }
        const bool was = m_windows10;
        readLook();
        if (was != m_windows10) {
            Q_EMIT lookChanged();
        }
    });
    reconfigure();
}

//__________________________________________________________________
void SettingsProvider::readLook()
{
    m_windows10 = DecorationMetrics::isWindows10Look(m_shellConfig->group(QStringLiteral("Appearance")).readEntry("Look", QString()));
}

//__________________________________________________________________
bool SettingsProvider::windows10() const
{
    return m_windows10;
}

//__________________________________________________________________
SettingsProvider::~SettingsProvider()
{
    s_self = nullptr;
}

//__________________________________________________________________
SettingsProvider *SettingsProvider::self()
{
    // TODO: this is not thread safe!
    if (!s_self) {
        s_self = new SettingsProvider();
    }

    return s_self;
}

//__________________________________________________________________
void SettingsProvider::reconfigure()
{
    if (!m_defaultSettings) {
        m_defaultSettings = InternalSettingsPtr(new InternalSettings());
        m_defaultSettings->setCurrentGroup(QStringLiteral("Windeco"));
    }

    m_defaultSettings->load();

    const bool was = m_windows10;
    m_shellConfig->reparseConfiguration();
    readLook();
    if (was != m_windows10) {
        Q_EMIT lookChanged();
    }

    ExceptionList exceptions;
    exceptions.readConfig(m_config);
    m_exceptions = exceptions.get();
}

//__________________________________________________________________
InternalSettingsPtr SettingsProvider::internalSettings(Decoration *decoration) const
{
    QString windowTitle;
    QString windowClass;

    for (auto internalSettings : std::as_const(m_exceptions)) {
        // discard disabled exceptions
        if (!internalSettings->enabled()) {
            continue;
        }

        // discard exceptions with empty exception pattern
        if (internalSettings->exceptionPattern().isEmpty()) {
            continue;
        }

        /*
        decide which value is to be compared
        to the regular expression, based on exception type
        */
        QString value;
        switch (internalSettings->exceptionType()) {
        case InternalSettings::ExceptionWindowTitle: {
            value = windowTitle.isEmpty() ? (windowTitle = decoration->window()->caption()) : windowTitle;
            break;
        }

        default:
        case InternalSettings::ExceptionWindowClassName: {
            value = windowClass.isEmpty() ? (windowClass = decoration->window()->windowClass()) : windowClass;
            break;
        }
        }

        // check matching
        QRegularExpression rx(internalSettings->exceptionPattern());
        if (rx.match(value).hasMatch()) {
            return internalSettings;
        }
    }

    return m_defaultSettings;
}

}
