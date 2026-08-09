#ifndef YKW_ANDROID_LAUNCHERWINDOW_H
#define YKW_ANDROID_LAUNCHERWINDOW_H

#include <QMainWindow>
#include <QLibrary>
#include <memory>
#include <vector>

class QWidget;

class LauncherWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LauncherWindow(QWidget *parent = nullptr);
    ~LauncherWindow() override;

private:
    using CreateWindowFn = QWidget *(*)();
    using LifecycleFn = void (*)();

    struct EditorPlugin {
        QString title;
        QString libraryName;
        QByteArray createSymbol;
        QByteArray activateSymbol;
        QByteArray deactivateSymbol;
        std::unique_ptr<QLibrary> library;
        CreateWindowFn createWindow = nullptr;
        LifecycleFn activate = nullptr;
        LifecycleFn deactivate = nullptr;
    };

    std::vector<EditorPlugin> m_plugins;
    QWidget *m_activeWindow = nullptr;
    EditorPlugin *m_activePlugin = nullptr;

    void addEditorButton(const QString &title, int pluginIndex, QWidget *container);
    bool resolvePlugin(EditorPlugin &plugin);
    void openEditor(int pluginIndex);
    void closeActiveEditor();
};

#endif
