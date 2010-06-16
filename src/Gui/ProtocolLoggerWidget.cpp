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
}

void ProtocolLoggerWidget::logMessage( const uint parser, const MessageType kind, const QByteArray& line )
{
    ParserLog& log = getLogger( parser );

    if ( ! loggingActive ) {
        ++log.skippedItems;
        return;
    }

    QString message = QString::fromAscii( "<pre><span style='color: #808080'>%1</span> %2<span style='color: %3;%4'>%5</span>%6</pre>" );
    QString direction;
    QString textColor;
    QString bgColor;
    QString niceLine;
    QString trimmedInfo;

    switch ( kind ) {
    case MSG_SENT:
        textColor = "#800000";
        direction = "<span style='color: #c0c0c0;'>&gt;&gt;&gt;&nbsp;</span>";
        break;
    case MSG_RECEIVED:
        textColor = "#008000";
        direction = "<span style='color: #c0c0c0;'>&lt;&lt;&lt;&nbsp;</span>";
        break;
    case MSG_INFO_SENT:
        textColor = "#800080";
        bgColor = "#d0d0d0";
        break;
    case MSG_INFO_RECEIVED:
        textColor = "#808000";
        bgColor = "#d0d0d0";
        break;
    case MSG_NONE:
        Q_ASSERT( false );
    }

    if ( line.size() > 120 ) {
        enum { SIZE = 100 };
        niceLine = Qt::escape( QString::fromAscii( line.left( SIZE ) ) );
        trimmedInfo = tr( "<br/><span style='color: #808080; font-style: italic;'>(%n bytes trimmed)</span>", "",  line.size() - SIZE );
    } else {
        niceLine = Qt::escape( QString::fromAscii( line ) );
    }

    niceLine.replace( QChar('\r'), 0x240d /* SYMBOL FOR CARRIAGE RETURN */ )
            .replace( QChar('\n'), 0x240a /* SYMBOL FOR LINE FEED */ );

    log.widget->appendHtml( message.arg( QTime::currentTime().toString( QString::fromAscii("hh:mm:ss.zzz") ),
                                         direction, textColor,
                                         bgColor.isEmpty() ? QString() : QString::fromAscii("background-color: %1").arg(bgColor),
                                         niceLine, trimmedInfo ) );
}

void ProtocolLoggerWidget::parserFatalError( uint parser, const QString& exceptionClass, const QString& message, const QByteArray& line, int position )
{
    QString buf = QString::fromAscii( "<pre><span style='color: #808080'>%1</span> "
                                          "<span style='color: #ff0000'>!!!</span> "
                                          "<span>%2</span></pre>" );
    QString niceLine = QString::fromLocal8Bit( line )
                       .replace( QChar('\r'), 0x240d /* SYMBOL FOR CARRIAGE RETURN */ )
                       .replace( QChar('\n'), 0x240a /* SYMBOL FOR LINE FEED */ );
    if ( position >= 0 && position < line.size() ) {
        niceLine = QString::fromAscii("%1<span style='background-color: #d08080;'>%2</span>")
                   .arg( Qt::escape( niceLine.left( position ) ), Qt::escape( niceLine.mid( position ) ) );
    } else {
        niceLine = Qt::escape( niceLine );
    }
    ParserLog& log = getLogger( parser );
    if ( log.skippedItems ) {
        log.widget->appendHtml(
                tr("<p style='color: #bb0000'><i>"
                   "<b>%n message(s)</b> were skipped because this widget was hidden.</i></p>",
                   "", log.skippedItems ) );
        log.skippedItems = 0;
    }
    log.widget->appendHtml( buf.arg( QTime::currentTime().toString( QString::fromAscii("hh:mm:ss.zzz") ),
                                     tr("Encountered fatal exception %1: %2").arg( exceptionClass, message ) ) );
    log.widget->appendHtml( buf.arg( QTime::currentTime().toString( QString::fromAscii("hh:mm:ss.zzz") ),
                                     niceLine ) );
}

void ProtocolLoggerWidget::parserLineReceived( uint parser, const QByteArray& line )
{
    logMessage( parser, line.startsWith( "*** " ) ? MSG_INFO_RECEIVED : MSG_RECEIVED, line.trimmed() );
}

void ProtocolLoggerWidget::parserLineSent( uint parser, const QByteArray& line )
{
    logMessage( parser, line.startsWith( "*** " ) ? MSG_INFO_SENT : MSG_SENT, line.trimmed() );
}

ProtocolLoggerWidget::ParserLog& ProtocolLoggerWidget::getLogger( const uint parser )
{
    ParserLog& res = buffers[ parser ];
    if ( ! res.widget ) {
        res.widget = new QPlainTextEdit();
        res.widget->setLineWrapMode( QPlainTextEdit::NoWrap );
        res.widget->setCenterOnScroll( true );
        res.widget->setMaximumBlockCount( BUFFER_SIZE );
        res.widget->setReadOnly( true );
        res.widget->setUndoRedoEnabled( false );
        // Got to output something here using the default background,
        // otherwise the QPlainTextEdit would default its background
        // to the very first value we throw at it, which might be a
        // grey one.
        res.widget->appendHtml( QString::fromAscii("<p>&nbsp;</p>") );
        tabs->addTab( res.widget, tr("Parser %1").arg( parser ) );
    }
    return res;
}

void ProtocolLoggerWidget::closeTab( int index )
{
    QPlainTextEdit* w = qobject_cast<QPlainTextEdit*>( tabs->widget( index ) );
    Q_ASSERT( w );
    for ( QMap<uint, ParserLog>::iterator it = buffers.begin(); it != buffers.end(); ++it ) {
        if ( it->widget != w )
            continue;
        buffers.erase( it );
        tabs->removeTab( index );
        w->deleteLater();
        return;
    }
}

void ProtocolLoggerWidget::clearLogDisplay()
{
    for ( QMap<uint, ParserLog>::iterator it = buffers.begin(); it != buffers.end(); ++it ) {
        it->widget->document()->clear();
    }
}

void ProtocolLoggerWidget::enableLogging( bool enabled )
{
    for ( QMap<uint, ParserLog>::iterator it = buffers.begin(); it != buffers.end(); ++it ) {
        if ( ! loggingActive && enabled ) {
            if ( it->skippedItems ) {
                it->widget->appendHtml(
                        tr("<p style='color: #bb0000'><i>Logging resumed. "
                           "<b>%n message(s)</b> got skipped while the logger widget was hidden.</i></p>",
                           "", it->skippedItems ) );
                it->skippedItems = 0;
            } else {
                it->widget->appendHtml( tr("<p style='color: #bb0000'><i>Logging resumed</i></p>") );
            }
        } else if ( loggingActive && ! enabled ) {
            it->widget->appendHtml( tr("<p style='color: #bb0000'><i>Logging suspended</i></p>") );
        }
    }
    loggingActive = enabled;
}

void ProtocolLoggerWidget::showEvent( QShowEvent* e )
{
    enableLogging( true );
    QWidget::showEvent( e );
}

void ProtocolLoggerWidget::hideEvent( QHideEvent* e )
{
    enableLogging( false );
    QWidget::hideEvent( e );
}

}
