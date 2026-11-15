// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalshortcut.h"

#include <KGlobalAccel>

using namespace Qt::StringLiterals;

GlobalShortcut::GlobalShortcut(QObject *parent)
    : QObject(parent)
{
}

GlobalShortcut::~GlobalShortcut()
{
    // the action goes with the object; its keys stay saved for next time
    delete m_action;
}

void GlobalShortcut::classBegin()
{
}

void GlobalShortcut::componentComplete()
{
    m_complete = true;
    registerAction();
}

void GlobalShortcut::registerAction()
{
    if (!m_complete || m_name.isEmpty() || m_action)
        return;
    auto *action = new QAction(this);
    action->setObjectName(m_name);
    action->setText(m_text.isEmpty() ? m_name : m_text);
    action->setProperty("componentName", m_component);
    action->setEnabled(m_enabled);
    connect(action, &QAction::triggered, this, [this] {
        if (m_enabled)
            Q_EMIT activated();
    });
    m_action = action;
    const QList<QKeySequence> keys = m_sequence.isEmpty() ? QList<QKeySequence>()
                                                          : QList<QKeySequence>{QKeySequence::fromString(m_sequence, QKeySequence::PortableText)};
    KGlobalAccel::self()->setDefaultShortcut(action, keys);
    // Autoloading: what the user saved in Settings wins
    KGlobalAccel::self()->setShortcut(action, keys, KGlobalAccel::Autoloading);
    if (!m_watching) {
        m_watching = true;
        connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, [this](QAction *a, const QKeySequence &) {
            if (a == m_action)
                Q_EMIT activeSequenceChanged();
        });
    }
    Q_EMIT activeSequenceChanged();
}

void GlobalShortcut::unregisterAction()
{
    if (!m_action)
        return;
    // only this process forgets it: the saved keys stay for next time
    delete m_action;
    m_action = nullptr;
    Q_EMIT activeSequenceChanged();
}

QString GlobalShortcut::activeSequence() const
{
    if (!m_action)
        return m_sequence;
    const QList<QKeySequence> keys = KGlobalAccel::self()->shortcut(m_action);
    return keys.isEmpty() ? QString() : keys.first().toString(QKeySequence::PortableText);
}

void GlobalShortcut::setName(const QString &name)
{
    if (name == m_name)
        return;
    unregisterAction();
    m_name = name;
    Q_EMIT nameChanged();
    registerAction();
}

void GlobalShortcut::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    if (m_action)
        m_action->setText(text);
    Q_EMIT textChanged();
}

void GlobalShortcut::setSequence(const QString &sequence)
{
    if (sequence == m_sequence)
        return;
    m_sequence = sequence;
    Q_EMIT sequenceChanged();
    if (m_action) {
        const QList<QKeySequence> keys = sequence.isEmpty() ? QList<QKeySequence>()
                                                            : QList<QKeySequence>{QKeySequence::fromString(sequence, QKeySequence::PortableText)};
        KGlobalAccel::self()->setDefaultShortcut(m_action, keys);
    }
}

void GlobalShortcut::setComponent(const QString &component)
{
    if (component == m_component || component.isEmpty())
        return;
    unregisterAction();
    m_component = component;
    Q_EMIT componentChanged();
    registerAction();
}

void GlobalShortcut::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    if (m_action)
        m_action->setEnabled(enabled);
    Q_EMIT enabledChanged();
}
