/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#include "terminalwindow.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/find/basetextfind.h>
#include <coreplugin/mainwindow.h>
#include <coreplugin/icontext.h>
#include <coreplugin/icore.h>
#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/projecttree.h>
#include <projectexplorer/project.h>
#include <projectexplorer/session.h>
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
#include <QLabel>
#include <QComboBox>
#include <QVector>
#include <QFileInfo>
#include <QDesktopServices>
#include <QGuiApplication>

#include <qtermwidget5/qtermwidget.h>
#include "findsupport.h"

namespace Terminal {
namespace Internal {

TerminalContainer::TerminalContainer(QWidget *parent, QComboBox *m_toolbarTerminalsComboBox)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_tabWidget(nullptr)
    , m_toolbarTerminalsComboBox(m_toolbarTerminalsComboBox)
{
    QCoreApplication::setOrganizationName("TermPlugin");
    QCoreApplication::setOrganizationDomain("TermPlugin");

    // Try to get the saved color scheme. If it doesn't exist, set the default and
    // update the saved scheme. If it does exist, use that to set the terminal's scheme.
    QSettings settings;
    QString savedColorScheme = settings.value("colorScheme").toString();
    bool hideTabs = settings.value("hideTabs", true).toBool();
    if (savedColorScheme.isEmpty()) {
#if defined(Q_OS_LINUX)
        m_currentColorScheme = "DarkPastels";
#elif defined(Q_OS_MACX)
        m_currentColorScheme = "WhiteOnBlack";
#else
        m_currentColorScheme = "BlackOnLightYellow";
#endif
        settings.setValue("colorScheme", m_currentColorScheme);
    } else {
        m_currentColorScheme = savedColorScheme;
    }

    if (!settings.contains("terminalFont"))
        settings.setValue("terminalFont", TextEditor::TextEditorSettings::instance()->fontSettings().font());

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, &TerminalContainer::contextMenuRequested);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(initializeTerm(), tr("terminal"));
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->tabBar()->setHidden(hideTabs);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            this, &TerminalContainer::closeTerminalId);

    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &TerminalContainer::currentTabChanged);

    connect(m_tabWidget, &QTabWidget::tabBarDoubleClicked,
            this, &TerminalContainer::tabBarDoubleClick);

    m_layout = new QVBoxLayout;
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addWidget(m_tabWidget);
    setLayout(m_layout);

    m_openSelection = new QAction("Open Selected File", this);
    addAction(m_openSelection);
    m_openSelection->setShortcut(QKeySequence(tr("Ctrl+Alt+O")));
    m_openSelection->setShortcutVisibleInContextMenu(true);
    m_openSelection->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_openSelection, &QAction::triggered, this, &TerminalContainer::openSelectedFile);

    m_showHideTabs = new QAction(this);
    m_showHideTabs->setText(hideTabs ? tr("Show Tabs") : tr("Hide Tabs"));
    connect(m_showHideTabs, &QAction::triggered, this, &TerminalContainer::toggleShowTabs);

    m_copy = new QAction("Copy", this);
    addAction(m_copy);
    m_copy->setShortcut(QKeySequence(tr("Ctrl+Shift+C")));
    m_copy->setShortcutVisibleInContextMenu(true);
    m_copy->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_copy, &QAction::triggered, this, &TerminalContainer::copyInvoked);

    m_paste = new QAction("Paste", this);
    addAction(m_paste);
    m_paste->setShortcut(QKeySequence(tr("Ctrl+Shift+V")));
    m_paste->setShortcutVisibleInContextMenu(true);
    m_paste->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_paste, &QAction::triggered, this, &TerminalContainer::pasteInvoked);

    m_increaseFont = new QAction("Increase Font", this);
    addAction(m_increaseFont);
    m_increaseFont->setShortcut(QKeySequence(tr("Ctrl++")));
    m_increaseFont->setShortcutVisibleInContextMenu(true);
    m_increaseFont->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_increaseFont, &QAction::triggered, this, &TerminalContainer::increaseFont);

    m_decreaseFont = new QAction("Decrease Font", this);
    addAction(m_decreaseFont);
    m_decreaseFont->setShortcut(QKeySequence(tr("Ctrl+-")));
    m_decreaseFont->setShortcutVisibleInContextMenu(true);
    m_decreaseFont->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_decreaseFont, &QAction::triggered, this, &TerminalContainer::decreaseFont);

    m_newTerminal = new QAction("New Terminal", this);
    addAction(m_newTerminal);
    m_newTerminal->setShortcut(QKeySequence(tr("Ctrl+Shift+T")));
    m_newTerminal->setShortcutVisibleInContextMenu(true);
    m_newTerminal->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_newTerminal, &QAction::triggered, this, &TerminalContainer::createTerminal);

    m_closeTerminal = new QAction("Close Terminal", this);
    addAction(m_closeTerminal);
    m_closeTerminal->setShortcut(QKeySequence(tr("Ctrl+Shift+D")));
    m_closeTerminal->setShortcutVisibleInContextMenu(true);
    m_closeTerminal->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_closeTerminal, &QAction::triggered, this, &TerminalContainer::closeTerminal);

    m_renameTerminal = new QAction("Rename Terminal", this);
    addAction(m_renameTerminal);
    m_renameTerminal->setShortcut(QKeySequence(tr("F2")));
    m_renameTerminal->setShortcutVisibleInContextMenu(true);
    m_renameTerminal->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_renameTerminal, &QAction::triggered, this, &TerminalContainer::renameCurrentTerminal);

    m_nextTerminal = new QAction("Next Terminal", this);
    addAction(m_nextTerminal);
    m_nextTerminal->setShortcut(QKeySequence(tr("Ctrl+PgDown")));
    m_nextTerminal->setShortcutVisibleInContextMenu(true);
    m_nextTerminal->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_nextTerminal, &QAction::triggered, this, &TerminalContainer::nextTerminal);

    m_prevTerminal = new QAction("Previous Terminal", this);
    addAction(m_prevTerminal);
    m_prevTerminal->setShortcut(QKeySequence(tr("Ctrl+PgUp")));
    m_prevTerminal->setShortcutVisibleInContextMenu(true);
    m_prevTerminal->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_prevTerminal, &QAction::triggered, this, &TerminalContainer::prevTerminal);

    m_moveTerminalRight = new QAction("Move Terminal Right", this);
    addAction(m_moveTerminalRight);
    m_moveTerminalRight->setShortcut(QKeySequence(tr("Ctrl+Shift+PgDown")));
    m_moveTerminalRight->setShortcutVisibleInContextMenu(true);
    m_moveTerminalRight->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_moveTerminalRight, &QAction::triggered, this, &TerminalContainer::moveTerminalRight);

    m_moveTerminalLeft = new QAction("Move Terminal Left", this);
    addAction(m_moveTerminalLeft);
    m_moveTerminalLeft->setShortcut(QKeySequence(tr("Ctrl+Shift+PgUp")));
    m_moveTerminalLeft->setShortcutVisibleInContextMenu(true);
    m_moveTerminalLeft->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_moveTerminalLeft, &QAction::triggered, this, &TerminalContainer::moveTerminalLeft);

    m_closeAllTerminals = new QAction("Close All Terminals", this);
    addAction(m_closeAllTerminals);
    m_closeAllTerminals->setShortcutContext(Qt::ShortcutContext::WidgetWithChildrenShortcut);
    connect(m_closeAllTerminals, &QAction::triggered, this, &TerminalContainer::closeAllTerminals);

    m_colorSchemes = new QMenu("Color Schemes", this);
    fillColorSchemeMenu();
    setTabActions();
    notifyTabsUpdated();
}

QTermWidget* TerminalContainer::initializeTerm(const QString & workingDirectory)
{
    QTermWidget *termWidget = new QTermWidget(0, this);
    termWidget->setWindowTitle(tr("Terminal"));
    termWidget->setWindowIcon(QIcon());
    termWidget->setScrollBarPosition(QTermWidget::ScrollBarRight);

    termWidget->setColorScheme(m_currentColorScheme);

    QSettings settings;
    QFont font = settings.value("terminalFont", QFont()).value<QFont>();
    termWidget->setTerminalFont(font);
    termWidget->setTerminalOpacity(1.0);

    setFocusProxy(termWidget);

    connect(termWidget, &QTermWidget::copyAvailable, this, &TerminalContainer::copyAvailable);
    connect(termWidget, &QTermWidget::finished, this, &TerminalContainer::finished);
    connect(termWidget, &QTermWidget::urlActivated, this, &TerminalContainer::urlActivated);

    termWidget->setWorkingDirectory(workingDirectory.isEmpty() ? QDir::homePath()
                                                               : workingDirectory);

    Utils::Environment env = Utils::Environment::systemEnvironment();
    env.set("TERM_PROGRAM", QString("qtermwidget5"));
    env.set("TERM", QString("xterm-256color"));
    env.set("QTCREATOR_PID", QString("%1").arg(QCoreApplication::applicationPid()));
    termWidget->setEnvironment(env.toStringList());
    termWidget->startShellProgram();
    termWidget->setBlinkingCursor(true);
//  termWidget->setConfirmMultilinePaste(false);

    return termWidget;
}

void TerminalContainer::setTabActions()
{
    bool enable = m_tabWidget->count() > 1;

    m_nextTerminal->setEnabled(enable);
    m_prevTerminal->setEnabled(enable);
    m_moveTerminalRight->setEnabled(enable);
    m_moveTerminalLeft->setEnabled(enable);
}

void TerminalContainer::closeAllTerminals()
{
    for (int i = m_tabWidget->count(); i > 0; i--)
    {
        m_tabWidget->widget(i-1)->deleteLater();
        m_tabWidget->removeTab(i-1);
    }

    createTerminal();
    setTabActions();
    notifyTabsUpdated();
}

void TerminalContainer::closeCurrentTerminal()
{
    if (m_tabWidget->currentIndex() < 0)
        return;

    m_tabWidget->removeTab(m_tabWidget->currentIndex());

    if (m_tabWidget->count() == 0)
        createTerminal();

    setTabActions();
    notifyTabsUpdated();
}

void TerminalContainer::closeTerminalId(int index)
{
    m_tabWidget->widget(index)->deleteLater();
    m_tabWidget->removeTab(index);

    if (m_tabWidget->count() == 0)
        createTerminal();

    m_tabWidget->setFocus();
    m_tabWidget->currentWidget()->setFocus();
    setTabActions();
    notifyTabsUpdated();
}

void TerminalContainer::currentTabChanged(int index)
{
    if (index <0 || index >= m_tabWidget->count())
        return;

    notifyTabsUpdated();
    emit termWidgetChanged(termWidget());
}

void TerminalContainer::contextMenuRequested(const QPoint &point)
{
    QMenu menu;

    fillContextMenu(&menu);

    menu.exec(mapToGlobal(point));
}

QFileInfo TerminalContainer::getSelectedFilePath()
{
    QString selectedText = termWidget()->selectedText(false).trimmed();
    QFileInfo file(selectedText);

    if (file.exists() && !file.isDir())
        return file;

    QString dir = termWidget()->workingDirectory();
    file = QFileInfo(QDir(dir), selectedText);

    if (file.exists() && !file.isDir())
        return file;

    ProjectExplorer::Project *project = ProjectExplorer::ProjectTree::currentProject();

    if (!project)
        return QFileInfo();

    Utils::FilePaths fileList = project->files(ProjectExplorer::Project::AllFiles);

    for (Utils::FilePath& projectsFile : fileList)
        if (projectsFile.endsWith(selectedText))
            return QFileInfo(projectsFile.toString());

    for (ProjectExplorer::Project *nonCurrentProj : ProjectExplorer::SessionManager::projects())
        if (project != nonCurrentProj)
            for (Utils::FilePath& projectsFile : nonCurrentProj->files(ProjectExplorer::Project::AllFiles))
                if (projectsFile.endsWith(selectedText))
                    return QFileInfo(projectsFile.toString());

    return QFileInfo();
}

void TerminalContainer::fillContextMenu(QMenu *menu)
{
    QFileInfo file = getSelectedFilePath();

    if (file.exists()) {
        m_openSelection->setText("Open \"" + file.fileName() + "\"");
        menu->addAction(m_openSelection);
        menu->addSeparator();
    }

    menu->addAction(m_showHideTabs);
    menu->addMenu(m_colorSchemes);
    menu->addSeparator();
    menu->addAction(m_copy);
    menu->addAction(m_paste);
    menu->addSeparator();
    menu->addAction(m_increaseFont);
    menu->addAction(m_decreaseFont);
    menu->addSeparator();
    menu->addAction(m_newTerminal);
    menu->addAction(m_closeTerminal);
    menu->addAction(m_renameTerminal);
    menu->addSeparator();
    menu->addAction(m_nextTerminal);
    menu->addAction(m_prevTerminal);
    menu->addAction(m_moveTerminalRight);
    menu->addAction(m_moveTerminalLeft);
    menu->addSeparator();
    menu->addAction(m_closeAllTerminals);
}

void TerminalContainer::fillColorSchemeMenu()
{
    QStringList schemes = termWidget()->availableColorSchemes();
    schemes.sort();
    for(QString &scheme : schemes) {
        QAction *action = new QAction(scheme, m_colorSchemes);
        if(scheme == m_currentColorScheme) {
            QFont font = action->font();
            font.setBold(true);
            font.setItalic(true);
            action->setFont(font);
        }
        connect(action, &QAction::triggered, this, [=] {
            m_currentColorScheme = scheme;
            setColorScheme(scheme);
            QSettings settings;
            settings.setValue("colorScheme", m_currentColorScheme);

            for (QAction *listAction : m_colorSchemes->actions()) {
                QFont font = listAction->font();
                font.setBold(listAction->text() == m_currentColorScheme);
                font.setItalic(listAction->text() == m_currentColorScheme);
                listAction->setFont(font);
            }
        });
        m_colorSchemes->addAction(action);
    }
}

void TerminalContainer::notifyTabsUpdated()
{
    QList<QString> tabs;
    for (int i = 0; i < m_tabWidget->count(); i++)
        tabs.append(m_tabWidget->tabText(i));

    emit tabsUpdated(m_tabWidget->currentIndex(), tabs);
}

void TerminalContainer::openSelectedFile()
{
    QFileInfo file = getSelectedFilePath();

    if (file.exists()) {
        Core::ICore::openFiles(QStringList() << file.absoluteFilePath(),
                               Core::ICore::SwitchMode);
    }
}

void TerminalContainer::toggleShowTabs()
{
    bool hide = !m_tabWidget->tabBar()->isHidden();
    m_tabWidget->tabBar()->setHidden(hide);
    m_showHideTabs->setText(hide ? tr("Show Tabs") : tr("Hide Tabs"));
    QSettings settings;
    settings.setValue("hideTabs", hide);
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

void TerminalContainer::urlActivated(const QUrl& url, bool)
{
    if (QGuiApplication::keyboardModifiers() & Qt::ControlModifier)
        QDesktopServices::openUrl(url);
}

void TerminalContainer::increaseFont()
{
    QFont termFont = termWidget()->getTerminalFont();

    if (termFont.pointSize() == -1 || termFont.pointSize() >= 32)
        return;

    termFont.setPointSize(termFont.pointSize() + 1);

    for (int i = 0; i < m_tabWidget->count(); i++) {
        QTermWidget *term = static_cast<QTermWidget *>(m_tabWidget->widget(i));
        term->setTerminalFont(termFont);
    }

    QSettings settings;
    settings.setValue("terminalFont", termFont);
}

void TerminalContainer::decreaseFont()
{
    QFont termFont = termWidget()->getTerminalFont();

    if (termFont.pointSize() <= 6)
        return;

    termFont.setPointSize(termFont.pointSize() - 1);

    for (int i = 0; i < m_tabWidget->count(); i++) {
        QTermWidget *term = static_cast<QTermWidget *>(m_tabWidget->widget(i));
        term->setTerminalFont(termFont);
    }

    QSettings settings;
    settings.setValue("terminalFont", termFont);
}

void TerminalContainer::closeTerminal()
{
    closeTerminalId(m_tabWidget->currentIndex());
}

void TerminalContainer::renameCurrentTerminal()
{
    renameTerminal(m_tabWidget->currentIndex());
}

void  TerminalContainer::tabBarDoubleClick(int index)
{
    if (index < 0 || index >= m_tabWidget->count())
        createTerminal();
    else
        renameTerminal(index);
}

void TerminalContainer::renameTerminal(int index)
{
    if (index < 0 || index >= m_tabWidget->count())
        return;

    QLineEdit *lineEdit;
    if (m_tabWidget->tabBar()->isHidden())
    {
        lineEdit = new QLineEdit(m_toolbarTerminalsComboBox);
        lineEdit->setGeometry(m_toolbarTerminalsComboBox->rect());
    }
    else
    {
        lineEdit = new QLineEdit(m_tabWidget);
        lineEdit->setGeometry(m_tabWidget->tabBar()->tabRect(index));
    }
    lineEdit->setText(m_tabWidget->tabText(index));
    lineEdit->selectAll();
    int lineEditWidth = lineEdit->size().width();

    connect(lineEdit, &QLineEdit::returnPressed,
            this, [this, lineEdit, index]() {
        lineEdit->deleteLater();
        if (lineEdit->text().isEmpty())
            return;

        m_tabWidget->setTabText(index, lineEdit->text());
        m_tabWidget->currentWidget()->setFocus();
        notifyTabsUpdated();
    });

    connect(lineEdit, &QLineEdit::editingFinished,
            this, [this, lineEdit]() {
        if (m_tabWidget->underMouse())
            m_tabWidget->currentWidget()->setFocus();
        lineEdit->deleteLater();
    });

    connect(lineEdit, &QLineEdit::textEdited,
            this, [lineEdit, lineEditWidth](const QString &text) {
        QFontMetrics fm(lineEdit->font());

        // not sure how to get platform specific pixel difference between
        // QLineEdit edge and text inside it, so use fixed 12 pixels for now
        int pixelsWide = fm.horizontalAdvance(text) + 12;

        if (lineEditWidth < pixelsWide)
            lineEdit->setFixedSize(pixelsWide, lineEdit->height());
    });

    lineEdit->show();
    lineEdit->setFocus();
}

void TerminalContainer::createTerminal()
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
    m_tabWidget->currentWidget()->setFocus();
    setTabActions();
    notifyTabsUpdated();

    emit termWidgetChanged(termWidget());
}

void TerminalContainer::nextTerminal()
{
    if (m_tabWidget->count() <= 1)
        return;

    int nextIndex = m_tabWidget->currentIndex() + 1;

    if (nextIndex >= m_tabWidget->count())
        nextIndex = 0;

    m_tabWidget->setCurrentIndex(nextIndex);
    m_tabWidget->currentWidget()->setFocus();
    notifyTabsUpdated();

    emit termWidgetChanged(termWidget());
}

void TerminalContainer::prevTerminal()
{
    if (m_tabWidget->count() <= 1)
        return;

    int prevIndex = m_tabWidget->currentIndex() - 1;

    if (prevIndex < 0)
        prevIndex = m_tabWidget->count() - 1;

    m_tabWidget->setCurrentIndex(prevIndex);
    m_tabWidget->currentWidget()->setFocus();
    notifyTabsUpdated();

    emit termWidgetChanged(termWidget());
}

void TerminalContainer::moveTerminalLeft()
{
    if (m_tabWidget->currentIndex() < 0)
        return;

    int currentIndex = m_tabWidget->currentIndex();
    int nextIndex = currentIndex ? currentIndex - 1 : m_tabWidget->count() - 1;

    m_tabWidget->tabBar()->moveTab(currentIndex, nextIndex);
    m_tabWidget->setCurrentIndex(nextIndex);
    m_tabWidget->currentWidget()->setFocus();
    notifyTabsUpdated();
}

void TerminalContainer::moveTerminalRight()
{
    if (m_tabWidget->currentIndex() < 0)
        return;

    int currentIndex = m_tabWidget->currentIndex();
    int nextIndex = (currentIndex + 1 == m_tabWidget->count()) ? 0 : currentIndex + 1;

    m_tabWidget->tabBar()->moveTab(currentIndex, nextIndex);
    m_tabWidget->setCurrentIndex(nextIndex);
    m_tabWidget->currentWidget()->setFocus();
    notifyTabsUpdated();
}

void TerminalContainer::setColorScheme(const QString &scheme)
{
    m_currentColorScheme = scheme;
    for (int i = 0; i < m_tabWidget->count(); i++) {
        QTermWidget *widget =  static_cast<QTermWidget *>(m_tabWidget->widget(i));
        if (widget)
            widget->setColorScheme(m_currentColorScheme);
    }
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
        createTerminal();

    return static_cast<QTermWidget *>(m_tabWidget->currentWidget());
}

void TerminalContainer::setCurrentIndex(int index)
{
    m_tabWidget->setCurrentIndex(index);
    m_tabWidget->currentWidget()->setFocus();
    emit termWidgetChanged(termWidget());
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

    m_settings = new QToolButton();
    m_settings->setIcon(Utils::Icons::SETTINGS_TOOLBAR.icon());
    m_settings->setEnabled(true);
    m_settings->setToolTip(tr("Settings"));

    m_terminalsBox = new QComboBox();
    m_terminalsBox->setProperty("drawleftborder", true);
    m_terminalsBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_terminalsBox->setMinimumWidth(200);
    m_terminalsBox->addItem(tr("1: terminal"));

    m_addTerminal = new QToolButton();
    m_addTerminal->setIcon(Utils::Icons::FITTOVIEW_TOOLBAR.icon());
    m_addTerminal->setEnabled(true);
    m_addTerminal->setToolTip(tr("Create New Terminal"));

    m_removeTerminal = new QToolButton();
    m_removeTerminal->setIcon(Utils::Icons::CLOSE_TOOLBAR.icon());
    m_removeTerminal->setEnabled(true);
    m_removeTerminal->setToolTip(tr("Remove Current Terminal"));

    m_spacer1 = new QWidget();
    m_spacer2 = new QWidget();
    m_spacer1->setMinimumWidth(30);
    m_spacer2->setMinimumWidth(5);
    m_terminalsLabel = new QLabel(tr("Terminals:"));
}

TerminalWindow::~TerminalWindow()
{
    delete m_settings;
    delete m_sync;
    delete m_terminalContainer;
    delete m_context;
    delete m_addTerminal;
    delete m_removeTerminal;
    delete m_spacer1;
    delete m_spacer2;
    delete m_terminalsLabel;
}

QWidget *TerminalWindow::outputWidget(QWidget *parent)
{
    if (!m_terminalContainer) {
        m_terminalContainer = new TerminalContainer(parent, m_terminalsBox);
        connect(m_terminalContainer, &TerminalContainer::finished,
                this, &TerminalWindow::terminalFinished);

        m_context->setWidget(m_terminalContainer->termWidget());
        Core::ICore::addContextObject(m_context);

        auto findSupport = new FindSupport(m_terminalContainer->termWidget());
        connect(m_terminalContainer, &TerminalContainer::termWidgetChanged,
                findSupport, &FindSupport::setTerminal);

        connect(m_terminalsBox, QOverload<int>::of(&QComboBox::activated),
                m_terminalContainer, &TerminalContainer::setCurrentIndex);

        connect(m_addTerminal, &QAbstractButton::clicked,
                m_terminalContainer, &TerminalContainer::createTerminal);

        connect(m_removeTerminal, &QAbstractButton::clicked,
                m_terminalContainer, &TerminalContainer::closeTerminal);

        connect(m_terminalContainer, &TerminalContainer::tabsUpdated,
                this, &TerminalWindow::tabsUpdated);

        connect(this, &TerminalWindow::zoomInRequested,
                m_terminalContainer, &TerminalContainer::increaseFont);

        connect(this, &TerminalWindow::zoomOutRequested,
                m_terminalContainer, &TerminalContainer::decreaseFont);

        Aggregation::Aggregate *agg = new Aggregation::Aggregate;
        agg->add(m_terminalContainer);
        agg->add(findSupport);

        QMenu *menu = new QMenu(m_settings);
        m_terminalContainer->fillContextMenu(menu);
        m_settings->setMenu(menu);
        m_settings->setPopupMode(QToolButton::InstantPopup);
    }
    return m_terminalContainer;
}

QList<QWidget *> TerminalWindow::toolBarWidgets() const
{
    return IOutputPane::toolBarWidgets() + QList<QWidget *>{ m_sync, m_settings, m_spacer1, m_terminalsLabel, m_spacer2,
                m_terminalsBox , m_addTerminal, m_removeTerminal};
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
    if (!docPath.isEmpty()) {
        m_terminalContainer->termWidget()->changeDir(docPath);
        m_terminalContainer->termWidget()->setFocus();
    }
}

void TerminalWindow::tabsUpdated(int currentIndex, QList<QString> tabNames)
{
    m_terminalsBox->clear();

    for(int i = 0; i < tabNames.count(); i++)
        m_terminalsBox->addItem(QString("%1: %2").arg(QString::number(i + 1), tabNames[i]));

    m_terminalsBox->setCurrentIndex(currentIndex);
}


void TerminalWindow::terminalFinished()
{
    m_terminalContainer->closeAllTerminals();
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

    m_terminalContainer->nextTerminal();
}

void TerminalWindow::goToPrev()
{
    if (!m_terminalContainer)
        return;

    m_terminalContainer->prevTerminal();
}

} // namespace Internal
} // namespace Terminal
