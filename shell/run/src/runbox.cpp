#include "runbox.h"

#include <KIO/ApplicationLauncherJob>
#include <KIO/CommandLauncherJob>
#include <KIO/Global>
#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>
#include <KService>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QWindow>

namespace
{
// The widgets delegate of KIO: it is the one that can ask "Open with"
// when nothing opens a file, and "Run this program?" for a script.
class LaunchDelegate : public KIO::JobUiDelegate
{
public:
    LaunchDelegate()
        : KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingDisabled, nullptr)
    {
    }
};
// What was typed can hold anything: only the user may read the history.
KSharedConfigPtr openState()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/raye-runstaterc");
    QFile file(path);
    if (file.open(QIODevice::ReadWrite))
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    return KSharedConfig::openConfig(path, KConfig::SimpleConfig);
}
} // namespace

RunBox::RunBox(QObject *parent)
    : QObject(parent)
    , m_history(openState()->group(QStringLiteral("History")))
{
}

void RunBox::setWindow(QWindow *window)
{
    m_window = window;
}

QStringList RunBox::history() const
{
    return m_history.entries();
}

QString RunBox::error() const
{
    return m_error;
}

bool RunBox::busy() const
{
    return m_busy;
}

void RunBox::run(const QString &text)
{
    if (m_busy)
        return;
    const QString line = text.trimmed();
    const RunTarget target = RunParser::parse(line, RunParser::systemEnvironment());
    switch (target.kind) {
    case RunTarget::Empty:
        return;
    case RunTarget::NotFound:
        setError(i18nd("raye_shell_run", "Raye can't find \"%1\". Make sure you typed the name correctly, and then try again.", target.name));
        return;
    default:
        start(target, line);
    }
}

void RunBox::start(const RunTarget &target, const QString &line)
{
    KJob *job = nullptr;
    switch (target.kind) {
    case RunTarget::Url:
        job = new KIO::OpenUrlJob(target.url);
        break;
    case RunTarget::Path: {
        auto *open = new KIO::OpenUrlJob(target.url);
        // A program or script typed by its path runs, as in Windows.
        open->setRunExecutables(true);
        job = open;
        break;
    }
    case RunTarget::Command:
        job = new KIO::CommandLauncherJob(target.program, target.args);
        break;
    case RunTarget::ShellCommand:
        // The user's own line, run with the user's rights, like a terminal.
        job = new KIO::CommandLauncherJob(target.text);
        break;
    case RunTarget::Application: {
        const KService::Ptr service = KService::serviceByDesktopName(target.desktopId);
        if (!service) {
            setError(i18nd("raye_shell_run", "Raye can't find \"%1\". Make sure you typed the name correctly, and then try again.", line));
            return;
        }
        auto *launch = new KIO::ApplicationLauncherJob(service);
        launch->setUrls(target.urls);
        job = launch;
        break;
    }
    default:
        return;
    }

    // Dialogs a launch may need ("Open with", "Run this program?") get a
    // window of their own; the box steps out of the way meanwhile.
    job->setUiDelegate(new LaunchDelegate);
    m_busy = true;
    Q_EMIT busyChanged();
    if (m_window)
        m_window->hide();
    connect(job, &KJob::result, this, [this, job, line]() {
        finished(line, job->error(), job->error() ? job->errorString() : QString());
    });
    job->start();
}

void RunBox::finished(const QString &line, int error, const QString &failure)
{
    m_busy = false;
    Q_EMIT busyChanged();
    if (error == KJob::NoError) {
        m_history.add(line);
        QCoreApplication::quit();
        return;
    }
    // "Open with" or "Run this program?" answered with Cancel: back to the box.
    if (error != KJob::KilledJobError && error != KIO::ERR_USER_CANCELED)
        setError(i18nd("raye_shell_run", "Raye couldn't open \"%1\". %2", line, failure));
    if (m_window) {
        m_window->show();
        m_window->requestActivate();
    }
}

void RunBox::cancel()
{
    QCoreApplication::quit();
}

void RunBox::clearError()
{
    setError(QString());
}

void RunBox::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    Q_EMIT errorChanged();
}

QString RunBox::textForFile(const QUrl &url) const
{
    if (!url.isLocalFile())
        return url.toString();
    return RunParser::textForPath(url.toLocalFile());
}

void RunBox::activate()
{
    if (m_window && !m_busy) {
        m_window->show();
        m_window->requestActivate();
    }
    Q_EMIT activated();
}
