/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@kde.org>
   Copyright (C) 2013 - 2015 Pali Rohár <pali.rohar@gmail.com>

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
#ifndef GUI_ONEENVELOPEADDRESS_H
#define GUI_ONEENVELOPEADDRESS_H

#include <QModelIndex>
#include <QLabel>
#include "Imap/Parser/MailAddress.h"

namespace Plugins {
class PluginManager;
}

namespace Gui {

class MessageView;

/** @short Widget displaying the message envelope */
class OneEnvelopeAddress : public QLabel
{
    Q_OBJECT
public:
    /** @short Is this the last item in a list? */
    enum class Position {
        Middle, /**< We are not the last item in a row */
        Last, /**< This is the last item */
    };

    OneEnvelopeAddress(QWidget *parent, const Imap::Message::MailAddress &address, MessageView *messageView, const Position lastOneInRow);

private slots:
    void onLinkHovered(const QString &target);
    void finishOnLinkHovered(const QStringList &matchingDisplayNames);
    void processAddress();
    void finishProcessAddress(const QStringList &matchingDisplayNames);

private:
    QString contactKnownUrl, contactUnknownUrl;
    Imap::Message::MailAddress m_address;
    Position m_lastOneInRow;
    Plugins::PluginManager *m_pluginManager;
    QString m_link;

    OneEnvelopeAddress(const OneEnvelopeAddress &); // don't implement
    OneEnvelopeAddress &operator=(const OneEnvelopeAddress &); // don't implement
};

}

#endif
