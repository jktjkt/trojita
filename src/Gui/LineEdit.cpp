/* Copyright (C) 2009, 2011, 2013 by Glad Deschrijver <glad.deschrijver@gmail.com>
   Copyright (C) 2013 Thomas Lübking <thomas.luebking@gmail.com>
   Copyright (C) 2006 - 2015 Jan Kundrát <jkt@kde.org>

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

#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include "Common/InvokeMethod.h"
#include "Gui/LineEdit.h"
#include "UiUtils/IconLoader.h"

LineEdit::LineEdit(const QString &text, QWidget *parent)
    : QLineEdit(text, parent), m_historyEnabled(false), m_historyPosition(0)
{
    setClearButtonEnabled(true);
    connect(this, &QLineEdit::editingFinished, this, [this](){
        emit textEditingFinished(this->text());
    });
}

LineEdit::LineEdit(QWidget *parent)
    : LineEdit(QString(), parent)
{
}

bool LineEdit::isHistoryEnabled()
{
    return m_historyEnabled;
}

void LineEdit::setHistoryEnabled(bool enabled)
{
    if (enabled == m_historyEnabled)
        return;
    m_historyEnabled = enabled;
    if (m_historyEnabled) {
        // queued so we learn and update the history list *after* the user selected an item from popup
        connect(this, &QLineEdit::returnPressed, this, &LineEdit::learnEntry, Qt::QueuedConnection);
        QCompleter *completer = new QCompleter(QStringList(), this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::InlineCompletion);
        setCompleter(completer);
    } else {
        disconnect(this, &QLineEdit::returnPressed, this, &LineEdit::learnEntry);
        delete completer();
        setCompleter(nullptr);
    }
}

bool LineEdit::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Hide && o == completer()->popup()) {
        // next event cycle, the popup is deleted by the change and "return true" does not help
        // connect queued to not manipulate the history *during* selection from popup, but *after*
        CALL_LATER_NOARG(this, restoreInlineCompletion);
    }
    return false;
}

void LineEdit::keyReleaseEvent(QKeyEvent *ke)
{
    if (!m_historyEnabled || completer()->completionMode() == QCompleter::UnfilteredPopupCompletion ||
        !(ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down)) {
        // irrelevant events -> ignore
        QLineEdit::keyReleaseEvent(ke);
        return;
    }

    // this manages the shell-a-like history and
    // triggers a popdown (when pressing "down" arrow from current entry)

    QAbstractItemModel *m = completer()->model();
    int historyCount = m->rowCount();

    if (ke->key() == Qt::Key_Up) {
        // shell-a-like history navigation
        if (m_historyPosition == historyCount) {
            m_currentText = text();
            if (historyCount && m->index(m_historyPosition - 1,0).data().toString() == m_currentText) {
                // user still sees entry he entered (into history) last - skip that one.
                --m_historyPosition;
            }
        }
        if (--m_historyPosition < 0) {
            m_historyPosition = historyCount;
            setText(m_currentText);
        } else {
            setText(m->index(m_historyPosition,0).data().toString());
        }
    } else if (ke->key() == Qt::Key_Down) {
        if (m_historyPosition + 1 < historyCount && m->index(m_historyPosition + 1,0).data().toString() == m_currentText) {
            // user still sees entry he entered (into history) last - skip that one.
            ++m_historyPosition;
        }
        if (++m_historyPosition == historyCount) {
            // returning from shell-a-like journey
            setText(m_currentText);
        } else if (m_historyPosition > historyCount) {
            // trigger pop...down ;-)
            completer()->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
            completer()->complete(); // make a popup
            completer()->popup()->removeEventFilter(this); // protected against accidental double filtering
            completer()->popup()->installEventFilter(this); // to restore inline completion when it closes
            m_historyPosition = historyCount; // fix value to "current" (it's usually historyCount + 1 now)
        } else {
            // shell-a-like history navigation
            setText(m->index(m_historyPosition,0).data().toString());
        }
    }
    QLineEdit::keyReleaseEvent(ke);
}

void LineEdit::learnEntry()
{
    QAbstractItemModel *m = completer()->model();
    int rows = m->rowCount();
    for (int i = 0; i < rows; ++i) {
        if (m->index(i,0).data() == text()) {
            m->removeRow(i);
            --rows;
            break;
        }
    }
    m->insertRows(rows, 1);
    m->setData(m->index(rows, 0), text(), Qt::DisplayRole);
    m_historyPosition = rows + 1;
}

void LineEdit::restoreInlineCompletion()
{
    m_currentText = text(); // this was probably just updated by seleting from combobox
    completer()->setCompletionMode(QCompleter::InlineCompletion);
    CALL_LATER_NOARG(this, setFocus); // can't get in the second event cycle either
}
