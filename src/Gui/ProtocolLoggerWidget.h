
/* Copyright (C) 2010 Jan Kundrát <jkt@gentoo.org>

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

class QPushButton;
class QTabWidget;
class QPlainTextEdit;

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

public slots:
    /** @short A parser received something from the server */
    void parserLineReceived( uint parser, const QByteArray& line );
    /** @short Parser just sent a piece of data */
    void parserLineSent( uint parser, const QByteArray& line );

private slots:
    /** @short A tab is requested to close */
    void closeTab( int index );
    /** @short Clear all logs */
    void clearLogDisplay();

private:
    typedef enum { MSG_NONE, MSG_SENT, MSG_RECEIVED, MSG_INFO_SENT, MSG_INFO_RECEIVED } MessageType;

    enum { BUFFER_SIZE = 200 };

    class ParserLog {
    public:
        ParserLog(): widget(0), lastInserted(MSG_NONE), skippedItems(0) {}
        QPlainTextEdit* widget; /**< @short Widget displaying the log */
        MessageType lastInserted; /**< @short Kind of the message which was last pushed into the widget */
        uint skippedItems;
    };

    QTabWidget* tabs;
    QMap<uint, ParserLog> buffers;
    QPushButton* clearAll;

    /** @short Return (possibly newly created) ParserLog struct for a given parser */
    ParserLog& getLogger( const uint parser );

    /** @short Log the message into the GUI */
    void logMessage( const uint parser, const MessageType kind, const QByteArray& line );
};

}

#endif // GUI_PROTOCOLLOGGERWIDGET_H
