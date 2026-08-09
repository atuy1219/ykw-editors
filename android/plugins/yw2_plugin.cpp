#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QResource>
#include <QTranslator>
#include <QWidget>

namespace {
QTranslator *g_translator = nullptr;
}

extern "C" Q_DECL_EXPORT void ykw_editor2_activate()
{
    Q_INIT_RESOURCE(resources);
    if (!g_translator) {
        g_translator = new QTranslator(qApp);
        if (!g_translator->load(QLocale::system(), QStringLiteral("app"), QStringLiteral("_"),
                                QStringLiteral(":/translations/translations"))) {
            g_translator->load(QStringLiteral(":/translations/translations/app_en.qm"));
        }
        qApp->installTranslator(g_translator);
    }
}

extern "C" Q_DECL_EXPORT void ykw_editor2_deactivate()
{
    if (g_translator) {
        qApp->removeTranslator(g_translator);
        delete g_translator;
        g_translator = nullptr;
    }
    Q_CLEANUP_RESOURCE(resources);
}

extern "C" Q_DECL_EXPORT QWidget *ykw_editor2_create()
{
    return new MainWindow();
}
