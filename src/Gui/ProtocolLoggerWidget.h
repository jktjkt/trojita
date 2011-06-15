
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

#ifndef GUI_PROTOCOLLOGGERWIDGET_H
#define GUI_PROTOCOLLOGGERWIDGET_H

#include <QMap>
#include <QWidget>
#include "Imap/Model/Logging.h"
#include "Imap/Model/RingBuffer.h"

class QPushButton;
class QTabWidget;
class QPlainTextEdit;
class QTextStream;
class QTimer;

namespace Gui {

/** @short Protocol chat logger

A widget used for displaying a log of textual communication between
the client and the IMAP mail server.
*/
class ProtocolLoggerWidget : public QWidget
{
Q_OBJECT
public:
    explicit ProtocolLoggerWidget(QWidget *parent = 0);
    virtual ~ProtocolLoggerWidget();

public slots:
    /** @short An IMAP model wants to log something */
    void slotImapLogged(uint parser, const Imap::Mailbox::LogMessage &message);

    /** @short Enable/disable persistent logging */
    void slotSetPersistentLogging(const bool enabled);

private slots:
    /** @short A tab is requested to close */
    void closeTab( int index );
    /** @short Clear all logs */
    void clearLogDisplay();

    /** @short Copy contents of all buffers into the GUI widgets */
    void slotShowLogs();

private:
    QTabWidget* tabs;
    QMap<uint, QPlainTextEdit*> loggerWidgets;
    QMap<uint, Imap::RingBuffer<Imap::Mailbox::LogMessage> > buffers;
    QPushButton* clearAll;
    bool loggingActive;
    QTimer *delayedDisplay;
    QTextStream *m_fileLog;

    /** @short Return (possibly newly created) logger widget for a given parser */
    QPlainTextEdit *getLogger( const uint parser );

    /** @short Dump the log bufer contents to the GUI widget */
    void flushToWidget(const uint parserId, Imap::RingBuffer<Imap::Mailbox::LogMessage> &buf);

    /** @short Append a message to the log file */
    void writeToDisk(uint parser, const Imap::Mailbox::LogMessage &message);

    virtual void showEvent( QShowEvent* e );
    virtual void hideEvent( QHideEvent* e );
};

}

#endif // GUI_PROTOCOLLOGGERWIDGET_H
