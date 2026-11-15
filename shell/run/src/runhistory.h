// The last lines that opened something, newest first, like Windows'
// list in the Run box: 26 at most, no duplicate, kept between sessions.
#pragma once

#include <KConfigGroup>

#include <QStringList>

class RunHistory
{
public:
    static constexpr int MaxEntries = 26;

    explicit RunHistory(const KConfigGroup &group);

    QStringList entries() const;
    // Put this line first (moving it up when it is already there) and save.
    void add(const QString &line);

    // Pure part, for tests: the list after adding a line.
    static QStringList added(const QStringList &entries, const QString &line);

private:
    KConfigGroup m_group;
};
