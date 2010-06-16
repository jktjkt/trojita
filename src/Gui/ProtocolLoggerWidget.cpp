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

#include <QDebug>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include "ProtocolLoggerWidget.h"

namespace Gui {

ProtocolLoggerWidget::ProtocolLoggerWidget(QWidget *parent) :
    QWidget(parent)
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

    dumper = new QTimer( this );
    dumper->setInterval( 10 );
    dumper->start();
    connect( dumper, SIGNAL(timeout()), this, SLOT(flushAllLogs()) );
}

void ProtocolLoggerWidget::logMessage( const uint parser, const MessageType kind, const QByteArray& line )
{
    ParserLog& log = getLogger( parser );

    log.kinds[ log.currentOffset ] = kind;
    log.lines[ log.currentOffset ] = line;

    ++log.currentOffset;
    if ( log.currentOffset == BUFFER_SIZE ) {
        log.currentOffset = 0;
        ++log.skippedItems;
    } else if ( log.skippedItems ) {
        ++log.skippedItems;
    }

    if ( ! dumper->isActive() )
        dumper->start();
}

void ProtocolLoggerWidget::flushLog( uint parser )
{
    ParserLog& log = getLogger( parser );

    /* Find out where to start the iteration and where to stop */
    int i;
    int stopAt;
    if ( log.kinds[ log.currentOffset ] == MSG_NONE ) {
        // The "next to be written to" is empty -> the log did not wrap around
        i = 0;
        stopAt = log.currentOffset;
        if ( log.skippedItems != 0 ) {
            qDebug() << "WTF???" << log.currentOffset << log.skippedItems;
            Q_ASSERT( 0 );
        }
    } else {
        // The log surely did wrap around, so we want to start at the
        // current position and go forward, possibly wrapping on crossing the boundary
        stopAt = log.currentOffset;
        i = log.currentOffset + 1;
        if ( i == BUFFER_SIZE )
            i = 0;

        // Now let's inform the user that we indeed wrapped around
        QTextCharFormat originalFormat = log.widget->currentCharFormat();
        QTextCharFormat f = log.widget->currentCharFormat();
        f.setFontItalic( true );
        f.setForeground( QBrush( Qt::red ) );
        f.setFontWeight( QFont::Bold );
        log.widget->mergeCurrentCharFormat( f );
        log.widget->appendPlainText(
                tr("\nThe logging GUI could not keep up with the protocol flow.\n %1 messages were not logged here.\n").arg(
                        log.skippedItems) );
        qDebug() << "wrapped; current position was" << log.currentOffset << "skipped" << log.skippedItems;
        log.widget->setCurrentCharFormat( originalFormat );
    }

    while ( i != stopAt ) {
        // Process current item
        MessageType& kind = log.kinds[ i ];
        const QByteArray& line = log.lines[ i ];
        if ( log.lastInserted != kind ) {
            log.lastInserted = kind;
            QTextCharFormat f = log.widget->currentCharFormat();
            switch ( kind ) {
            case MSG_SENT:
                f.setFontItalic( false );
                f.setForeground( QBrush( Qt::darkRed ) );
                break;
            case MSG_RECEIVED:
                f.setFontItalic( false );
                f.setForeground( QBrush( Qt::darkGreen ) );
                break;
            case MSG_INFO_SENT:
                f.setFontItalic( true );
                f.setForeground( QBrush( Qt::darkMagenta ) );
                break;
            case MSG_INFO_RECEIVED:
                f.setFontItalic( true );
                f.setForeground( QBrush( Qt::darkYellow ) );
                break;
            case MSG_NONE:
                Q_ASSERT( false );
                break;
            }
            log.widget->mergeCurrentCharFormat( f );
        }
        kind = MSG_NONE;
        log.widget->appendPlainText( QString::fromLocal8Bit( line.left( 100 ).replace( '\r', ' ').replace('\n', ' ') ) );

        // Increment the "current item" counter
        ++i;
        if ( i == BUFFER_SIZE ) {
            i = 0;
        }
    };
    log.currentOffset = 0;
    log.skippedItems = 0;
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
        res.kinds.fill( MSG_NONE, BUFFER_SIZE );
        res.lines.fill( QByteArray(), BUFFER_SIZE );
        res.widget->setLineWrapMode( QPlainTextEdit::NoWrap );
        res.widget->setCenterOnScroll( true );
        res.widget->setMaximumBlockCount( BUFFER_SIZE * 10 );
        res.widget->setReadOnly( true );
        res.widget->setUndoRedoEnabled( false );
        QTextCharFormat f = res.widget->currentCharFormat();
        f.setFontFamily( QString::fromAscii("Courier") );
        res.widget->mergeCurrentCharFormat( f );
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

void ProtocolLoggerWidget::flushAllLogs()
{
    Q_FOREACH( const uint& parser, buffers.keys() ) {
        flushLog( parser );
    }

    dumper->stop();
}

}
