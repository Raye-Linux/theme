// The Project panel's link to the screens: reads them, says which of the
// four choices they are in, and applies the one picked.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "displayplan.h"

#include <KScreen/Types>

#include <QObject>
#include <QString>

class DisplayController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.raye.Project")
    // "pc-only", "duplicate", "extend", "second-only", or "" when it is none of them
    Q_PROPERTY(QString currentMode READ currentMode NOTIFY changed)
    Q_PROPERTY(bool canChoose READ canChoose NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    // reads the screens
    explicit DisplayController(QObject *parent = nullptr);

    QString currentMode() const;
    bool canChoose() const;
    bool ready() const;
    bool busy() const;

    Q_INVOKABLE void choose(const QString &mode);
    Q_INVOKABLE void quit();

public Q_SLOTS:
    // Win+P while the panel is open: on to the next choice, like Windows.
    Q_SCRIPTABLE void activate();
    // the screen that asked for the panel went away again
    Q_SCRIPTABLE void dismiss();

Q_SIGNALS:
    void changed();
    void busyChanged();
    void activated();
    void dismissed();
    // the choice went through, or there was nothing to do
    void chosen();
    void failed();

protected:
    // starts from these screens and reads nothing (the tests)
    DisplayController(const KScreen::ConfigPtr &config, QObject *parent);

    // asks for the screens as they are; the answer goes to loaded()
    virtual void read();
    // hands the screens a changed copy of their state; the answer goes to sent()
    virtual void send(const KScreen::ConfigPtr &config);

    // a null config: they could not be read
    void loaded(const KScreen::ConfigPtr &config);
    void sent(const KScreen::ConfigPtr &config, bool ok, const QString &error);

private:
    QList<DisplayPlan::Screen> screens() const;

    KScreen::ConfigPtr m_config;
    bool m_busy = false;
};
