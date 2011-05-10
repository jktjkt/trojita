/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QDateTime>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include "ProtocolLoggerWidget.h"

namespace Gui {

ProtocolLoggerWidget::ProtocolLoggerWidget(QWidget *parent) :
    QWidget(parent), loggingActive(false)
{
    QVBoxLayout* layout = new QVBoxLayout( this );
    tabs = new QTabWidget( this );
    tabs->setTabsClosable( true );
    tabs->setTabPosition( QTabWidget::South );
    layout->addWidget( tabs );
    connect( tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)) );

    clearAll = new QPushButton( tr("Clear all"), this );
    connect( clearAll, SIGNAL(clicked()), this, SLOT(clearLogDisplay()) );
    tabs->setCornerWidget( clearAll, Qt::BottomRightCorner );

    delayedDisplay = new QTimer(this);
    delayedDisplay->setSingleShot(true);
    delayedDisplay->setInterval(300);
    connect(delayedDisplay, SIGNAL(timeout()), this, SLOT(slotShowLogs()));
}

QPlainTextEdit *ProtocolLoggerWidget::getLogger( const uint parser )
{
    QPlainTextEdit *res = loggerWidgets[parser];
    if (!res) {
        res = new QPlainTextEdit();
        res->setLineWrapMode( QPlainTextEdit::NoWrap );
        res->setCenterOnScroll( true );
        res->setMaximumBlockCount( 1000 );
        res->setReadOnly( true );
        res->setUndoRedoEnabled( false );
        res->setWordWrapMode(QTextOption::NoWrap);
        // Got to output something here using the default background,
        // otherwise the QPlainTextEdit would default its background
        // to the very first value we throw at it, which might be a
        // grey one.
        res->appendHtml(QString::fromAscii("<p>&nbsp;</p>"));
        tabs->addTab(res, tr("Connection %1").arg(parser));
        loggerWidgets[parser] = res;
    }
    return res;
}

void ProtocolLoggerWidget::closeTab( int index )
{
    QPlainTextEdit* w = qobject_cast<QPlainTextEdit*>( tabs->widget( index ) );
    Q_ASSERT( w );
    for ( QMap<uint, QPlainTextEdit*>::iterator it = loggerWidgets.begin(); it != loggerWidgets.end(); ++it ) {
        if ( *it != w )
            continue;
        loggerWidgets.erase( it );
        tabs->removeTab( index );
        w->deleteLater();
        return;
    }
}

void ProtocolLoggerWidget::clearLogDisplay()
{
    for ( QMap<uint, QPlainTextEdit*>::iterator it = loggerWidgets.begin(); it != loggerWidgets.end(); ++it ) {
        (*it)->document()->clear();
    }
}

void ProtocolLoggerWidget::showEvent( QShowEvent* e )
{
    loggingActive = true;
    QWidget::showEvent( e );
    slotShowLogs();
}

void ProtocolLoggerWidget::hideEvent( QHideEvent* e )
{
    loggingActive = false;
    QWidget::hideEvent( e );
}

void ProtocolLoggerWidget::slotImapLogged(uint parser, const Imap::Mailbox::LogMessage &message)
{
    QMap<uint, Imap::RingBuffer<Imap::Mailbox::LogMessage> >::iterator bufIt = buffers.find(parser);
    if (bufIt == buffers.end()) {
        // FIXME: don't hard-code that
        bufIt = buffers.insert(parser, Imap::RingBuffer<Imap::Mailbox::LogMessage>(900));
    }
    bufIt->append(message);
    if (loggingActive && !delayedDisplay->isActive())
        delayedDisplay->start();
}

void ProtocolLoggerWidget::flushToWidget(const uint parserId, Imap::RingBuffer<Imap::Mailbox::LogMessage> &buf)
{
    if (buf.skippedCount()) {
        getLogger(parserId)->appendHtml(tr("<p style='color: #bb0000'><i><b>%n message(s)</b> were skipped because this widget was hidden.</i></p>",
                                                  "", buf.skippedCount()));
    }

    QPlainTextEdit *w = getLogger(parserId);

    for (Imap::RingBuffer<Imap::Mailbox::LogMessage>::const_iterator it = buf.begin(); it != buf.end(); ++it) {
        QString message = QString::fromAscii("<pre><span style='color: #808080'>%1</span> %2<span style='color: %3;%4'>%5</span>%6</pre>");
        QString direction;
        QString textColor;
        QString bgColor;
        QString trimmedInfo;

        switch (it->kind) {
        case Imap::Mailbox::LOG_IO_WRITTEN:
            if ( it->message.startsWith("***")) {
                textColor = "#800080";
                bgColor = "#d0d0d0";
            } else {
                textColor = "#800000";
                direction = "<span style='color: #c0c0c0;'>&gt;&gt;&gt;&nbsp;</span>";
            }
            break;
        case Imap::Mailbox::LOG_IO_READ:
            if ( it->message.startsWith("***")) {
                textColor = "#808000";
                bgColor = "#d0d0d0";
            } else {
                textColor = "#008000";
                direction = "<span style='color: #c0c0c0;'>&lt;&lt;&lt;&nbsp;</span>";
            }
            break;
        case Imap::Mailbox::LOG_MAILBOX_SYNC:
        case Imap::Mailbox::LOG_MESSAGES:
        case Imap::Mailbox::LOG_OTHER:
        case Imap::Mailbox::LOG_PARSE_ERROR:
        case Imap::Mailbox::LOG_TASKS:
            break;
        }

        if (it->truncatedBytes) {
            trimmedInfo = tr("<br/><span style='color: #808080; font-style: italic;'>(+ %n more bytes)</span>", "", it->truncatedBytes);
        }

        QString niceLine = Qt::escape(it->message);
        niceLine.replace( QChar('\r'), 0x240d /* SYMBOL FOR CARRIAGE RETURN */ )
                .replace( QChar('\n'), 0x240a /* SYMBOL FOR LINE FEED */ );

        w->appendHtml(message.arg(QTime::currentTime().toString(QString::fromAscii("hh:mm:ss.zzz")),
                                  direction, textColor,
                                  bgColor.isEmpty() ? QString() : QString::fromAscii("background-color: %1").arg(bgColor),
                                  niceLine, trimmedInfo));
    }
    buf.clear();
}

void ProtocolLoggerWidget::slotShowLogs()
{
    // Please note that we can't return to the event loop from this context, as the log buffer has to be read atomically
    for (QMap<uint, Imap::RingBuffer<Imap::Mailbox::LogMessage> >::iterator it = buffers.begin(); it != buffers.end(); ++it) {
        flushToWidget(it.key(), *it);
    }
}

}
