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
#include "ExternalElementsWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace Gui
{

ExternalElementsWidget::ExternalElementsWidget(QWidget *parent):
    QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    loadStuffButton = new QPushButton(tr("Load"), this);
    QLabel *lbl = new QLabel(tr("This e-mail wants to load external entities form the Internet"), this);
    layout->addWidget(lbl);
    layout->addWidget(loadStuffButton);
    connect(loadStuffButton, SIGNAL(clicked()), this, SIGNAL(loadingEnabled()));

    /*
    FIXME: would be cool to have more clear error messages

    Also perhaps provide a list of URLs of external entities? But note that
    the current implementation is an "all or nothing" -- once the user clicks
    the "allow external entities" button, they will be enabled for the current
    message until it is closed. Therefore, it is possible that user allows requests
    to a presumably "safe site", but something evil subsequently asks for something
    from a malicious, third-party site. I'm not sure how they'd do that, though, given
    that we already do disable JavaScript...

    */

}

}
