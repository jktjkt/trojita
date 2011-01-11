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
#ifndef ATTACHMENTVIEW_H
#define ATTACHMENTVIEW_H

#include <QWidget>

class QNetworkReply;

namespace Imap {
namespace Network {
    class FileDownloadManager;
    class MsgPartNetAccessManager;
}
namespace Mailbox {
    class TreeItemPart;
}
}

namespace Gui {

/** @short Widget depicting an attachment

  This widget provides a graphical representation about an e-mail attachment,
  that is, an interactive item which shows some basic information like the MIME
  type of the body part and the download button.  It also includes code for
  handling the actual download.
*/
class AttachmentView : public QWidget
{
    Q_OBJECT
public:
    AttachmentView( QWidget* parent,
                    Imap::Network::MsgPartNetAccessManager* _manager,
                    Imap::Mailbox::TreeItemPart* _part );
private slots:
    void slotTransferError( const QString& errorString );
    void slotFileNameRequested( QString* fileName );
private:
    Imap::Network::FileDownloadManager* _fileDownloadManager;

    AttachmentView(const AttachmentView&); // don't implement
    AttachmentView& operator=(const AttachmentView&); // don't implement
};

}

#endif // ATTACHMENTVIEW_H
