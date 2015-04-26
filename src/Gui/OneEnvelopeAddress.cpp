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
#include "OneEnvelopeAddress.h"
#include <QHeaderView>
#include <QFontMetrics>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <QUrl>
#  include <QTextDocument>
#else
#  include <QUrlQuery>
#endif
#include "Gui/MessageView.h"
#include "Gui/Util.h"

namespace Gui {

OneEnvelopeAddress::OneEnvelopeAddress(QWidget *parent, const Imap::Message::MailAddress &address, MessageView *messageView, const Position lastOneInRow):
    QLabel(parent), m_address(address), m_lastOneInRow(lastOneInRow)
{
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    setIndent(5);
    connect(this, SIGNAL(linkHovered(QString)), this, SLOT(onLinkHovered(QString)));

    QFontMetrics fm(font());
    int iconSize = fm.boundingRect(QLatin1Char('M')).height();
    contactKnownUrl = Gui::Util::resizedImageAsDataUrl(QLatin1String(":/icons/contact-known.png"), iconSize);
    contactUnknownUrl = Gui::Util::resizedImageAsDataUrl(QLatin1String(":/icons/contact-unknown.png"), iconSize);

    connect(this, SIGNAL(linkActivated(QString)), messageView, SLOT(headerLinkActivated(QString)));
    connect(this, SIGNAL(addressDetailsRequested(QString,QStringList&)),
            messageView, SIGNAL(addressDetailsRequested(QString,QStringList&)));

    processAddress();
}

void OneEnvelopeAddress::processAddress()
{
    QStringList matchingDisplayNames;
    emit addressDetailsRequested(m_address.mailbox + QLatin1Char('@') + m_address.host, matchingDisplayNames);
    QUrl url = m_address.asUrl();
    QString icon = QString::fromUtf8("<span style='width: 4px;'>&nbsp;</span><a href='x-trojita-manage-contact:%1'>"
                                     "<img src='%2' align='center'/></a>").arg(
                url.toString(QUrl::RemoveScheme),
                matchingDisplayNames.isEmpty() ? contactUnknownUrl : contactKnownUrl);
    setText(m_address.prettyName(Imap::Message::MailAddress::FORMAT_CLICKABLE) + icon + (m_lastOneInRow == Position::Middle ? tr(","): QString()));
}

void OneEnvelopeAddress::onLinkHovered(const QString &target)
{
    QUrl url(target);

    Imap::Message::MailAddress addr;
    if (Imap::Message::MailAddress::fromUrl(addr, url, QLatin1String("mailto")) ||
            Imap::Message::MailAddress::fromUrl(addr, url, QLatin1String("x-trojita-manage-contact"))) {
        QStringList matchingIdentities;
        emit addressDetailsRequested(addr.mailbox + QLatin1Char('@') + addr.host, matchingIdentities);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        QString link = Qt::escape(addr.prettyName(Imap::Message::MailAddress::FORMAT_READABLE));
        for (int i = 0; i < matchingIdentities.size(); ++i)
            matchingIdentities[i] = Qt::escape(matchingIdentities[i]);
#else
        QString link = addr.prettyName(Imap::Message::MailAddress::FORMAT_READABLE).toHtmlEscaped();
        for (int i = 0; i < matchingIdentities.size(); ++i)
            matchingIdentities[i] = matchingIdentities[i].toHtmlEscaped();
#endif
        QString identities = matchingIdentities.join(QLatin1String("<br/>\n"));
        if (matchingIdentities.isEmpty()) {
            setToolTip(link);
        } else {
            setToolTip(link + tr("<hr/><b>Address Book:</b><br/>") + identities);
        }
    } else {
        setToolTip(QString());
    }
}

}
