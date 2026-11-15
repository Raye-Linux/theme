//////////////////////////////////////////////////////////////////////////////
// rayeanimationconfigitem.h
// animation configuration item
// -------------------
//
// SPDX-FileCopyrightText: 2010 Hugo Pereira Da Costa <hugo.pereira@free.fr>
//
// SPDX-License-Identifier: MIT
//////////////////////////////////////////////////////////////////////////////

#include <QAbstractScrollArea>
#include <QApplication>
#include <QIcon>

#include <KCMultiDialog>
#include <KLocalizedString>
#include <KPluginMetaData>

//__________________________________________
int main(int argc, char *argv[])
{
    KLocalizedString::setApplicationDomain("raye_style_config");

    QApplication app(argc, argv);
    app.setApplicationName(i18n("Raye Style Settings"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("raye-style-settings")));

    KCMultiDialog dialog;
    dialog.setWindowTitle(i18n("Raye Style Settings"));
    dialog.addModule(KPluginMetaData(QStringLiteral("kstyle_config/rayestyleconfig")));
    dialog.addModule(KPluginMetaData(QStringLiteral("org.kde.kdecoration3.kcm/kcm_rayedecoration.so")));
    dialog.show();

    const auto children = dialog.findChildren<QAbstractScrollArea *>();
    for (auto child : children) {
        child->adjustSize();
        child->viewport()->adjustSize();
    }

    return app.exec();
}
