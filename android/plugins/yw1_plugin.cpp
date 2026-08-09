#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QResource>
#include <QTranslator>
#include <QWidget>

namespace {
QTranslator *g_translator = nullptr;
}

static void activateResources()
{
    Q_INIT_RESOURCE(resources);
}

static void deactivateResources()
{
    Q_CLEANUP_RESOURCE(resources);
}

extern "C" Q_DECL_EXPORT void ykw_editor1_activate()
{
    activateResources();
    if (!g_translator) {
        g_translator = new QTranslator(qApp);
        const QString resource = QStringLiteral(":/translations/translations/qt_%1.qm")
                                     .arg(QLocale::system().name());
        if (g_translator->load(resource)) {
            qApp->installTranslator(g_translator);
        }
    }
}

extern "C" Q_DECL_EXPORT void ykw_editor1_deactivate()
{
    if (g_translator) {
        qApp->removeTranslator(g_translator);
        delete g_translator;
        g_translator = nullptr;
    }
    deactivateResources();
}

extern "C" Q_DECL_EXPORT QWidget *ykw_editor1_create()
{
    return new MainWindow();
}
