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

namespace Gui
{

ProtocolLoggerWidget::ProtocolLoggerWidget(QWidget *parent) :
    QWidget(parent), loggingActive(false), m_fileLogger(0)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    tabs = new QTabWidget(this);
    tabs->setTabsClosable(true);
    tabs->setTabPosition(QTabWidget::South);
    layout->addWidget(tabs);
    connect(tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

    clearAll = new QPushButton(tr("Clear all"), this);
    connect(clearAll, SIGNAL(clicked()), this, SLOT(clearLogDisplay()));
    tabs->setCornerWidget(clearAll, Qt::BottomRightCorner);

    delayedDisplay = new QTimer(this);
    delayedDisplay->setSingleShot(true);
    delayedDisplay->setInterval(300);
    connect(delayedDisplay, SIGNAL(timeout()), this, SLOT(slotShowLogs()));
}

void ProtocolLoggerWidget::slotSetPersistentLogging(const bool enabled)
{
    if (enabled) {
        if (m_fileLogger)
            return;

        m_fileLogger = new Common::FileLogger(this);
        m_fileLogger->setFileLogging(true, Imap::Mailbox::persistentLogFileName());
        m_fileLogger->setAutoFlush(true);
    } else {
        delete m_fileLogger;
        m_fileLogger = 0;
    }
}

ProtocolLoggerWidget::~ProtocolLoggerWidget()
{
}

QPlainTextEdit *ProtocolLoggerWidget::getLogger(const uint parser)
{
    QPlainTextEdit *res = loggerWidgets[parser];
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
        res->appendHtml(QLatin1String("<p>&nbsp;</p>"));
        tabs->addTab(res, tr("Connection %1").arg(parser));
        loggerWidgets[parser] = res;
    }
    return res;
}

void ProtocolLoggerWidget::closeTab(int index)
{
    QPlainTextEdit *w = qobject_cast<QPlainTextEdit *>(tabs->widget(index));
    Q_ASSERT(w);
    for (QMap<uint, QPlainTextEdit *>::iterator it = loggerWidgets.begin(); it != loggerWidgets.end(); ++it) {
        if (*it != w)
            continue;
        const uint key = it.key();
        loggerWidgets.erase(it);
        tabs->removeTab(index);
        w->deleteLater();
        buffers.remove(key);
        return;
    }
}

void ProtocolLoggerWidget::clearLogDisplay()
{
    // These will be freed from the GUI
    loggerWidgets.clear();

    // We use very different indexing internally, to an extent where QTabWidget's ints are not easily obtainable from that,
    // so it's much better to clean up the GUI at first and only after that purge the underlying data
    while (tabs->count()) {
        QWidget *w = tabs->widget(0);
        Q_ASSERT(w);
        tabs->removeTab(0);
        w->deleteLater();
    }

    buffers.clear();
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

void ProtocolLoggerWidget::slotImapLogged(uint parser, Common::LogMessage message)
{
    using namespace Common;

    QMap<uint, RingBuffer<LogMessage> >::iterator bufIt = buffers.find(parser);
    if (bufIt == buffers.end()) {
        // FIXME: don't hard-code that
        bufIt = buffers.insert(parser, RingBuffer<LogMessage>(900));
    }
    if (m_fileLogger) {
        m_fileLogger->slotImapLogged(parser, message);
    }
    enum {CUTOFF=200};
    if (message.message.size() > CUTOFF) {
        message.truncatedBytes = message.message.size() - CUTOFF;
        message.message = message.message.left(CUTOFF);
    }
    bufIt->append(message);
    if (loggingActive && !delayedDisplay->isActive())
        delayedDisplay->start();
}

void ProtocolLoggerWidget::flushToWidget(const uint parserId, Common::RingBuffer<Common::LogMessage> &buf)
{
    using namespace Common;

    QPlainTextEdit *w = getLogger(parserId);

    if (buf.skippedCount()) {
        w->appendHtml(tr("<p style='color: #bb0000'><i><b>%n message(s)</b> were skipped because this widget was hidden.</i></p>",
                         "", buf.skippedCount()));
    }

    for (RingBuffer<LogMessage>::const_iterator it = buf.begin(); it != buf.end(); ++it) {
        QString message = QString::fromUtf8("<pre><span style='color: #808080'>%1</span> %2<span style='color: %3;%4'>%5</span>%6</pre>");
        QString direction;
        QString textColor;
        QString bgColor;
        QString trimmedInfo;

        switch (it->kind) {
        case LOG_IO_WRITTEN:
            if (it->message.startsWith(QLatin1String("***"))) {
                textColor = "#800080";
                bgColor = "#d0d0d0";
            } else {
                textColor = "#800000";
                direction = "<span style='color: #c0c0c0;'>&gt;&gt;&gt;&nbsp;</span>";
            }
            break;
        case LOG_IO_READ:
            if (it->message.startsWith(QLatin1String("***"))) {
                textColor = "#808000";
                bgColor = "#d0d0d0";
            } else {
                textColor = "#008000";
                direction = "<span style='color: #c0c0c0;'>&lt;&lt;&lt;&nbsp;</span>";
            }
            break;
        case LOG_MAILBOX_SYNC:
        case LOG_MESSAGES:
        case LOG_OTHER:
        case LOG_PARSE_ERROR:
        case LOG_TASKS:
            direction = QLatin1String("<span style='color: #c0c0c0;'>") + it->source + QLatin1String("</span> ");
            break;
        }

        if (it->truncatedBytes) {
            trimmedInfo = tr("<br/><span style='color: #808080; font-style: italic;'>(+ %n more bytes)</span>", "", it->truncatedBytes);
        }

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        QString niceLine = it->message.toHtmlEscaped();
#else
        QString niceLine = Qt::escape(it->message);
#endif
        niceLine.replace(QChar('\r'), 0x240d /* SYMBOL FOR CARRIAGE RETURN */)
        .replace(QChar('\n'), 0x240a /* SYMBOL FOR LINE FEED */);

        w->appendHtml(message.arg(it->timestamp.toString(QLatin1String("hh:mm:ss.zzz")),
                                  direction, textColor,
                                  bgColor.isEmpty() ? QString() : QString::fromUtf8("background-color: %1").arg(bgColor),
                                  niceLine, trimmedInfo));
    }
    buf.clear();
}

void ProtocolLoggerWidget::slotShowLogs()
{
    // Please note that we can't return to the event loop from this context, as the log buffer has to be read atomically
    for (QMap<uint, Common::RingBuffer<Common::LogMessage> >::iterator it = buffers.begin(); it != buffers.end(); ++it) {
        flushToWidget(it.key(), *it);
    }
}

}
