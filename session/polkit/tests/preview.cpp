// The real prompt windows with a pretend PAM, to look at the prompt
// without polkit: "raye" is the right password, anything else is wrong.
//
//   preview [--unknown] [--standard] [--pkexec]
//
// Prints "authorized" or "refused" when the prompt closes.
#include "iconprovider.h"
#include "prompt.h"
#include "request.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QTimer>
#include <QUrl>

#include <cstdio>

namespace
{

class PretendSession : public AuthSession
{
public:
    using AuthSession::AuthSession;

    void start() override
    {
        QTimer::singleShot(100, this, [this] {
            Q_EMIT question(QStringLiteral("Password: "), false);
        });
    }
    void respond(const QString &answer) override
    {
        const bool ok = answer == QLatin1String("raye");
        // PAM waits a moment after a wrong password
        QTimer::singleShot(ok ? 200 : 1200, this, [this, ok] {
            Q_EMIT finished(ok);
        });
    }
    void cancel() override { Q_EMIT finished(false); }
};

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addOption({QStringLiteral("unknown"), QStringLiteral("An app nobody vouches for")});
    parser.addOption({QStringLiteral("standard"), QStringLiteral("Signed in as a standard user")});
    parser.addOption({QStringLiteral("pkexec"), QStringLiteral("A command typed in the terminal")});
    parser.process(app);

    AppInfo::Caller caller;
    QString action = QStringLiteral("org.freedesktop.timedate1.set-timezone");
    QString message = QStringLiteral("Authentication is required to set the system timezone.");
    caller.name = QStringLiteral("Settings");
    caller.icon = QStringLiteral("preferences-system");
    caller.program = QStringLiteral("/usr/bin/raye-settings");
    caller.publisher = QStringLiteral("Raye Linux");
    if (parser.isSet(QStringLiteral("pkexec"))) {
        action = QStringLiteral("org.freedesktop.policykit.exec");
        message = QStringLiteral("Authentication is needed to run `/usr/bin/true' as the super user");
        caller.name = QStringLiteral("Konsole");
        caller.icon = QStringLiteral("utilities-terminal");
        caller.program = QStringLiteral("/usr/bin/true");
        caller.command = QStringLiteral("/usr/bin/true");
    }
    if (parser.isSet(QStringLiteral("unknown"))) {
        caller.name = QStringLiteral("install.sh");
        caller.icon.clear();
        caller.program = QStringLiteral("/home/techoraye/Downloads/install.sh");
        caller.command = QStringLiteral("/home/techoraye/Downloads/install.sh --all");
        caller.publisher.clear();
    }

    QList<Prompt::Account> accounts = {{QStringLiteral("techoraye"), QStringLiteral("Techoraye"), QString()}};
    QString user = QStringLiteral("techoraye");
    if (parser.isSet(QStringLiteral("standard"))) {
        accounts = {{QStringLiteral("alex"), QStringLiteral("Alex Martin"), QString()},
                    {QStringLiteral("camille"), QStringLiteral("Camille Roy"), QString()}};
        user = QStringLiteral("alex");
    }

    auto *request = new Request(user, [](const QString &, QObject *parent) {
        return new PretendSession(parent);
    });
    QObject::connect(request, &Request::finished, &app, [&app](bool ok) {
        std::printf("%s\n", ok ? "authorized" : "refused");
        std::fflush(stdout);
        app.quit();
    });

    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    // the pictures of the sign-in module, from a theme checkout
    const QByteArray repo = qgetenv("RAYE_THEME_DIR");
    if (!repo.isEmpty()) {
        QObject *palette = engine.singletonInstance<QObject *>(QStringLiteral("org.raye.signin"), QStringLiteral("RayePalette"));
        if (palette) {
            const QString dir = QString::fromLocal8Bit(repo);
            palette->setProperty("assets", QUrl::fromLocalFile(dir + QStringLiteral("/assets/signin/")));
            palette->setProperty("eye", QUrl::fromLocalFile(dir + QStringLiteral("/assets/logo/raye-eye.png")));
        }
    }
    Prompt prompt(caller, action, message, accounts, request);
    prompt.setSelf(QStringLiteral("techoraye"));
    prompt.show(&engine);
    return app.exec();
}
