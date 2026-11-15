// A global shortcut for a part of the shell, declared in QML:
//
//   GlobalShortcut { name: "raye-quick-settings"; text: i18nd(...); sequence: "Meta+A"; onActivated: ... }
//
// The action lives in the "plasmashell" component of kglobalaccel, so it
// shows in Settings with Plasma's own and the user can change it there; a
// key the user picked wins over the default given here.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAction>
#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QQmlParserStatus>
#include <qqmlregistration.h>

class GlobalShortcut : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)

    // the action's id, stable across versions (what kglobalshortcutsrc stores)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    // what Settings shows for it
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    // the default keys, "Meta+A"; empty for none
    Q_PROPERTY(QString sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)
    Q_PROPERTY(QString component READ component WRITE setComponent NOTIFY componentChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    // the keys in use (the user's choice or the default)
    Q_PROPERTY(QString activeSequence READ activeSequence NOTIFY activeSequenceChanged)

public:
    explicit GlobalShortcut(QObject *parent = nullptr);
    ~GlobalShortcut() override;

    QString name() const { return m_name; }
    void setName(const QString &name);
    QString text() const { return m_text; }
    void setText(const QString &text);
    QString sequence() const { return m_sequence; }
    void setSequence(const QString &sequence);
    QString component() const { return m_component; }
    void setComponent(const QString &component);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    QString activeSequence() const;

    void classBegin() override;
    void componentComplete() override;

    // registered with kglobalaccel (false until name is set and QML is done)
    bool isRegistered() const { return m_action != nullptr; }

Q_SIGNALS:
    void activated();
    void nameChanged();
    void textChanged();
    void sequenceChanged();
    void componentChanged();
    void enabledChanged();
    void activeSequenceChanged();

private:
    void registerAction();
    void unregisterAction();

    QString m_name;
    QString m_text;
    QString m_sequence;
    QString m_component = QStringLiteral("plasmashell");
    bool m_enabled = true;
    bool m_complete = false;
    bool m_watching = false;
    QPointer<QAction> m_action;
};
