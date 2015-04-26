/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@kde.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "Gui/AddressRowWidget.h"
#include "Gui/FlowLayout.h"
#include "Gui/OneEnvelopeAddress.h"
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <QTextDocument>
#endif

namespace Gui {

AddressRowWidget::AddressRowWidget(QWidget *parent, const QString &headerName,
                                   const QList<Imap::Message::MailAddress> &addresses, MessageView *messageView):
    QWidget(parent)
{
    FlowLayout *lay = new FlowLayout(this, 0, 0, -1);
    setLayout(lay);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    const QString &headerNameEscaped = Qt::escape(headerName);
#else
    const QString &headerNameEscaped = headerName.toHtmlEscaped();
#endif

    QLabel *title = new QLabel(QString::fromUtf8("<b>%1:</b>").arg(headerNameEscaped), this);
    title->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    lay->addWidget(title);
    for (int i = 0; i < addresses.size(); ++i) {
        QWidget *w = new OneEnvelopeAddress(this, addresses[i], messageView,
                                            i == addresses.size() - 1 ?
                                                OneEnvelopeAddress::Position::Last :
                                                OneEnvelopeAddress::Position::Middle);
        lay->addWidget(w);
    }
}

}
