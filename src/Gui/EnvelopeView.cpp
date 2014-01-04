/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#include "EnvelopeView.h"
#include <QHeaderView>
#include <QFontMetrics>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <QTextDocument>
#else
#  include <QUrlQuery>
#endif
#include "MessageView.h"
#include "Gui/Util.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Gui {

EnvelopeView::EnvelopeView(QWidget *parent, MessageView *messageView): QLabel(parent)
{
    // we create a dummy header, pass it through the style and the use it's color roles so we
    // know what headers in general look like in the system
    QHeaderView helpingHeader(Qt::Horizontal);
    helpingHeader.ensurePolished();

    setBackgroundRole(helpingHeader.backgroundRole());
    setForegroundRole(helpingHeader.foregroundRole());
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    setIndent(5);
    setWordWrap(true);
    connect(this, SIGNAL(linkHovered(QString)), this, SLOT(onLinkHovered(QString)));

    QFontMetrics fm(font());
    int iconSize = fm.boundingRect(QLatin1Char('M')).height();
    contactKnownUrl = Gui::Util::resizedImageAsDataUrl(QLatin1String(":/icons/contact-known.svg"), iconSize);
    contactUnknownUrl = Gui::Util::resizedImageAsDataUrl(QLatin1String(":/icons/contact-unknown.svgz"), iconSize);

    connect(this, SIGNAL(linkActivated(QString)), messageView, SLOT(headerLinkActivated(QString)));
    connect(this, SIGNAL(addressDetailsRequested(QString,QStringList&)),
            messageView, SIGNAL(addressDetailsRequested(QString,QStringList&)));
}

/** @short */
void EnvelopeView::setMessage(const QModelIndex &index)
{
    setText(headerText(index));
}

/** @short Return a HTML representation of the address list */
QString EnvelopeView::htmlizeAddresses(const QList<Imap::Message::MailAddress> &addresses)
{
    Q_ASSERT(!addresses.isEmpty());
    QStringList buf;
    Q_FOREACH(const Imap::Message::MailAddress &addr, addresses) {
        QStringList matchingDisplayNames;
        emit addressDetailsRequested(addr.mailbox + QLatin1Char('@') + addr.host, matchingDisplayNames);
        QUrl url = addr.asUrl();
        QString icon = QString::fromUtf8("<span style='width: 4px;'>&nbsp;</span><a href='x-trojita-manage-contact:%1'>"
                                         "<img src='%2' align='center'/></a>").arg(
                    url.toString(QUrl::RemoveScheme),
                    matchingDisplayNames.isEmpty() ? contactUnknownUrl : contactKnownUrl);
        buf << addr.prettyName(Imap::Message::MailAddress::FORMAT_CLICKABLE) + icon;
    }
    return buf.join(QLatin1String(", "));
}

QString EnvelopeView::headerText(const QModelIndex &index)
{
    if (!index.isValid())
        return QString();

    const Imap::Message::Envelope e = index.data(Imap::Mailbox::RoleMessageEnvelope).value<Imap::Message::Envelope>();

    QString res;
    if (!e.from.isEmpty())
        res += tr("<b>From:</b>&nbsp;%1<br/>").arg(htmlizeAddresses(e.from));
    if (!e.sender.isEmpty() && e.sender != e.from)
        res += tr("<b>Sender:</b>&nbsp;%1<br/>").arg(htmlizeAddresses(e.sender));
    if (!e.replyTo.isEmpty() && e.replyTo != e.from)
        res += tr("<b>Reply-To:</b>&nbsp;%1<br/>").arg(htmlizeAddresses(e.replyTo));
    QVariantList headerListPost = index.data(Imap::Mailbox::RoleMessageHeaderListPost).toList();
    if (!headerListPost.isEmpty()) {
        QStringList buf;
        Q_FOREACH(const QVariant &item, headerListPost) {
            const QString scheme = item.toUrl().scheme().toLower();
            if (scheme == QLatin1String("http") || scheme == QLatin1String("https") || scheme == QLatin1String("mailto")) {
                QString target = item.toUrl().toString();
                QString caption = item.toUrl().toString(scheme == QLatin1String("mailto") ? QUrl::RemoveScheme : QUrl::None);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                target = Qt::escape(target);
                caption = Qt::escape(caption);
#else
                target = target.toHtmlEscaped();
                caption = caption.toHtmlEscaped();
#endif
                buf << tr("<a href=\"%1\">%2</a>").arg(target, caption);
            } else {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                buf << Qt::escape(item.toUrl().toString());
#else
                buf << item.toUrl().toString().toHtmlEscaped();
#endif
            }
        }
        res += tr("<b>List-Post:</b>&nbsp;%1<br/>").arg(buf.join(tr(", ")));
    }
    if (!e.to.isEmpty())
        res += tr("<b>To:</b>&nbsp;%1<br/>").arg(htmlizeAddresses(e.to));
    if (!e.cc.isEmpty())
        res += tr("<b>Cc:</b>&nbsp;%1<br/>").arg(htmlizeAddresses(e.cc));
    if (!e.bcc.isEmpty())
        res += tr("<b>Bcc:</b>&nbsp;%1<br/>").arg(htmlizeAddresses(e.bcc));
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    res += tr("<b>Subject:</b>&nbsp;%1").arg(Qt::escape(e.subject));
#else
    res += tr("<b>Subject:</b>&nbsp;%1").arg(e.subject.toHtmlEscaped());
#endif
    if (e.date.isValid())
        res += tr("<br/><b>Date:</b>&nbsp;%1").arg(e.date.toLocalTime().toString(Qt::SystemLocaleLongDate));
    return res;
}

void EnvelopeView::onLinkHovered(const QString &target)
{
    QUrl url(target);

    Imap::Message::MailAddress addr;
    if (Imap::Message::MailAddress::fromUrl(addr, url, QLatin1String("mailto")) ||
            Imap::Message::MailAddress::fromUrl(addr, url, QLatin1String("x-trojita-manage-contact"))) {
        QStringList matchingIdentities;
        emit addressDetailsRequested(addr.mailbox + QLatin1Char('@') + addr.host, matchingIdentities);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        QString link = Qt::escape(addr.prettyName(Imap::Message::MailAddress::FORMAT_READABLE));
        QString identities = Qt::escape(matchingIdentities.join(QLatin1String("<br/>\n")));
#else
        QString link = addr.prettyName(Imap::Message::MailAddress::FORMAT_READABLE).toHtmlEscaped();
        QString identities = matchingIdentities.join(QLatin1String("<br/>\n")).toHtmlEscaped();
#endif
        if (matchingIdentities.isEmpty()) {
            setToolTip(link);
        } else {
            setToolTip(link + tr("<hr/><b>Address Book:</b><br/>") + identities);
        }
    } else {
        setToolTip(QString());
        return;
    }
}

}
