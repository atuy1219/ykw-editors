#include <QApplication>
#include "launcherwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Yo-kai Watch Editors"));
    QApplication::setOrganizationName(QStringLiteral("atuy1219"));

    LauncherWindow launcher;
    launcher.showMaximized();
    return app.exec();
}
