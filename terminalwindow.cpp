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

#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVector>

#include <qtermwidget5/qtermwidget.h>
#include "findsupport.h"

namespace Terminal {
namespace Internal {

TerminalContainer::TerminalContainer(QWidget *parent)
    : QWidget(parent)
    , m_parent(parent)
{
	QCoreApplication::setOrganizationName("TermPlugin");
	QCoreApplication::setOrganizationDomain("TermPlugin");

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, &TerminalContainer::contextMenuRequested);

    Core::Context context("Terminal.Window");

    m_copy = new QAction("Copy", this);
    Core::ActionManager::registerAction(m_copy, "Terminal.Copy", context)->setDefaultKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C);
    connect(m_copy, &QAction::triggered, this, &TerminalContainer::copyInvoked);

    m_paste = new QAction("Paste", this);
    Core::ActionManager::registerAction(m_paste, "Terminal.Paste", context)->setDefaultKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V);
    connect(m_paste, &QAction::triggered, this, &TerminalContainer::pasteInvoked);

    // The equivalent of this can be "invoked" with EOF (Ctrl+D) in the controlled terminal
    m_close = new QAction("Close", this);
    Core::ActionManager::registerAction(m_close, "Terminal.Close", context);
    connect(m_close, &QAction::triggered, this, &TerminalContainer::closeInvoked);

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

	// Try to get the saved color scheme. If it doesn't exist, set the default and
	// update the saved scheme. If it does exist, use that to set the terminal's scheme.
	QSettings settings;
	QString savedColorScheme = settings.value("colorScheme").toString();
	if(savedColorScheme.isEmpty()) {
		#if defined(Q_OS_LINUX)
			currentColorScheme = "DarkPastels";
			m_termWidget->setKeyBindings("linux");
		#elif defined(Q_OS_MACX)
			currentColorScheme = "WhiteOnBlack";
			m_termWidget->setKeyBindings("macbook");
		#else
			currentColorScheme = "BlackOnLightYellow";
			m_termWidget->setKeyBindings("default");
		#endif
		settings.setValue("colorScheme", currentColorScheme);
	}
	else {
		currentColorScheme = savedColorScheme;
	}
	m_termWidget->setColorScheme(currentColorScheme);

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

    emit termWidgetChanged(m_termWidget);
}

void TerminalContainer::contextMenuRequested(const QPoint &point)
{
    QPoint globalPos = mapToGlobal(point);
    QMenu menu;
	QMenu themesSubMenu("Color Schemes");
	QVector<QAction*> actions;

    menu.addAction(m_copy);
    menu.addAction(m_paste);

	// Get all available color schemes and create a dynamic list of actions corresponding
	// to each. Add to submenu. Connect a lambda function to it that will set the theme
	// when the submenu item is chosen.
	if(m_termWidget) {
		for(QString scheme : m_termWidget->availableColorSchemes()) {
			actions.push_back(new QAction(scheme, this));
			if(scheme == currentColorScheme) {
				QFont font = actions.back()->font();
				font.setBold(true);
				font.setItalic(true);
				actions.back()->setFont(font);
			}
			connect(actions.back(), &QAction::triggered, this, [=]{
				currentColorScheme = scheme;
				m_termWidget->setColorScheme(currentColorScheme);
				QSettings settings;
				settings.setValue("colorScheme", currentColorScheme);
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
   , m_context(new Core::IContext(this))
   , m_terminalContainer(0)
{
    Core::Context context("Terminal.Window");
    m_context->setContext(context);

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
