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

#include <QDateTime>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include "ProtocolLoggerWidget.h"
#include "Common/FileLogger.h"
#include "Imap/Model/Utils.h"

namespace Gui {

ConnectionLog::ConnectionLog(): widget(0), buffer(Common::RingBuffer<Common::LogMessage>(900)), closedTime(0)
{
}

ProtocolLoggerWidget::ProtocolLoggerWidget(QWidget *parent) :
    QWidget(parent), loggingActive(false), m_fileLogger(0)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    tabs = new QTabWidget(this);
    tabs->setTabsClosable(true);
    tabs->setTabPosition(QTabWidget::South);
    layout->addWidget(tabs);
    connect(tabs, &QTabWidget::tabCloseRequested, this, &ProtocolLoggerWidget::closeTab);

    clearAll = new QPushButton(tr("Clear all"), this);
    connect(clearAll, &QAbstractButton::clicked, this, &ProtocolLoggerWidget::clearLogDisplay);
    tabs->setCornerWidget(clearAll, Qt::BottomRightCorner);

    delayedDisplay = new QTimer(this);
    delayedDisplay->setSingleShot(true);
    delayedDisplay->setInterval(300);
    connect(delayedDisplay, &QTimer::timeout, this, &ProtocolLoggerWidget::slotShowLogs);
}

void ProtocolLoggerWidget::slotSetPersistentLogging(const bool enabled)
{
    if (enabled == !!m_fileLogger)
        return;

    if (enabled) {
        Q_ASSERT(!m_fileLogger);
        m_fileLogger = new Common::FileLogger(this);
        m_fileLogger->setFileLogging(true, Imap::Mailbox::persistentLogFileName());
        m_fileLogger->setAutoFlush(true);
    } else {
        delete m_fileLogger;
        m_fileLogger = 0;
    }
    emit persistentLoggingChanged(!!m_fileLogger);
}

ProtocolLoggerWidget::~ProtocolLoggerWidget()
{
}

QPlainTextEdit *ProtocolLoggerWidget::getLogger(const uint parserId)
{
    QPlainTextEdit *res = logs[parserId].widget;
    if (!res) {
        res = new QPlainTextEdit();
        res->setLineWrapMode(QPlainTextEdit::NoWrap);
        res->setCenterOnScroll(true);
        res->setMaximumBlockCount(1000);
        res->setReadOnly(true);
        res->setUndoRedoEnabled(false);
        res->setWordWrapMode(QTextOption::NoWrap);
        // Got to output something here using the default background,
        // otherwise the QPlainTextEdit would default its background
        // to the very first value we throw at it, which might be a
        // grey one.
        res->appendHtml(QStringLiteral("<p>&nbsp;</p>"));
        tabs->addTab(res, tr("Connection %1").arg(parserId));
        logs[parserId].widget = res;
    }
    return res;
}

void ProtocolLoggerWidget::closeTab(int index)
{
    QPlainTextEdit *w = qobject_cast<QPlainTextEdit *>(tabs->widget(index));
    Q_ASSERT(w);
    for (auto it = logs.begin(); it != logs.end(); ++it) {
        if (it->widget != w)
            continue;
        logs.erase(it);
        tabs->removeTab(index);
        w->deleteLater();
        return;
    }
}

void ProtocolLoggerWidget::clearLogDisplay()
{
    // We use very different indexing internally, to an extent where QTabWidget's ints are not easily obtainable from that,
    // so it's much better to clean up the GUI at first and only after that purge the underlying data
    while (tabs->count()) {
        QWidget *w = tabs->widget(0);
        Q_ASSERT(w);
        tabs->removeTab(0);
        w->deleteLater();
    }

    logs.clear();
}

void ProtocolLoggerWidget::showEvent(QShowEvent *e)
{
    loggingActive = true;
    QWidget::showEvent(e);
    slotShowLogs();
}

void ProtocolLoggerWidget::hideEvent(QHideEvent *e)
{
    loggingActive = false;
    QWidget::hideEvent(e);
}

void ProtocolLoggerWidget::slotImapLogged(uint parserId, Common::LogMessage message)
{
    using namespace Common;

    if (m_fileLogger) {
        m_fileLogger->slotImapLogged(parserId, message);
    }
    enum {CUTOFF=200};
    if (message.message.size() > CUTOFF) {
        message.truncatedBytes = message.message.size() - CUTOFF;
        message.message = message.message.left(CUTOFF);
    }
    // we rely on the default constructor and QMap's behavior of operator[] to call it here
    logs[parserId].buffer.append(message);
    if (loggingActive && !delayedDisplay->isActive())
        delayedDisplay->start();
}

void ProtocolLoggerWidget::flushToWidget(const uint parserId, Common::RingBuffer<Common::LogMessage> &buf)
{
    using namespace Common;

    QPlainTextEdit *w = getLogger(parserId);

    if (buf.skippedCount()) {
        w->appendHtml(tr("<p style=\"color: #bb0000\"><i><b>%n message(s)</b> were skipped because this widget was hidden.</i></p>",
                         "", buf.skippedCount()));
    }

    for (RingBuffer<LogMessage>::const_iterator it = buf.begin(); it != buf.end(); ++it) {
        QString message = QStringLiteral("<pre><span style=\"color: #808080\">%1</span> %2<span style=\"color: %3;%4\">%5</span>%6</pre>");
        QString direction;
        QString textColor;
        QString bgColor;
        QString trimmedInfo;

        switch (it->kind) {
        case LOG_IO_WRITTEN:
            if (it->message.startsWith(QLatin1String("***"))) {
                textColor = QStringLiteral("#800080");
                bgColor = QStringLiteral("#d0d0d0");
            } else {
                textColor = QStringLiteral("#800000");
                direction = QStringLiteral("<span style=\"color: #c0c0c0;\">&gt;&gt;&gt;&nbsp;</span>");
            }
            break;
        case LOG_IO_READ:
            if (it->message.startsWith(QLatin1String("***"))) {
                textColor = QStringLiteral("#808000");
                bgColor = QStringLiteral("#d0d0d0");
            } else {
                textColor = QStringLiteral("#008000");
                direction = QStringLiteral("<span style=\"color: #c0c0c0;\">&lt;&lt;&lt;&nbsp;</span>");
            }
            break;
        case LOG_MAILBOX_SYNC:
        case LOG_MESSAGES:
        case LOG_OTHER:
        case LOG_PARSE_ERROR:
        case LOG_TASKS:
            direction = QLatin1String("<span style=\"color: #c0c0c0;\">") + it->source + QLatin1String("</span> ");
            break;
        }

        if (it->truncatedBytes) {
            trimmedInfo = tr("<br/><span style=\"color: #808080; font-style: italic;\">(+ %n more bytes)</span>", "", it->truncatedBytes);
        }

        QString niceLine = it->message.toHtmlEscaped();
        niceLine.replace(QLatin1Char('\r'), 0x240d /* SYMBOL FOR CARRIAGE RETURN */)
        .replace(QLatin1Char('\n'), 0x240a /* SYMBOL FOR LINE FEED */);

        w->appendHtml(message.arg(it->timestamp.toString(QStringLiteral("hh:mm:ss.zzz")),
                                  direction, textColor,
                                  bgColor.isEmpty() ? QString() : QStringLiteral("background-color: %1").arg(bgColor),
                                  niceLine, trimmedInfo));
    }
    buf.clear();
}

void ProtocolLoggerWidget::slotShowLogs()
{
    // Please note that we can't return to the event loop from this context, as the log buffer has to be read atomically
    for (auto it = logs.begin(); it != logs.end(); ++it ) {
        flushToWidget(it.key(), it->buffer);
    }
}

/** @short Check whether some of the logs need cleaning */
void ProtocolLoggerWidget::onConnectionClosed(uint parserId, Imap::ConnectionState state)
{
    if (state == Imap::CONN_STATE_LOGOUT) {
        auto now = QDateTime::currentMSecsSinceEpoch();
        auto cutoff = now - 3 * 60 * 1000; // upon each disconnect, trash logs older than three minutes
        auto it = logs.find(parserId);
        if (it != logs.end()) {
            it->closedTime = now;
        }

        it = logs.begin() + 1; // do not ever delete log#0, that's a special one
        while (it != logs.end()) {
            if (it->closedTime != 0 && it->closedTime < cutoff) {
                if (it->widget) {
                    it->widget->deleteLater();
                }
                it = logs.erase(it);
            } else {
                ++it;
            }
        }
    }
}

}
