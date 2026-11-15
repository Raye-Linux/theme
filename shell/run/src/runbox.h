// The Run box behind the QML window: reads the history, runs what the
// user typed, and reports a miss in the box instead of a dialog.
#pragma once

#include "runhistory.h"
#include "runparser.h"

#include <QObject>
#include <QStringList>
#include <QUrl>

class QWindow;

class RunBox : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.raye.Run")
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit RunBox(QObject *parent = nullptr);

    void setWindow(QWindow *window);

    QStringList history() const;
    QString error() const;
    bool busy() const;

    Q_INVOKABLE void run(const QString &text);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearError();
    // The text to put in the field for a file picked with Browse.
    Q_INVOKABLE QString textForFile(const QUrl &url) const;

public Q_SLOTS:
    // Called by a second "raye-run": bring this box back to the front.
    Q_SCRIPTABLE void activate();

Q_SIGNALS:
    void historyChanged();
    void errorChanged();
    void busyChanged();
    void activated();

private:
    void start(const RunTarget &target, const QString &line);
    void finished(const QString &line, int error, const QString &failure);
    void setError(const QString &error);

    RunHistory m_history;
    QWindow *m_window = nullptr;
    QString m_error;
    bool m_busy = false;
};
