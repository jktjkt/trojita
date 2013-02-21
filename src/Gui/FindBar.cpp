/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2008-2012 by Andrea Diamantini <adjam7 at gmail dot com>
* Copyright (C) 2009-2011 by Lionel Chauvin <megabigbug@yahoo.fr>
*
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License or (at your option) version 3 or any later version
* accepted by the membership of KDE e.V. (or its successor approved
* by the membership of KDE e.V.), which shall act as a proxy
* defined in Section 14 of version 3 of the license.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* ============================================================ */


// Self Includes
#include "findbar.h"
#include "findbar.moc"

// Local Includes
#include "webtab.h"
#include "webpage.h"
#include "webwindow.h"

// KDE Includes
#include <KApplication>
#include <KIcon>
#include <KLineEdit>
#include <KLocalizedString>
#include <KPushButton>
#include <KColorScheme>

// Qt Includes
#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QToolButton>
#include <QWebFrame>


FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
    , m_lineEdit(new KLineEdit(this))
    , m_matchCase(new QCheckBox(i18n("&Match case"), this))
    , m_highlightAll(new QCheckBox(i18n("&Highlight all"), this))
{
    QHBoxLayout *layout = new QHBoxLayout;

    // cosmetic
    layout->setContentsMargins(2, 0, 2, 0);

    // hide button
    QToolButton *hideButton = new QToolButton(this);
    hideButton->setAutoRaise(true);
    hideButton->setIcon(KIcon("dialog-close"));
    connect(hideButton, SIGNAL(clicked()), this, SLOT(hide()));
    layout->addWidget(hideButton);
    layout->setAlignment(hideButton, Qt::AlignLeft | Qt::AlignTop);

    // label
    QLabel *label = new QLabel(i18n("Find:"));
    layout->addWidget(label);

    // Find Bar signal
    connect(this, SIGNAL(searchString(QString)), this, SLOT(find(QString)));

    // lineEdit, focusProxy
    setFocusProxy(m_lineEdit);
    m_lineEdit->setMaximumWidth(250);
    connect(m_lineEdit, SIGNAL(textChanged(QString)), this, SLOT(find(QString)));
    layout->addWidget(m_lineEdit);

    // buttons
    KPushButton *findNext = new KPushButton(KIcon("go-down"), i18n("&Next"), this);
    KPushButton *findPrev = new KPushButton(KIcon("go-up"), i18n("&Previous"), this);
    connect(findNext, SIGNAL(clicked()), this, SLOT(findNext()));
    connect(findPrev, SIGNAL(clicked()), this, SLOT(findPrevious()));
    layout->addWidget(findNext);
    layout->addWidget(findPrev);

    // Case sensitivity. Deliberately set so this is off by default.
    m_matchCase->setCheckState(Qt::Unchecked);
    m_matchCase->setTristate(false);
    connect(m_matchCase, SIGNAL(toggled(bool)), this, SLOT(matchCaseUpdate()));
    layout->addWidget(m_matchCase);

    // Hightlight All. On by default
    m_highlightAll->setCheckState(Qt::Checked);
    m_highlightAll->setTristate(false);
    connect(m_highlightAll, SIGNAL(toggled(bool)), this, SLOT(updateHighlight()));
    layout->addWidget(m_highlightAll);

    // stretching widget on the left
    layout->addStretch();

    setLayout(layout);

    // we start off hidden
    hide();
}


void FindBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (event->modifiers() == Qt::ShiftModifier)
        {
            findPrevious();
        }
        else
        {
            findNext();
        }
    }

    QWidget::keyPressEvent(event);
}


bool FindBar::matchCase() const
{
    return m_matchCase->isChecked();
}


bool FindBar::highlightAllState() const
{
    return m_highlightAll->isChecked();
}


void FindBar::setVisible(bool visible)
{
    // parent webwindow
    WebWindow *w = qobject_cast<WebWindow *>(parent());

    if (visible
            && w->page()->isOnRekonqPage()
            && w->tabView()->part() != 0)
    {
        // findNext is the slot containing part integration code
        findNext();
        return;
    }

    QWidget::setVisible(visible);

    if (visible)
    {
        const QString selectedText = w->page()->selectedText();
        if (!hasFocus() && !selectedText.isEmpty())
        {
            const QString previousText = m_lineEdit->text();
            m_lineEdit->setText(selectedText);

            if (m_lineEdit->text() != previousText)
                findPrevious();
            else
                updateHighlight();
        }
        else if (selectedText.isEmpty())
        {
            emit searchString(m_lineEdit->text());
        }

        m_lineEdit->setFocus();
        m_lineEdit->selectAll();
    }
    else
    {
        updateHighlight();
    }
}


void FindBar::notifyMatch(bool match)
{
    QPalette p = m_lineEdit->palette();
    KColorScheme colorScheme(p.currentColorGroup());

    if (m_lineEdit->text().isEmpty())
    {
        p.setColor(QPalette::Base, colorScheme.background(KColorScheme::NormalBackground).color());
    }
    else
    {
        if (match)
        {
            p.setColor(QPalette::Base, colorScheme.background(KColorScheme::PositiveBackground).color());
        }
        else
        {
            p.setColor(QPalette::Base, colorScheme.background(KColorScheme::NegativeBackground).color()); // previous were 247, 230, 230
        }
    }
    m_lineEdit->setPalette(p);
}



// FIND RELATED -------------------------------------------------------------------------------------
void FindBar::find(const QString & search)
{
    _lastStringSearched = search;

    updateHighlight();
    findNext();
}


void FindBar::findNext()
{
    // parent webwindow
    WebWindow *w = qobject_cast<WebWindow *>(parent());

    if (w->page()->isOnRekonqPage())
    {
        // trigger part find action
        KParts::ReadOnlyPart *p = w->tabView()->part();
        if (p)
        {
//             connect(this, SIGNAL(triggerPartFind()), p, SLOT(slotFind()));
//             emit triggerPartFind();
            return;
        }
    }

    if (isHidden())
    {
        QPoint previous_position = w->page()->currentFrame()->scrollPosition();
        w->page()->focusNextPrevChild(true);
        w->page()->currentFrame()->setScrollPosition(previous_position);
        return;
    }

    QWebPage::FindFlags options = QWebPage::FindWrapsAroundDocument;
    if (matchCase())
        options |= QWebPage::FindCaseSensitively;

    bool found = w->page()->findText(_lastStringSearched, options);
    notifyMatch(found);

    if (!found)
    {
        QPoint previous_position = w->page()->currentFrame()->scrollPosition();
        w->page()->focusNextPrevChild(true);
        w->page()->currentFrame()->setScrollPosition(previous_position);
    }
}


void FindBar::findPrevious()
{
    // parent webwindow
    WebWindow *w = qobject_cast<WebWindow *>(parent());

    QWebPage::FindFlags options = QWebPage::FindBackward | QWebPage::FindWrapsAroundDocument;
    if (matchCase())
        options |= QWebPage::FindCaseSensitively;

    bool found = w->page()->findText(_lastStringSearched, options);
    notifyMatch(found);
}


void FindBar::matchCaseUpdate()
{
    // parent webwindow
    WebWindow *w = qobject_cast<WebWindow *>(parent());

    w->page()->findText(_lastStringSearched, QWebPage::FindBackward);
    findNext();
    updateHighlight();
}


void FindBar::updateHighlight()
{
    // parent webwindow
    WebWindow *w = qobject_cast<WebWindow *>(parent());

    QWebPage::FindFlags options = QWebPage::HighlightAllOccurrences;

    w->page()->findText(QL1S(""), options); //Clear an existing highlight

    if (!isHidden() && highlightAllState())
    {
        if (matchCase())
            options |= QWebPage::FindCaseSensitively;

        w->page()->findText(_lastStringSearched, options);
    }
}
