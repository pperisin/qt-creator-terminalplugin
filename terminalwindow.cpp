/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#include "terminalwindow.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/icontext.h>
#include <extensionsystem/pluginmanager.h>
#include <texteditor/fontsettings.h>
#include <texteditor/texteditorsettings.h>
#include <utils/environment.h>
#include <utils/fileutils.h>
#include <utils/utilsicons.h>

#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

#include <qtermwidget5/qtermwidget.h>

namespace Terminal {
namespace Internal {

TerminalContainer::TerminalContainer(QWidget *parent)
    : QWidget(parent)
    , m_parent(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, &TerminalContainer::contextMenuRequested);

    m_copy = new QAction("Copy", this);
    m_copy->setShortcut(QKeySequence::Copy);
    m_copy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_copy, &QAction::triggered, this, &TerminalContainer::copyInvoked);
    addAction(m_copy);

    m_paste = new QAction("Paste", this);
    m_paste->setShortcut(QKeySequence::Paste);
    m_paste->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_paste, &QAction::triggered, this, &TerminalContainer::pasteInvoked);
    addAction(m_paste);

    m_close = new QAction("Close", this);
    m_close->setShortcut(QKeySequence::Close);
    m_close->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_close, &QAction::triggered, this, &TerminalContainer::closeInvoked);
    addAction(m_close);

    initializeTerm();
}

void TerminalContainer::initializeTerm(const QString & workingDirectory)
{
    if (m_termWidget) {
        delete m_layout;
        delete m_termWidget;
    }

    m_termWidget = new QTermWidget(0, this);
    m_termWidget->setWindowTitle(tr("Terminal"));
    m_termWidget->setWindowIcon(QIcon());
    m_termWidget->setScrollBarPosition(QTermWidget::ScrollBarRight);
    qDebug() << m_termWidget->availableColorSchemes();
#if defined(Q_OS_LINUX)
    m_termWidget->setColorScheme("Linux");
    m_termWidget->setKeyBindings("linux");
#elif defined(Q_OS_MACX)
    m_termWidget->setColorScheme("WhiteOnBlack");
    m_termWidget->setKeyBindings("macbook");
#else
    m_termWidget->setColorScheme("BlackOnLightYellow");
    m_termWidget->setKeyBindings("default");
#endif
    QFont font = TextEditor::TextEditorSettings::instance()->fontSettings().font();
    m_termWidget->setTerminalFont(font);
    m_termWidget->setTerminalOpacity(1.0);

    setFocusProxy(m_termWidget);

    m_layout = new QVBoxLayout;
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->addWidget(m_termWidget);
    setLayout(m_layout);

    connect(m_termWidget, &QTermWidget::copyAvailable, this, &TerminalContainer::copyAvailable);
    connect(m_termWidget, &QTermWidget::finished, this, &TerminalContainer::finished);

    m_termWidget->setWorkingDirectory(workingDirectory.isEmpty() ? QDir::homePath()
                                                                 : workingDirectory);
    Utils::Environment env = Utils::Environment::systemEnvironment();
    env.set("TERM_PROGRAM", QString("qtermwidget5"));
    env.set("TERM", QString("xterm-256color"));
    env.set("QTCREATOR_PID", QString("%1").arg(QCoreApplication::applicationPid()));
    m_termWidget->setEnvironment(env.toStringList());
    m_termWidget->startShellProgram();
}

void TerminalContainer::contextMenuRequested(const QPoint &point)
{
    QPoint globalPos = mapToGlobal(point);
    QMenu menu;
    menu.addAction(m_copy);
    menu.addAction(m_paste);
    menu.addAction(m_close);
    menu.exec(globalPos);
}

void TerminalContainer::copyInvoked()
{
    m_termWidget->copyClipboard();
}

void TerminalContainer::pasteInvoked()
{
    m_termWidget->pasteClipboard();
}

void TerminalContainer::copyAvailable(bool available)
{
    m_copy->setEnabled(available);
}

void TerminalContainer::closeInvoked()
{
    QString cmd = "exit\n";
    m_termWidget->sendText(cmd);
}

TerminalWindow::TerminalWindow(QObject *parent)
   : IOutputPane(parent)
   , m_terminalContainer(0)
{
    Core::Context context("Terminal.Window");

    m_sync = new QToolButton();
    m_sync->setEnabled(false);
    m_sync->setIcon(Utils::Icons::LINK.icon());
    m_sync->setToolTip(tr("Synchronize with Editor"));
    connect(m_sync, &QAbstractButton::clicked, this, &TerminalWindow::sync);
    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged,
            this, [this](Core::IEditor *editor) { m_sync->setEnabled(editor); });
}

QWidget *TerminalWindow::outputWidget(QWidget *parent)
{
    if (!m_terminalContainer) {
        m_terminalContainer = new TerminalContainer(parent);
        connect(m_terminalContainer, &TerminalContainer::finished,
                this, &TerminalWindow::terminalFinished);
    }
    return m_terminalContainer;
}

QList<QWidget *> TerminalWindow::toolBarWidgets() const
{
    return { m_sync };
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
    QString docPath = currentDocumentPath();
    if (!docPath.isEmpty())
        m_terminalContainer->termWidget()->changeDir(docPath);
}

void TerminalWindow::visibilityChanged(bool visible)
{
    static bool initialized = false;
    if (!m_terminalContainer || !m_terminalContainer->termWidget() || initialized || !visible)
        return;

    m_terminalContainer->initializeTerm(currentDocumentPath());
    initialized = true;
}

void TerminalWindow::terminalFinished()
{
    m_terminalContainer->initializeTerm(currentDocumentPath());
}

QString TerminalWindow::currentDocumentPath() const
{  
    if (Core::IDocument *doc = Core::EditorManager::instance()->currentDocument()) {
        const QDir dir = doc->filePath().toFileInfo().absoluteDir();
        if (dir.exists())
            return dir.canonicalPath();
    }
    return QString();
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
    return false;
}

bool TerminalWindow::canNext() const
{
    return false;
}

bool TerminalWindow::canPrevious() const
{
    return false;
}

void TerminalWindow::goToNext()
{
    // no-op
}

void TerminalWindow::goToPrev()
{
    // no-op
}

} // namespace Internal
} // namespace Terminal
