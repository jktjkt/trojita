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

#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
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
    connect( clearAll, SIGNAL(clicked()), this, SLOT(clearLogs()) );
    tabs->setCornerWidget( clearAll, Qt::BottomRightCorner );
}

void ProtocolLoggerWidget::logMessage( const uint parser, const MessageType kind, const QByteArray& line )
{
    ParserLog& log = getLogger( parser );

    log.kinds[ log.currentOffset ] = MSG_NONE;
    ++log.currentOffset;
    if ( log.currentOffset == BUFFER_SIZE )
        log.currentOffset = 0;
    log.kinds[ log.currentOffset ] = kind;
    log.lines[ log.currentOffset ] = line;
}

void ProtocolLoggerWidget::flushLog( uint parser )
{
    ParserLog& log = getLogger( parser );

    while ( log.kinds[ log.currentOffset ] != MSG_NONE ) {
        const MessageType& kind = log.kinds[ log.currentOffset ];
        const QByteArray& line = log.lines[ log.currentOffset ];
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
                // what the hell?
                break;
            }
            log.widget->mergeCurrentCharFormat( f );
        }
        log.widget->appendPlainText( QString::fromLocal8Bit( line ) );
        ++log.currentOffset;
        if ( log.currentOffset == BUFFER_SIZE )
            log.currentOffset = 0;
    }
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
        res.widget->setMaximumBlockCount( BUFFER_SIZE );
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

void ProtocolLoggerWidget::clearLogs()
{
    for ( QMap<uint, ParserLog>::iterator it = buffers.begin(); it != buffers.end(); ++it ) {
        it->widget->document()->clear();
    }
}

}
