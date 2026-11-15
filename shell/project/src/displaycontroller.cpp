// SPDX-License-Identifier: GPL-3.0-or-later
#include "displaycontroller.h"

#include <KScreen/Config>
#include <KScreen/GetConfigOperation>
#include <KScreen/Mode>
#include <KScreen/Output>
#include <KScreen/SetConfigOperation>

#include <QCoreApplication>
#include <QDebug>

using namespace DisplayPlan;

namespace
{
// the mode of that size with the highest refresh rate
QString modeFor(const KScreen::OutputPtr &output, QSize size)
{
    QString best;
    float rate = -1;
    const auto modes = output->modes();
    for (const KScreen::ModePtr &mode : modes) {
        if (mode->size() == size && mode->refreshRate() > rate) {
            best = mode->id();
            rate = mode->refreshRate();
        }
    }
    return best;
}
}

DisplayController::DisplayController(QObject *parent)
    : QObject(parent)
{
    read();
}

DisplayController::DisplayController(const KScreen::ConfigPtr &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

void DisplayController::read()
{
    auto *op = new KScreen::GetConfigOperation();
    connect(op, &KScreen::GetConfigOperation::finished, this, [this](KScreen::ConfigOperation *done) {
        if (done->hasError()) {
            qWarning() << "raye-project: cannot read the screens:" << done->errorString();
            loaded({});
            return;
        }
        loaded(static_cast<KScreen::GetConfigOperation *>(done)->config());
    });
}

void DisplayController::loaded(const KScreen::ConfigPtr &config)
{
    if (!config) {
        Q_EMIT failed();
        return;
    }
    m_config = config;
    Q_EMIT changed();
}

QList<Screen> DisplayController::screens() const
{
    QList<Screen> out;
    if (!m_config)
        return out;
    const auto outputs = m_config->connectedOutputs();
    for (const KScreen::OutputPtr &o : outputs) {
        Screen s;
        s.name = o->name();
        s.embedded = o->type() == KScreen::Output::Panel;
        s.enabled = o->isEnabled();
        s.pos = o->pos();
        s.scale = o->scale();
        s.sideways = !o->isHorizontal();
        if (const KScreen::ModePtr mode = o->currentMode())
            s.size = mode->size();
        if (const KScreen::ModePtr mode = o->preferredMode())
            s.preferred = mode->size();
        const auto modes = o->modes();
        for (const KScreen::ModePtr &mode : modes) {
            if (!s.sizes.contains(mode->size()))
                s.sizes.append(mode->size());
        }
        out.append(s);
    }
    return out;
}

QString DisplayController::currentMode() const
{
    return modeName(current(screens()));
}

bool DisplayController::canChoose() const
{
    return DisplayPlan::canChoose(screens());
}

bool DisplayController::ready() const
{
    return !m_config.isNull();
}

bool DisplayController::busy() const
{
    return m_busy;
}

void DisplayController::choose(const QString &name)
{
    if (m_busy || !m_config)
        return;
    const QList<Target> targets = plan(modeFromName(name), screens());
    if (targets.isEmpty()) {
        Q_EMIT chosen();
        return;
    }

    // the change is made on a copy: the screens may refuse it, and then
    // what this panel knows of them must still be what they are
    const KScreen::ConfigPtr next = m_config->clone();
    const auto outputs = next->connectedOutputs();
    for (const Target &t : targets) {
        for (const KScreen::OutputPtr &o : outputs) {
            if (o->name() != t.name)
                continue;
            o->setEnabled(t.enabled);
            if (!t.enabled)
                continue;
            o->setPos(t.pos);
            const QString mode = t.size.isValid() ? modeFor(o, t.size) : QString();
            if (!mode.isEmpty())
                o->setCurrentModeId(mode);
            next->setOutputPriority(o, t.priority);
        }
    }

    m_busy = true;
    Q_EMIT busyChanged();
    send(next);
}

void DisplayController::send(const KScreen::ConfigPtr &config)
{
    auto *op = new KScreen::SetConfigOperation(config);
    connect(op, &KScreen::SetConfigOperation::finished, this, [this, config](KScreen::ConfigOperation *done) {
        sent(config, !done->hasError(), done->errorString());
    });
}

void DisplayController::sent(const KScreen::ConfigPtr &config, bool ok, const QString &error)
{
    m_busy = false;
    Q_EMIT busyChanged();
    if (!ok) {
        qWarning() << "raye-project: the screens refused the change:" << error;
        Q_EMIT failed();
        // some of it may have gone through: ask them where they are now
        read();
        return;
    }
    m_config = config;
    Q_EMIT changed();
    Q_EMIT chosen();
}

void DisplayController::quit()
{
    QCoreApplication::quit();
}

void DisplayController::activate()
{
    Q_EMIT activated();
}

void DisplayController::dismiss()
{
    Q_EMIT dismissed();
}
