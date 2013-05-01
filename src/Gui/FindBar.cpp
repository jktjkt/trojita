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


#include "FindBar.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QWebFrame>
#include <QWebView>
#include "LineEdit.h"
#include "Gui/Util.h"

namespace Gui {

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
    , m_lineEdit(new LineEdit(this))
    , m_matchCase(new QCheckBox(tr("&Match case"), this))
    , m_highlightAll(new QCheckBox(tr("&Highlight all"), this)),
      m_associatedWebView(0)
{
    QHBoxLayout *layout = new QHBoxLayout;

    // cosmetic
    layout->setContentsMargins(2, 0, 2, 0);

    // hide button
    QToolButton *hideButton = new QToolButton(this);
    hideButton->setAutoRaise(true);
    hideButton->setIcon(QIcon::fromTheme(QLatin1String("dialog-close")));
    hideButton->setShortcut(tr("Esc"));
    connect(hideButton, SIGNAL(clicked()), this, SLOT(hide()));
    layout->addWidget(hideButton);
    layout->setAlignment(hideButton, Qt::AlignLeft | Qt::AlignTop);

    // label
    QLabel *label = new QLabel(tr("Find:"));
    layout->addWidget(label);

    // Find Bar signal
    connect(this, SIGNAL(searchString(QString)), this, SLOT(find(QString)));

    // lineEdit, focusProxy
    setFocusProxy(m_lineEdit);
    m_lineEdit->setMaximumWidth(250);
    connect(m_lineEdit, SIGNAL(textChanged(QString)), this, SLOT(find(QString)));
    layout->addWidget(m_lineEdit);

    // buttons
    QPushButton *findNext = new QPushButton(QIcon::fromTheme(QLatin1String("go-down")), tr("&Next"), this);
    findNext->setShortcut(tr("F3"));
    QPushButton *findPrev = new QPushButton(QIcon::fromTheme(QLatin1String("go-up")), tr("&Previous"), this);
    findPrev->setShortcut(tr("Shift+F3"));
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
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() == Qt::ShiftModifier) {
            findPrevious();
        } else {
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
    QWidget::setVisible(visible);

    if (!m_associatedWebView)
        return;

    if (visible) {
        const QString selectedText = m_associatedWebView->page()->selectedText();
        if (!hasFocus() && !selectedText.isEmpty()) {
            const QString previousText = m_lineEdit->text();
            m_lineEdit->setText(selectedText);

            if (m_lineEdit->text() != previousText) {
                findPrevious();
            } else {
                updateHighlight();
            }
        } else if (selectedText.isEmpty()) {
            emit searchString(m_lineEdit->text());
        }

        m_lineEdit->setFocus();
        m_lineEdit->selectAll();
    } else {
        updateHighlight();

        // Clear the selection
        m_associatedWebView->page()->findText(QString());
    }
}


void FindBar::notifyMatch(bool match)
{
    if (m_lineEdit->text().isEmpty()) {
        m_lineEdit->setPalette(QPalette());
    } else {
        QColor backgroundTint = match ? QColor(0, 0xff, 0, 0x20) : QColor(0xff, 0, 0, 0x20);
        QPalette p;
        p.setColor(QPalette::Base, Gui::Util::tintColor(p.color(QPalette::Base), backgroundTint));
        m_lineEdit->setPalette(p);
    }
}


void FindBar::find(const QString & search)
{
    _lastStringSearched = search;

    updateHighlight();
    findNext();
}


void FindBar::findNext()
{
    Q_ASSERT(m_associatedWebView);

    if (isHidden()) {
        QPoint previous_position = m_associatedWebView->page()->currentFrame()->scrollPosition();
        m_associatedWebView->page()->focusNextPrevChild(true);
        m_associatedWebView->page()->currentFrame()->setScrollPosition(previous_position);
        return;
    }

    QWebPage::FindFlags options = QWebPage::FindWrapsAroundDocument;
    if (matchCase())
        options |= QWebPage::FindCaseSensitively;

    // FIXME: there's a problem with scrolling. Because we're using the QWebView inside a QScrollArea container, the attempts
    // to scroll the QWebView itself have no effect.
    // The WebKit sources themselves contain a nice comment about MacOS's Mail application which embeds a web view into
    // a scrollable container (now *this* looks familiar, doesn't it) and provides a special function, the scrollRectIntoView.
    // Sadly, the Qt wrapper doesn't implement it :(.

    bool found = m_associatedWebView->page()->findText(_lastStringSearched, options);
    notifyMatch(found);

    if (!found) {
        QPoint previous_position = m_associatedWebView->page()->currentFrame()->scrollPosition();
        m_associatedWebView->page()->focusNextPrevChild(true);
        m_associatedWebView->page()->currentFrame()->setScrollPosition(previous_position);
    }
}


void FindBar::findPrevious()
{
    Q_ASSERT(m_associatedWebView);

    QWebPage::FindFlags options = QWebPage::FindBackward | QWebPage::FindWrapsAroundDocument;
    if (matchCase())
        options |= QWebPage::FindCaseSensitively;

    bool found = m_associatedWebView->page()->findText(_lastStringSearched, options);
    notifyMatch(found);
}


void FindBar::matchCaseUpdate()
{
    Q_ASSERT(m_associatedWebView);

    m_associatedWebView->page()->findText(_lastStringSearched, QWebPage::FindBackward);
    findNext();
    updateHighlight();
}


void FindBar::updateHighlight()
{
    Q_ASSERT(m_associatedWebView);

    QWebPage::FindFlags options = QWebPage::HighlightAllOccurrences;

    m_associatedWebView->page()->findText(QString(), options); //Clear an existing highlight

    if (!isHidden() && highlightAllState())
    {
        if (matchCase())
            options |= QWebPage::FindCaseSensitively;

        m_associatedWebView->page()->findText(_lastStringSearched, options);
    }
}

void FindBar::setAssociatedWebView(QWebView *webView)
{
    if (m_associatedWebView)
        disconnect(m_associatedWebView, 0, this, 0);
    m_associatedWebView = webView;

    // Automatically hide this FindBar widget when the underlying webview goes away
    connect(m_associatedWebView, SIGNAL(destroyed(QObject*)), this, SLOT(hide()));
}

}
