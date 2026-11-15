// The clipboard history of the session, from Klipper over D-Bus
// (org.kde.klipper /klipper).
#pragma once

#include "clipboardstore.h"

class KlipperBackend : public ClipboardBackend
{
    Q_OBJECT

public:
    explicit KlipperBackend(QObject *parent = nullptr);

    bool available() const override;
    QStringList history() const override;
    void setCurrent(const QString &text) override;
    void clear() override;
    QString current() const;
};
