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

#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include "ProtocolLoggerWidget.h"

namespace Gui {

ProtocolLoggerWidget::ProtocolLoggerWidget(QWidget *parent) :
    QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout( this );
    tabs = new QTabWidget( this );
    tabs->setTabsClosable( true );
    layout->addWidget( tabs );
    connect( tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)) );
}

void ProtocolLoggerWidget::parserLineReceived( uint parser, const QByteArray& line )
{
    QTextEdit* e = getLogger( parser );
    e->setPlainText( e->toPlainText() + QString::fromAscii("<<< %1: %2\n").arg( QString::number( parser ), QString::fromLocal8Bit( line ) ) );
}

void ProtocolLoggerWidget::parserLineSent( uint parser, const QByteArray& line )
{
    QTextEdit* e = getLogger( parser );
    e->setPlainText( e->toPlainText() + QString::fromAscii(">>> %1: %2\n").arg( QString::number( parser ), QString::fromLocal8Bit( line ) ) );
}

QTextEdit* ProtocolLoggerWidget::getLogger( const uint parser )
{
    QTextEdit* res = widgets[ parser ];
    if ( ! res ) {
        res = new QTextEdit();
        tabs->addTab( res, tr("Parser %1").arg( parser ) );
        widgets[ parser ] = res;
    }
    return res;
}

void ProtocolLoggerWidget::closeTab( int index )
{
    QTextEdit* w = qobject_cast<QTextEdit*>( tabs->widget( index ) );
    Q_ASSERT( w );
    uint parser = widgets.key( w );
    widgets.remove( parser );
    tabs->removeTab( index );
}

}
