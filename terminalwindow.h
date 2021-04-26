/*
 * Copyright (C) 2012 Adam Treat. All rights reserved.
 */

#ifndef TERMINALWINDOW_H
#define TERMINALWINDOW_H

#include <coreplugin/outputwindow.h>
#include <coreplugin/ioutputpane.h>

QT_FORWARD_DECLARE_CLASS(QLabel);
QT_FORWARD_DECLARE_CLASS(QSettings);
QT_FORWARD_DECLARE_CLASS(QVBoxLayout);
QT_FORWARD_DECLARE_CLASS(QTermWidget);
QT_FORWARD_DECLARE_CLASS(QToolButton);
QT_FORWARD_DECLARE_CLASS(QTabWidget);

namespace Terminal {
namespace Internal {

class TerminalContainer : public QWidget
{
    Q_OBJECT

public:
    TerminalContainer(QWidget *parent);
    QTermWidget *initializeTerm(const QString &workingDirectory = QString());

    QTermWidget *termWidget();
    QString currentDocumentPath() const;
    void closeAllTerminals();
    void createTerminal();
    void nextTerminal();
    void prevTerminal();
    void closeCurrentTerminal();
    void setColorScheme(const QString &scheme);
    void fillContextMenu(QMenu *menu);

signals:
    void termWidgetChanged(QTermWidget * termWdiget);
    void finished();

private slots:
    void contextMenuRequested(const QPoint &);
    void copyAvailable(bool);
    void copyInvoked();
    void pasteInvoked();
    void closeTerminal();
    void increaseFont();
    void decreaseFont();
    void closeTerminalId(int index);
    void currentTabChanged(int index);
    void renameCurrentTerminal();
    void tabBarDoubleClick(int index);
    void moveTerminalLeft();
    void moveTerminalRight();

private:
    void setTabActions();
    void fillColorSchemeMenu(QMenu *menu);
    void renameTerminal(int index);

    QVBoxLayout *m_layout;
    QTabWidget  *m_tabWidget;
    QAction *m_copy;
    QAction *m_paste;
    QAction *m_increaseFont;
    QAction *m_decreaseFont;
    QAction *m_newTerminal;
    QAction *m_closeTerminal;
    QAction *m_renameTerminal;
    QAction *m_nextTerminal;
    QAction *m_prevTerminal;
    QAction *m_moveTerminalRight;
    QAction *m_moveTerminalLeft;
    QAction *m_closeAllTerminals;
    QString m_currentColorScheme;
};

class TerminalWindow : public Core::IOutputPane
{
    Q_OBJECT

public:
    TerminalWindow(QObject *parent = nullptr);

    // Pure virtual in IOutputPane
    virtual QWidget *outputWidget(QWidget *parent) override;
    virtual QList<QWidget *> toolBarWidgets() const override;
    virtual QString displayName() const override;
    virtual int priorityInStatusBar() const override;
    virtual void clearContents() override;
    virtual void setFocus() override;
    virtual bool hasFocus() const override;
    virtual bool canFocus() const override;
    virtual bool canNavigate() const override;
    virtual bool canNext() const override;
    virtual bool canPrevious() const override;
    virtual void goToNext() override;
    virtual void goToPrev() override;

private slots:
    void terminalFinished();
    void sync();
    void newTerminal();
    void setSettingsMenu();

private:
    Core::IContext *m_context;
    TerminalContainer *m_terminalContainer;
    QToolButton *m_sync;
    QToolButton *m_addTerminal;
    QToolButton *m_settings;
};

} // namespace Internal
} // namespace Terminal

#endif // TERMINALWINDOW_H
