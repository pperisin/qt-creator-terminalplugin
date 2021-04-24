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
    TerminalContainer(QWidget *parent, const QString &colorScheme);
    QTermWidget *initializeTerm(const QString &workingDirectory = QString());

    QTermWidget *termWidget();
    QString currentDocumentPath() const;
    void closeAllTabs();
    void createTab();
    void nextTab();
    void prevTab();
    void closeCurrentTab();
    void setColorScheme(const QString &scheme);

signals:
    void termWidgetChanged(QTermWidget * termWdiget);
    void finished();

private slots:
    void contextMenuRequested(const QPoint &);
    void copyAvailable(bool);
    void copyInvoked();
    void pasteInvoked();
    void closeInvoked();
    void closeTab(int tabIndex);
    void currentTabChanged(int tabIndex);
    void renameCurrentTab();
    void renameTabId(int index);
    void tabBarDoubleClick(int index);
    void moveTabLeft();
    void moveTabRight();

private:
    QVBoxLayout *m_layout;
    QTabWidget  *m_tabWidget;
    QAction *m_copy;
    QAction *m_paste;
    QAction *m_closeAll;
    QAction *m_newTab;
    QAction *m_renameTab;
    QWidget *m_parent;
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
    void newTab();
    void colorScheme();

private:
    Core::IContext *m_context;
    TerminalContainer *m_terminalContainer;
    QToolButton *m_sync;
    QToolButton *m_addTab;
    QToolButton *m_colorScheme;
    QString m_currentColorScheme;
};

} // namespace Internal
} // namespace Terminal

#endif // TERMINALWINDOW_H
