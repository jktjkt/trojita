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
    QWidget(parent), lastOne(MSG_NONE)
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

void ProtocolLoggerWidget::logMessage( const uint parser, const LastMessageType kind, const QByteArray& line )
{
    QPlainTextEdit* e = getLogger( parser );

    if ( lastOne != kind ) {
        lastOne = kind;
        QTextCharFormat f = e->currentCharFormat();
        switch ( kind ) {
        case MSG_SENT:
            f.setForeground( QBrush( Qt::blue ) );
            break;
        case MSG_RECEIVED:
            f.setForeground( QBrush( Qt::green ) );
            break;
        default:
            Q_ASSERT( false );
        }
        e->mergeCurrentCharFormat( f );
    }

    e->appendPlainText( QString::fromLocal8Bit( line ) );
}

void ProtocolLoggerWidget::parserLineReceived( uint parser, const QByteArray& line )
{
    logMessage( parser, MSG_RECEIVED, line.trimmed() );
}

void ProtocolLoggerWidget::parserLineSent( uint parser, const QByteArray& line )
{
    logMessage( parser, MSG_SENT, line.trimmed() );
}

QPlainTextEdit* ProtocolLoggerWidget::getLogger( const uint parser )
{
    QPlainTextEdit* res = widgets[ parser ];
    if ( ! res ) {
        res = new QPlainTextEdit();
        res->setLineWrapMode( QPlainTextEdit::NoWrap );
        res->setCenterOnScroll( true );
        res->setMaximumBlockCount( 200 );
        res->setReadOnly( true );
        res->setUndoRedoEnabled( false );
        tabs->addTab( res, tr("Parser %1").arg( parser ) );
        widgets[ parser ] = res;
    }
    return res;
}

void ProtocolLoggerWidget::closeTab( int index )
{
    QPlainTextEdit* w = qobject_cast<QPlainTextEdit*>( tabs->widget( index ) );
    Q_ASSERT( w );
    uint parser = widgets.key( w );
    widgets.remove( parser );
    tabs->removeTab( index );
}

void ProtocolLoggerWidget::clearLogs()
{
    for ( QMap<uint, QPlainTextEdit*>::iterator it = widgets.begin(); it != widgets.end(); ++it ) {
        (*it)->document()->clear();
    }
}

}
