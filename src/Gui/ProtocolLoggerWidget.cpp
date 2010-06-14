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

#include <QTextEdit>
#include <QVBoxLayout>
#include "ProtocolLoggerWidget.h"

namespace Gui {

ProtocolLoggerWidget::ProtocolLoggerWidget(QWidget *parent) :
    QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout( this );
    w = new QTextEdit( this );
    layout->addWidget( w );
}

void ProtocolLoggerWidget::parserLineReceived( uint parser, const QByteArray& line )
{
    w->setPlainText( w->toPlainText() + QString::fromAscii("<<< %1: %2\n").arg( QString::number( parser ), QString::fromLocal8Bit( line ) ) );
}

void ProtocolLoggerWidget::parserLineSent( uint parser, const QByteArray& line )
{
    w->setPlainText( w->toPlainText() + QString::fromAscii(">>> %1: %2\n").arg( QString::number( parser ), QString::fromLocal8Bit( line ) ) );
}


}
