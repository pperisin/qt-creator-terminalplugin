/*
 * Copyright (C) 2017 Francois Ferrand. All rights reserved.
 */
#ifndef FINDSUPPORT_H
#define FINDSUPPORT_H

#include <coreplugin/find/ifindsupport.h>

QT_FORWARD_DECLARE_CLASS(QAction)
QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QTermWidget)
QT_FORWARD_DECLARE_CLASS(QToolButton)

namespace Terminal {
namespace Internal {

class FindSupport : public Core::IFindSupport
{
    Q_OBJECT

public:
    FindSupport(QTermWidget * termWidget);

    virtual bool supportsReplace() const override;
    virtual Core::FindFlags supportedFindFlags() const override;
    virtual QString currentFindString() const override;
    virtual QString completedFindString() const override;

    virtual void clearHighlights() override;
    virtual void highlightAll(const QString &txt, Core::FindFlags findFlags) override;

    virtual void resetIncrementalSearch() override;
    virtual Result findIncremental(const QString &txt, Core::FindFlags findFlags) override;
    virtual Result findStep(const QString &txt, Core::FindFlags findFlags) override;

public slots:
    void setTerminal(QTermWidget * termWidget);

private:
    void setupSearch(const QString &txt, Core::FindFlags findFlags);

    QLineEdit * m_textEdit;
    QToolButton * m_findPreviousButton;
    QToolButton * m_findNextButton;
    QAction * m_matchCaseAction;
    QAction * m_regexpAction;
    QAction * m_highlightAllAction;
    Result m_result;
};

} // namespace Internal
} // namespace Terminal

#endif // FINDSUPPORT_H
