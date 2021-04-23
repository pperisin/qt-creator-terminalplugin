/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#include "terminalwindow.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/find/basetextfind.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>
#include <utils/environment.h>
#include <utils/fileutils.h>
#include <utils/utilsicons.h>
#include <utils/qtcassert.h>

#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QTabBar>
#include <QInputDialog>
#include <QLabel>
#include <QVector>

#include <qtermwidget5/qtermwidget.h>
#include "findsupport.h"

namespace Terminal {
namespace Internal {

TerminalContainer::TerminalContainer(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_tabWidget(nullptr)
    , m_parent(parent)
{
    QCoreApplication::setOrganizationName("TermPlugin");
    QCoreApplication::setOrganizationDomain("TermPlugin");

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, &TerminalContainer::contextMenuRequested);

    Core::Context context("Terminal.Window");

    m_copy = new QAction("Copy", this);
    Core::ActionManager::registerAction(m_copy, "Terminal.Copy", context);
    connect(m_copy, &QAction::triggered, this, &TerminalContainer::copyInvoked);

    m_paste = new QAction("Paste", this);
    Core::ActionManager::registerAction(m_paste, "Terminal.Paste", context);
    connect(m_paste, &QAction::triggered, this, &TerminalContainer::pasteInvoked);

    m_close = new QAction("Close", this);
    Core::ActionManager::registerAction(m_close, "Terminal.Close", context);
    connect(m_close, &QAction::triggered, this, &TerminalContainer::closeInvoked);

    m_newTab = new QAction("New Tab", this);
    Core::ActionManager::registerAction(m_newTab, "Terminal.NewTab", context);
    connect(m_newTab, &QAction::triggered, this, &TerminalContainer::createTab);

    m_renameTab = new QAction("Rename Current Tab", this);
    Core::ActionManager::registerAction(m_renameTab, "Terminal.RenameTab", context);
    connect(m_renameTab, &QAction::triggered, this, &TerminalContainer::renameCurrentTab);

    m_tabWidget = new QTabWidget;
    m_tabWidget->addTab(initializeTerm(), "terminal");
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            this, &TerminalContainer::closeTab);

    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &TerminalContainer::currentTabChanged);

    connect(m_tabWidget, &QTabWidget::tabBarDoubleClicked,
            this, &TerminalContainer::tabBarDoubleClick);

    m_layout = new QVBoxLayout;
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addWidget(m_tabWidget);
    setLayout(m_layout);
}

QTermWidget* TerminalContainer::initializeTerm(const QString & workingDirectory)
{
    QTermWidget *termWidget = new QTermWidget(0, this);
    termWidget->setWindowTitle(tr("Terminal"));
    termWidget->setWindowIcon(QIcon());
    termWidget->setScrollBarPosition(QTermWidget::ScrollBarRight);

    // Try to get the saved color scheme. If it doesn't exist, set the default and
    // update the saved scheme. If it does exist, use that to set the terminal's scheme.
    QSettings settings;
    QString savedColorScheme = settings.value("colorScheme").toString();
    if(savedColorScheme.isEmpty()) {
#if defined(Q_OS_LINUX)
        m_currentColorScheme = "DarkPastels";
        termWidget->setKeyBindings("linux");
#elif defined(Q_OS_MACX)
        currentColorScheme = "WhiteOnBlack";
        termWidget->setKeyBindings("macbook");
#else
        currentColorScheme = "BlackOnLightYellow";
        termWidget->setKeyBindings("default");
#endif
        settings.setValue("colorScheme", m_currentColorScheme);
    }
    else {
        m_currentColorScheme = savedColorScheme;
    }
    termWidget->setColorScheme(m_currentColorScheme);

    QFont font = TextEditor::TextEditorSettings::instance()->fontSettings().font();
    termWidget->setTerminalFont(font);
    termWidget->setTerminalOpacity(1.0);

    setFocusProxy(termWidget);

    connect(termWidget, &QTermWidget::copyAvailable, this, &TerminalContainer::copyAvailable);
    connect(termWidget, &QTermWidget::finished, this, &TerminalContainer::finished);

    termWidget->setWorkingDirectory(workingDirectory.isEmpty() ? QDir::homePath()
                                                               : workingDirectory);
    Utils::Environment env = Utils::Environment::systemEnvironment();
    env.set("TERM_PROGRAM", QString("qtermwidget5"));
    env.set("TERM", QString("xterm-256color"));
    env.set("QTCREATOR_PID", QString("%1").arg(QCoreApplication::applicationPid()));
    termWidget->setEnvironment(env.toStringList());
    termWidget->startShellProgram();
    termWidget->setBlinkingCursor(true);

    QAction *copy = new QAction(termWidget);
    termWidget->addAction(copy);
    copy->setShortcut(QKeySequence(tr("Ctrl+C")));
    connect(copy, &QAction::triggered, this, &TerminalContainer::copyInvoked);

    QAction *paste = new QAction(termWidget);
    termWidget->addAction(paste);
    paste->setShortcut(QKeySequence(tr("Ctrl+V")));
    connect(paste, &QAction::triggered, this, &TerminalContainer::pasteInvoked);

    QAction *close = new QAction(termWidget);
    termWidget->addAction(close);
    close->setShortcut(QKeySequence(tr("Ctrl+D")));
    connect(close, &QAction::triggered, this, &TerminalContainer::closeInvoked);

    QAction *newTab = new QAction(termWidget);
    termWidget->addAction(newTab);
    newTab->setShortcut(QKeySequence(tr("Ctrl+T")));
    connect(newTab, &QAction::triggered, this, &TerminalContainer::createTab);

    QAction *renameTab = new QAction(termWidget);
    termWidget->addAction(renameTab);
    renameTab->setShortcut(QKeySequence(tr("F2")));
    connect(renameTab, &QAction::triggered, this, &TerminalContainer::renameCurrentTab);

    QAction *nextTab = new QAction(termWidget);
    termWidget->addAction(nextTab);
    nextTab->setShortcut(QKeySequence(tr("Ctrl+PgDown")));
    connect(nextTab, &QAction::triggered, this, &TerminalContainer::nextTab);

    QAction *prevTab = new QAction(termWidget);
    termWidget->addAction(prevTab);
    prevTab->setShortcut(QKeySequence(tr("Ctrl+PgUp")));
    connect(prevTab, &QAction::triggered, this, &TerminalContainer::prevTab);

    QAction *moveTabRight = new QAction(termWidget);
    termWidget->addAction(moveTabRight);
    moveTabRight->setShortcut(QKeySequence(tr("Ctrl+Shift+PgDown")));
    connect(moveTabRight, &QAction::triggered, this, &TerminalContainer::moveTabRight);

    QAction *moveTabLeft = new QAction(termWidget);
    termWidget->addAction(moveTabLeft);
    moveTabLeft->setShortcut(QKeySequence(tr("Ctrl+Shift+PgUp")));
    connect(moveTabLeft, &QAction::triggered, this, &TerminalContainer::moveTabLeft);

    return termWidget;
}

void TerminalContainer::closeAllTabs()
{
    for (int i = m_tabWidget->count(); i > 0; i--)
    {
        m_tabWidget->widget(i-1)->deleteLater();
        m_tabWidget->removeTab(i-1);
    }

    createTab();
}

void TerminalContainer::closeCurrentTab()
{
    if (m_tabWidget->currentIndex() < 0)
        return;

    m_tabWidget->removeTab(m_tabWidget->currentIndex());

    if (m_tabWidget->count() == 0)
        createTab();
}

void TerminalContainer::closeTab(int tabIndex)
{
    m_tabWidget->widget(tabIndex)->deleteLater();
    m_tabWidget->removeTab(tabIndex);

    if (m_tabWidget->count() == 0)
        createTab();
}

void TerminalContainer::currentTabChanged(int tabIndex)
{
    if (tabIndex <0 || tabIndex >= m_tabWidget->count())
        return;

    emit termWidgetChanged(termWidget());
}

void TerminalContainer::contextMenuRequested(const QPoint &point)
{
    QPoint globalPos = mapToGlobal(point);
    QMenu menu;
    QMenu themesSubMenu("Color Schemes");
    QVector<QAction*> actions;

    menu.addAction(m_newTab);
    menu.addAction(m_renameTab);
    menu.addAction(m_copy);
    menu.addAction(m_paste);

    // Get all available color schemes and create a dynamic list of actions corresponding
    // to each. Add to submenu. Connect a lambda function to it that will set the theme
    // when the submenu item is chosen.
    QTermWidget *terminal = termWidget();
    if(terminal) {
        for(QString &scheme : terminal->availableColorSchemes()) {
            actions.push_back(new QAction(scheme, this));
            if(scheme == m_currentColorScheme) {
                QFont font = actions.back()->font();
                font.setBold(true);
                font.setItalic(true);
                actions.back()->setFont(font);
            }
            connect(actions.back(), &QAction::triggered, this, [=]{
                m_currentColorScheme = scheme;
                for (int i = 0; i < m_tabWidget->count(); i++) {
                    QTermWidget *widget =  static_cast<QTermWidget *>(m_tabWidget->widget(i));
                    if (widget)
                        widget->setColorScheme(m_currentColorScheme);
                }

                QSettings settings;
                settings.setValue("colorScheme", m_currentColorScheme);
            });
            themesSubMenu.addAction(actions.back());
        }
    }

    menu.addMenu(&themesSubMenu);
    menu.addAction(m_close);
    menu.exec(globalPos);

    for(QAction* action : actions) {
        delete action;
    }
}

void TerminalContainer::copyInvoked()
{
    termWidget()->copyClipboard();
}

void TerminalContainer::pasteInvoked()
{
    termWidget()->pasteClipboard();
}

void TerminalContainer::copyAvailable(bool available)
{
    m_copy->setEnabled(available);
}

void TerminalContainer::closeInvoked()
{
    closeTab(m_tabWidget->currentIndex());
}

void TerminalContainer::renameCurrentTab()
{
    renameTabId(m_tabWidget->currentIndex());
}

void  TerminalContainer::tabBarDoubleClick(int index)
{
    if (index < 0 || index >= m_tabWidget->count())
        createTab();
    else
        renameTabId(index);
}

void TerminalContainer::renameTabId(int index)
{
    if (index < 0 || index >= m_tabWidget->count())
        return;

    QLineEdit *lineEdit = new QLineEdit(this);
    lineEdit->setGeometry(m_tabWidget->tabBar()->tabRect(index));
    lineEdit->setText(m_tabWidget->tabText(index));

    connect(lineEdit, &QLineEdit::editingFinished,
            this, [this, lineEdit, index]() {
        this->m_tabWidget->setTabText(index, lineEdit->text());
        lineEdit->deleteLater();
    });

    lineEdit->selectAll();
    lineEdit->show();
    lineEdit->setFocus();
}

void TerminalContainer::createTab()
{
    QString path;
    int count = m_tabWidget->count();
    if (count == 0)
    {
        path = currentDocumentPath();
    }
    else
    {
        path = termWidget()->workingDirectory();
    }
    int index = m_tabWidget->addTab(initializeTerm(path), "terminal");
    m_tabWidget->setCurrentIndex(index);

    emit termWidgetChanged(termWidget());
}

void TerminalContainer::nextTab()
{
    if (m_tabWidget->count() <= 1)
        return;

    int nextIndex = m_tabWidget->currentIndex() + 1;

    if (nextIndex >= m_tabWidget->count())
        nextIndex = 0;

    m_tabWidget->setCurrentIndex(nextIndex);

    emit termWidgetChanged(termWidget());
}

void TerminalContainer::prevTab()
{
    if (m_tabWidget->count() <= 1)
        return;

    int prevIndex = m_tabWidget->currentIndex() - 1;

    if (prevIndex < 0)
        prevIndex = m_tabWidget->count() - 1;

    m_tabWidget->setCurrentIndex(prevIndex);

    emit termWidgetChanged(termWidget());
}

void TerminalContainer::moveTabLeft()
{
    if (m_tabWidget->currentIndex() < 0)
        return;

    int currentIndex = m_tabWidget->currentIndex();
    int nextIndex = currentIndex ? currentIndex - 1 : m_tabWidget->count() - 1;

    m_tabWidget->tabBar()->moveTab(currentIndex, nextIndex);

    m_tabWidget->setCurrentIndex(nextIndex);
}

void TerminalContainer::moveTabRight()
{
    if (m_tabWidget->currentIndex() < 0)
        return;

    int currentIndex = m_tabWidget->currentIndex();
    int nextIndex = (currentIndex + 1 == m_tabWidget->count()) ? 0 : currentIndex + 1;

    m_tabWidget->tabBar()->moveTab(currentIndex, nextIndex);

    m_tabWidget->setCurrentIndex(nextIndex);
}

QString TerminalContainer::currentDocumentPath() const
{
    if (Core::IDocument *doc = Core::EditorManager::instance()->currentDocument()) {
        const QDir dir = doc->filePath().toFileInfo().absoluteDir();
        if (dir.exists())
            return dir.canonicalPath();
    }
    return QString();
}

QTermWidget *TerminalContainer::termWidget()
{
    if (m_tabWidget->count() == 0)
    {
        createTab();
    }

    return static_cast<QTermWidget *>(m_tabWidget->currentWidget());
}

TerminalWindow::TerminalWindow(QObject *parent)
    : IOutputPane(parent)
    , m_context(new Core::IContext(this))
    , m_terminalContainer(nullptr)
{
    Core::Context context("Terminal.Window");
    m_context->setContext(context);

    m_sync = new QToolButton();
    m_sync->setIcon(Utils::Icons::LINK_TOOLBAR.icon());
    m_sync->setEnabled(false);
    m_sync->setToolTip(tr("Synchronize with Editor"));
    connect(m_sync, &QAbstractButton::clicked, this, &TerminalWindow::sync);
    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
            this, [this](Core::IEditor *editor) { m_sync->setEnabled(editor); });

    m_addTab = new QToolButton();
    m_addTab->setIcon(Utils::Icons::PLUS_TOOLBAR.icon());
    m_addTab->setEnabled(true);
    m_addTab->setToolTip(tr("Create new Terminal"));

    connect(m_addTab, &QAbstractButton::clicked, this, &TerminalWindow::newTab);

    m_closeTab = new QToolButton();
    m_closeTab->setIcon(Utils::Icons::CLOSE_TOOLBAR.icon());
    m_closeTab->setEnabled(true);
    m_closeTab->setToolTip(tr("Close Current Terminal"));

    connect(m_closeTab, &QAbstractButton::clicked, this, &TerminalWindow::closeTab);
}

QWidget *TerminalWindow::outputWidget(QWidget *parent)
{
    if (!m_terminalContainer) {
        m_terminalContainer = new TerminalContainer(parent);
        connect(m_terminalContainer, &TerminalContainer::finished,
                this, &TerminalWindow::terminalFinished);

        m_context->setWidget(m_terminalContainer->termWidget());
        Core::ICore::addContextObject(m_context);

        auto findSupport = new FindSupport(m_terminalContainer->termWidget());
        connect(m_terminalContainer, &TerminalContainer::termWidgetChanged,
                findSupport, &FindSupport::setTerminal);

        Aggregation::Aggregate *agg = new Aggregation::Aggregate;
        agg->add(m_terminalContainer);
        agg->add(findSupport);
    }
    return m_terminalContainer;
}

QList<QWidget *> TerminalWindow::toolBarWidgets() const
{
    return { m_sync, m_addTab, m_closeTab };
}

QString TerminalWindow::displayName() const
{
    return tr("Terminal");
}

int TerminalWindow::priorityInStatusBar() const
{
    return 50;
}

void TerminalWindow::clearContents()
{
    if (!m_terminalContainer || !m_terminalContainer->termWidget())
        return;
    QString cmd = "clear\n";
    m_terminalContainer->termWidget()->sendText(cmd);
}

void TerminalWindow::sync()
{
    if (!m_terminalContainer || !m_terminalContainer->termWidget())
        return;
    QString docPath = m_terminalContainer->currentDocumentPath();
    if (!docPath.isEmpty())
        m_terminalContainer->termWidget()->changeDir(docPath);
}

void TerminalWindow::newTab()
{
    if (m_terminalContainer)
        m_terminalContainer->createTab();
}

void TerminalWindow::closeTab()
{
    if (m_terminalContainer)
        m_terminalContainer->closeCurrentTab();
}

void TerminalWindow::visibilityChanged(bool visible)
{
    static bool initialized = false;
    if (!m_terminalContainer || !m_terminalContainer->termWidget() || initialized || !visible)
        return;

    initialized = true;
}

void TerminalWindow::terminalFinished()
{
    m_terminalContainer->closeAllTabs();
}

void TerminalWindow::setFocus()
{
    if (!m_terminalContainer || !m_terminalContainer->termWidget())
        return;
    m_terminalContainer->setFocus(Qt::OtherFocusReason);
}

bool TerminalWindow::hasFocus() const
{
    if (!m_terminalContainer || !m_terminalContainer->termWidget())
        return false;
    return m_terminalContainer->hasFocus();
}

bool TerminalWindow::canFocus() const
{
    return true;
}

bool TerminalWindow::canNavigate() const
{
    return true;
}

bool TerminalWindow::canNext() const
{
    return true;
}

bool TerminalWindow::canPrevious() const
{
    return true;
}

void TerminalWindow::goToNext()
{
    if (!m_terminalContainer)
        return;

    m_terminalContainer->nextTab();
}

void TerminalWindow::goToPrev()
{
    if (!m_terminalContainer)
        return;

    m_terminalContainer->prevTab();
}

} // namespace Internal
} // namespace Terminal
