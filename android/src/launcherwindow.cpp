#include "launcherwindow.h"

#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

LauncherWindow::LauncherWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Yo-kai Watch Editors"));

    m_plugins.push_back({
        tr("Yo-kai Watch 1"),
        QStringLiteral("ykw_editor1"),
        QByteArrayLiteral("ykw_editor1_create"),
        QByteArrayLiteral("ykw_editor1_activate"),
        QByteArrayLiteral("ykw_editor1_deactivate")
    });
    m_plugins.push_back({
        tr("Yo-kai Watch 2"),
        QStringLiteral("ykw_editor2"),
        QByteArrayLiteral("ykw_editor2_create"),
        QByteArrayLiteral("ykw_editor2_activate"),
        QByteArrayLiteral("ykw_editor2_deactivate")
    });
    m_plugins.push_back({
        tr("Yo-kai Watch Blasters"),
        QStringLiteral("ykw_editor_b1"),
        QByteArrayLiteral("ykw_editor_b1_create"),
        QByteArrayLiteral("ykw_editor_b1_activate"),
        QByteArrayLiteral("ykw_editor_b1_deactivate")
    });

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *title = new QLabel(tr("Select an editor"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    layout->addStretch(1);
    layout->addWidget(title);

    for (int i = 0; i < static_cast<int>(m_plugins.size()); ++i) {
        addEditorButton(m_plugins[static_cast<size_t>(i)].title, i, central);
    }

    auto *note = new QLabel(
        tr("The original Qt Widgets editor UI is used without redesign. "
           "On small screens, landscape orientation is recommended."),
        central);
    note->setWordWrap(true);
    note->setAlignment(Qt::AlignCenter);
    layout->addWidget(note);
    layout->addStretch(1);

    setCentralWidget(central);

    // androiddeployqt loads QT_ANDROID_EXTRA_LIBS before main(). Their compiled
    // resources use overlapping paths such as :/data, so keep every editor's
    // resource bundle disabled until that editor is selected.
    for (auto &plugin : m_plugins) {
        if (resolvePlugin(plugin) && plugin.deactivate) {
            plugin.deactivate();
        }
    }
}

LauncherWindow::~LauncherWindow()
{
    closeActiveEditor();
}

void LauncherWindow::addEditorButton(const QString &title, int pluginIndex, QWidget *container)
{
    auto *button = new QPushButton(title, container);
    button->setMinimumHeight(64);
    QFont font = button->font();
    font.setPointSize(font.pointSize() + 2);
    button->setFont(font);
    centralWidget()->layout()->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, pluginIndex]() {
        openEditor(pluginIndex);
    });
}

bool LauncherWindow::resolvePlugin(EditorPlugin &plugin)
{
    if (plugin.library && plugin.library->isLoaded() && plugin.createWindow) {
        return true;
    }

    plugin.library = std::make_unique<QLibrary>(plugin.libraryName);
    plugin.library->setLoadHints(QLibrary::ResolveAllSymbolsHint);
    if (!plugin.library->load()) {
        return false;
    }

    plugin.createWindow = reinterpret_cast<CreateWindowFn>(
        plugin.library->resolve(plugin.createSymbol.constData()));
    plugin.activate = reinterpret_cast<LifecycleFn>(
        plugin.library->resolve(plugin.activateSymbol.constData()));
    plugin.deactivate = reinterpret_cast<LifecycleFn>(
        plugin.library->resolve(plugin.deactivateSymbol.constData()));

    return plugin.createWindow && plugin.activate && plugin.deactivate;
}

void LauncherWindow::openEditor(int pluginIndex)
{
    if (pluginIndex < 0 || pluginIndex >= static_cast<int>(m_plugins.size())) {
        return;
    }
    if (m_activeWindow) {
        return;
    }

    EditorPlugin &plugin = m_plugins[static_cast<size_t>(pluginIndex)];
    if (!resolvePlugin(plugin)) {
        const QString reason = plugin.library ? plugin.library->errorString() : tr("Unknown error");
        QMessageBox::critical(this, tr("Editor load failed"),
                              tr("Could not load %1.\n%2").arg(plugin.title, reason));
        return;
    }

    // Defensive cleanup in case another editor's resource bundle was restored.
    for (auto &candidate : m_plugins) {
        if (&candidate != &plugin && resolvePlugin(candidate) && candidate.deactivate) {
            candidate.deactivate();
        }
    }

    plugin.activate();
    QWidget *window = plugin.createWindow();
    if (!window) {
        plugin.deactivate();
        QMessageBox::critical(this, tr("Editor load failed"),
                              tr("%1 did not create a window.").arg(plugin.title));
        return;
    }

    m_activePlugin = &plugin;
    m_activeWindow = window;
    window->setAttribute(Qt::WA_DeleteOnClose, true);

    connect(window, &QObject::destroyed, this, [this]() {
        if (m_activePlugin && m_activePlugin->deactivate) {
            m_activePlugin->deactivate();
        }
        m_activeWindow = nullptr;
        m_activePlugin = nullptr;
        showMaximized();
        raise();
        activateWindow();
    });

    hide();
    window->showMaximized();
}

void LauncherWindow::closeActiveEditor()
{
    if (m_activeWindow) {
        QWidget *window = m_activeWindow;
        m_activeWindow = nullptr;
        window->close();
    } else if (m_activePlugin && m_activePlugin->deactivate) {
        m_activePlugin->deactivate();
        m_activePlugin = nullptr;
    }
}
