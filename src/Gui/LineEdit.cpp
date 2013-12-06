/****************************************************************************
**
** Copyright (c) 2007 Trolltech ASA <info@trolltech.com>
** Modified (c) 2009, 2011, 2013 by Glad Deschrijver <glad.deschrijver@gmail.com>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#include "LineEdit.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QCompleter>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QToolButton>
#include <QStyle>
#include "Common/InvokeMethod.h"

LineEdit::LineEdit(const QString &text, QWidget *parent)
    : QLineEdit(parent), m_historyEnabled(false), m_historyPosition(0)
{
    init();
    setText(text);
}

LineEdit::LineEdit(QWidget *parent)
    : QLineEdit(parent), m_historyEnabled(false), m_historyPosition(0)
{
    init();
}

void LineEdit::init()
{
    m_clearButton = new QToolButton(this);
    const QPixmap pixmap(QLatin1String(":/icons/edit-clear-locationbar-rtl.png"));
    m_clearButton->setIcon(QIcon(pixmap));
    m_clearButton->setIconSize(pixmap.size());
    m_clearButton->setCursor(Qt::ArrowCursor);
    m_clearButton->setToolTip(tr("Clear input field"));
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    m_clearButton->hide();
    connect(m_clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(this, SIGNAL(textChanged(QString)), this, SLOT(updateClearButton(QString)));

    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    m_clearButton->setStyleSheet(QString("QToolButton { border: none; padding-left: 1px; padding-top: %1px; padding-bottom: %1px; padding-right: %1px; }")
        .arg(frameWidth + 1));
    setStyleSheet(QString("QLineEdit { padding-right: %1px; }")
        .arg(m_clearButton->sizeHint().width()));

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    layout->addWidget(m_clearButton);
}

QToolButton *LineEdit::clearButton()
{
    return m_clearButton;
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
        connect(this, SIGNAL(returnPressed()), this, SLOT(learnEntry()), Qt::QueuedConnection);
        QCompleter *completer = new QCompleter(QStringList(), this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::InlineCompletion);
        setCompleter(completer);
    } else {
        disconnect(this, SIGNAL(returnPressed()), this, SLOT(learnEntry()));
        delete completer();
        setCompleter(NULL);
    }
}

QSize LineEdit::sizeHint() const
{
    const QSize defaultSize = QLineEdit::sizeHint();
    const int minimumWidth = m_clearButton->sizeHint().width() + 2;
    return QSize(qMax(defaultSize.width(), minimumWidth), defaultSize.height());
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

void LineEdit::updateClearButton(const QString &text)
{
    m_clearButton->setVisible(!text.isEmpty());
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
