/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "MessageListWidget.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QMenu>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>
#include "LineEdit.h"
#include "MsgListView.h"
#include "ReplaceCharValidator.h"
#include "UiUtils/IconLoader.h"

namespace Gui {

MessageListWidget::MessageListWidget(QWidget *parent) :
    QWidget(parent), m_supportsFuzzySearch(false)
{
    tree = new MsgListView(this);

    m_quickSearchText = new LineEdit(this);
    m_quickSearchText->setHistoryEnabled(true);
    // Filter out newline. It will wreak havoc into the direct IMAP passthrough and could lead to data loss.
    QValidator *validator = new ReplaceCharValidator(QLatin1Char('\n'), QLatin1Char(' '), m_quickSearchText);
    m_quickSearchText->setValidator(validator);
    m_quickSearchText->setPlaceholderText(tr("Quick Search"));
    m_quickSearchText->setToolTip(tr("Type in a text to search for within this mailbox. "
                                     "The icon on the left can be used to limit the search options "
                                     "(like whether to include addresses or message bodies, etc)."
                                     "<br/><hr/>"
                                     "Experts who have read RFC3501 can use the <code>:=</code> prefix and switch to a raw IMAP mode."));
    m_queryPlaceholder = tr("<query>");

    connect(m_quickSearchText, &QLineEdit::returnPressed, this, &MessageListWidget::slotApplySearch);
    connect(m_quickSearchText, &QLineEdit::textChanged, this, &MessageListWidget::slotConditionalSearchReset);
    connect(m_quickSearchText, &QLineEdit::cursorPositionChanged, this, &MessageListWidget::slotUpdateSearchCursor);

    m_searchOptions = new QAction(UiUtils::loadIcon(QStringLiteral("imap-search-details")), QStringLiteral("*"), this);
    m_searchOptions->setToolTip(tr("Options for the IMAP search..."));
    QMenu *optionsMenu = new QMenu(this);
    m_searchOptions->setMenu(optionsMenu);
    m_searchFuzzy = optionsMenu->addAction(tr("Fuzzy Search"));
    m_searchFuzzy->setCheckable(true);
    optionsMenu->addSeparator();
    m_searchInSubject = optionsMenu->addAction(tr("Subject"));
    m_searchInSubject->setCheckable(true);
    m_searchInSubject->setChecked(true);
    m_searchInBody = optionsMenu->addAction(tr("Body"));
    m_searchInBody->setCheckable(true);
    m_searchInSenders = optionsMenu->addAction(tr("Senders"));
    m_searchInSenders->setCheckable(true);
    m_searchInSenders->setChecked(true);
    m_searchInRecipients = optionsMenu->addAction(tr("Recipients"));
    m_searchInRecipients->setCheckable(true);

    optionsMenu->addSeparator();

    QMenu *complexMenu = new QMenu(tr("Complex IMAP query"), optionsMenu);
    connect(complexMenu, &QMenu::triggered, this, &MessageListWidget::slotComplexSearchInput);
    complexMenu->addAction(tr("Not ..."))->setData(QString(QLatin1String("NOT ") + m_queryPlaceholder));
    complexMenu->addAction(tr("Either... or..."))->setData(QString(QLatin1String("OR ") + m_queryPlaceholder + QLatin1Char(' ') + m_queryPlaceholder));
    complexMenu->addSeparator();
    complexMenu->addAction(tr("From sender"))->setData(QString(QLatin1String("FROM ") + m_queryPlaceholder));
    complexMenu->addAction(tr("To receiver"))->setData(QString(QLatin1String("TO ") + m_queryPlaceholder));
    complexMenu->addSeparator();
    complexMenu->addAction(tr("About subject"))->setData(QString(QLatin1String("SUBJECT " )+ m_queryPlaceholder));
    complexMenu->addAction(tr("Message contains ..."))->setData(QString(QLatin1String("BODY ") + m_queryPlaceholder));
    complexMenu->addSeparator();
    complexMenu->addAction(tr("Before date"))->setData(QLatin1String("BEFORE <d-mmm-yyyy>"));
    complexMenu->addAction(tr("Since date"))->setData(QLatin1String("SINCE <d-mmm-yyyy>"));
    complexMenu->addSeparator();
    complexMenu->addAction(tr("Has been seen"))->setData(QLatin1String("SEEN"));

    m_rawSearch = optionsMenu->addAction(tr("Allow raw IMAP search"));
    m_rawSearch->setCheckable(true);
    QAction *rawSearchMenu = optionsMenu->addMenu(complexMenu);
    rawSearchMenu->setVisible(false);
    connect(m_rawSearch, &QAction::toggled, rawSearchMenu, &QAction::setVisible);
    connect(m_rawSearch, &QAction::toggled, this, &MessageListWidget::rawSearchSettingChanged);

    m_searchOptions->setMenu(optionsMenu);
    connect(optionsMenu, &QMenu::aboutToShow, this, &MessageListWidget::slotDeActivateSimpleSearch);

    m_quickSearchText->addAction(m_searchOptions, QLineEdit::LeadingPosition);
    connect(m_searchOptions, &QAction::triggered, optionsMenu, [this, optionsMenu](){
        optionsMenu->popup(m_quickSearchText->mapToGlobal(QPoint(0, m_quickSearchText->height())), nullptr);
    });

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_quickSearchText);
    layout->addWidget(tree);

    m_searchResetTimer = new QTimer(this);
    m_searchResetTimer->setSingleShot(true);
    connect(m_searchResetTimer, &QTimer::timeout, this, &MessageListWidget::slotApplySearch);

    slotAutoEnableDisableSearch();
}

void MessageListWidget::focusSearch()
{
    if (!m_quickSearchText->isEnabled() || m_quickSearchText->hasFocus())
        return;
    m_quickSearchText->setFocus(Qt::ShortcutFocusReason);
}

void MessageListWidget::focusRawSearch()
{
    if (!m_quickSearchText->isEnabled() || m_quickSearchText->hasFocus() || !m_rawSearch->isChecked())
        return;
    m_quickSearchText->setFocus(Qt::ShortcutFocusReason);
    m_quickSearchText->setText(QStringLiteral(":="));
    m_quickSearchText->deselect();
    m_quickSearchText->setCursorPosition(m_quickSearchText->text().length());
}

void MessageListWidget::slotApplySearch()
{
    emit requestingSearch(searchConditions());
}

void MessageListWidget::slotAutoEnableDisableSearch()
{
    bool isEnabled;
    if (!m_quickSearchText->text().isEmpty()) {
        // Some search criteria are in effect and suddenly all matching messages
        // disappear. We have to make sure that the search bar remains enabled.
        isEnabled = true;
    } else if (tree && tree->model()) {
        isEnabled = tree->model()->rowCount();
    } else {
        isEnabled = false;
    }
    m_quickSearchText->setEnabled(isEnabled);
    m_searchOptions->setEnabled(isEnabled);
}

void MessageListWidget::slotSortingFailed()
{
    QPalette pal = m_quickSearchText->palette();
    pal.setColor(m_quickSearchText->backgroundRole(), Qt::red);
    pal.setColor(m_quickSearchText->foregroundRole(), Qt::white);
    m_quickSearchText->setPalette(pal);
    QTimer::singleShot(500, this, SLOT(slotResetSortingFailed()));
}

void MessageListWidget::slotResetSortingFailed()
{
    m_quickSearchText->setPalette(QPalette());
}

void MessageListWidget::slotConditionalSearchReset()
{
    if (m_quickSearchText->text().isEmpty())
        m_searchResetTimer->start(250);
    else
        m_searchResetTimer->stop();
}

void MessageListWidget::slotUpdateSearchCursor()
{
    int cp = m_quickSearchText->cursorPosition();
    int ts = -1, te = -1;
    for (int i = cp-1; i > -1; --i) {
        if (m_quickSearchText->text().at(i) == QLatin1Char('>'))
            break; // invalid
        if (m_quickSearchText->text().at(i) == QLatin1Char('<')) {
            ts = i;
            break; // found TagStart
        }
    }
    if (ts < 0)
        return; // not inside tag!
    for (int i = cp; i < m_quickSearchText->text().length(); ++i) {
        if (m_quickSearchText->text().at(i) == QLatin1Char('<'))
            break; // invalid
        if (m_quickSearchText->text().at(i) == QLatin1Char('>')) {
            te = i;
            break; // found TagEnd
        }
    }
    if (te < 0)
        return; // not inside tag?
    if (m_quickSearchText->text().midRef(ts, m_queryPlaceholder.length()) == m_queryPlaceholder)
        m_quickSearchText->setSelection(ts, m_queryPlaceholder.length());
}

void MessageListWidget::slotComplexSearchInput(QAction *act)
{
    QString s = act->data().toString();
    const int selectionStart = m_quickSearchText->selectionStart() - 1;
    if (selectionStart > -1 && m_quickSearchText->text().at(selectionStart) != QLatin1Char(' '))
            s.prepend(QLatin1Char(' '));
    m_quickSearchText->insert(s);
    if (!m_quickSearchText->text().startsWith(QLatin1String(":="))) {
        s = m_quickSearchText->text().trimmed();
        m_quickSearchText->setText(QLatin1String(":=") + s);
    }
    m_quickSearchText->setFocus();
    const int pos = m_quickSearchText->text().indexOf(m_queryPlaceholder);
    if (pos > -1)
        m_quickSearchText->setSelection(pos, m_queryPlaceholder.length());
}

void MessageListWidget::slotDeActivateSimpleSearch()
{
    const bool isEnabled = !(m_rawSearch->isChecked() && m_quickSearchText->text().startsWith(QLatin1String(":=")));
    m_searchInSubject->setEnabled(isEnabled);
    m_searchInBody->setEnabled(isEnabled);
    m_searchInSenders->setEnabled(isEnabled);
    m_searchInRecipients->setEnabled(isEnabled);
    m_searchFuzzy->setEnabled(isEnabled && m_supportsFuzzySearch);
}

QStringList MessageListWidget::searchConditions() const
{
    if (!m_quickSearchText->isEnabled() || m_quickSearchText->text().isEmpty())
        return QStringList();

    static QString rawPrefix = QStringLiteral(":=");

    if (m_rawSearch->isChecked() && m_quickSearchText->text().startsWith(rawPrefix)) {
        // It's a "raw" IMAP search, let's simply pass it through
        return QStringList() << m_quickSearchText->text().mid(rawPrefix.size());
    }

    QStringList keys;
    if (m_searchInSubject->isChecked())
        keys << QStringLiteral("SUBJECT");
    if (m_searchInBody->isChecked())
        keys << QStringLiteral("BODY");
    if (m_searchInRecipients->isChecked())
        keys << QStringLiteral("TO") << QStringLiteral("CC") << QStringLiteral("BCC");
    if (m_searchInSenders->isChecked())
        keys << QStringLiteral("FROM");

    if (keys.isEmpty())
        return keys;

    QStringList res;
    Q_FOREACH(const QString &key, keys) {
        if (m_supportsFuzzySearch)
            res << QStringLiteral("FUZZY");
        res << key << m_quickSearchText->text();
    }
    if (keys.size() > 1) {
        // Got to make this a conjunction. The OR operator's reverse-polish-notation accepts just two operands, though.
        int num = keys.size() - 1;
        for (int i = 0; i < num; ++i) {
            res.prepend(QStringLiteral("OR"));
        }
    }

    return res;
}

void MessageListWidget::setFuzzySearchSupported(bool supported)
{
    m_supportsFuzzySearch = supported;
    m_searchFuzzy->setEnabled(supported);
    m_searchFuzzy->setChecked(supported);
}

void MessageListWidget::setRawSearchEnabled(bool enabled)
{
    m_rawSearch->setChecked(enabled);
}

}
