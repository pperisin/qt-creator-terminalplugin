/*
 * Copyright (C) 2017 Francois Ferrand. All rights reserved.
 */
#include "findsupport.h"
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>
#include <qtermwidget5/qtermwidget.h>

#include <QDebug>
using namespace Terminal::Internal;

FindSupport::FindSupport(QTermWidget * termWidget)
{
    setTerminal(termWidget);
}

void FindSupport::setTerminal(QTermWidget * termWidget)
{
    m_textEdit = termWidget->findChild<QLineEdit *>("searchTextEdit");
    m_findPreviousButton = termWidget->findChild<QToolButton *>("findPreviousButton");
    m_findNextButton = termWidget->findChild<QToolButton *>("findNextButton");

    auto options = termWidget->findChild<QToolButton *>("optionsButton")->menu()->actions();
    m_matchCaseAction = options[0];
    m_regexpAction = options[1];
    m_highlightAllAction = options[2];

    connect(m_textEdit->parentWidget(), SIGNAL(noMatchFound()), this, SLOT(noMatchFound()));
}

void FindSupport::noMatchFound()
{
    m_result = NotFound;
}

bool FindSupport::supportsReplace() const
{
    return false;
}

Core::FindFlags FindSupport::supportedFindFlags() const
{
    return Core::FindBackward | Core::FindCaseSensitively | Core::FindRegularExpression;
}

QString FindSupport::currentFindString() const
{
    return m_textEdit->text();
}

QString FindSupport::completedFindString() const
{
    return QString();
}

void FindSupport::clearHighlights()
{
    m_highlightAllAction->setChecked(false);
}

void FindSupport::resetIncrementalSearch()
{
    m_textEdit->clear();
}

void FindSupport::highlightAll(const QString & txt, Core::FindFlags findFlags)
{
    setupSearch(txt, findFlags);
    m_highlightAllAction->setChecked(true);
}

Core::IFindSupport::Result FindSupport::findIncremental(const QString & txt, Core::FindFlags findFlags)
{
    setupSearch(txt, findFlags);
    m_highlightAllAction->setChecked(false);
    m_result = Found;
    if (findFlags & Core::FindBackward)
        m_findPreviousButton->click();
    else
        m_findNextButton->click();
    return m_result;
}

Core::IFindSupport::Result FindSupport::findStep(const QString & txt, Core::FindFlags findFlags)
{
    setupSearch(txt, findFlags);
    m_result = Found;
    if (findFlags & Core::FindBackward)
        m_findPreviousButton->click();
    else
        m_findNextButton->click();
    return m_result;
}

void FindSupport::setupSearch(const QString & txt, Core::FindFlags findFlags)
{
    m_textEdit->setText(txt);
    m_matchCaseAction->setChecked(findFlags & Core::FindCaseSensitively);
    m_regexpAction->setChecked(findFlags & Core::FindRegularExpression);
}
