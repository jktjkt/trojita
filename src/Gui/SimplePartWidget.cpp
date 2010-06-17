/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkReply>
#include <QWebFrame>

#include "AttachmentView.h"
#include "SimplePartWidget.h"
#include "Imap/Model/MailboxTree.h"

namespace Gui {

SimplePartWidget::SimplePartWidget( QWidget* parent,
                                    Imap::Network::MsgPartNetAccessManager* manager,
                                    Imap::Mailbox::TreeItemPart* _part ):
        EmbeddedWebView( parent, manager ), part(_part), reply(0)
{
    QUrl url;
    url.setScheme( QLatin1String("trojita-imap") );
    url.setHost( QLatin1String("msg") );
    url.setPath( part->pathToPart() );
    load( url );

    saveAction = new QAction( tr("Save..."), this );
    connect( saveAction, SIGNAL(triggered()), this, SLOT(slotSaveContents()) );
    this->addAction( saveAction );

    connect( page()->networkAccessManager(), SIGNAL(finished(QNetworkReply*)),
             this, SLOT(slotDeleteReply(QNetworkReply*)) );

    setContextMenuPolicy( Qt::ActionsContextMenu );
}

void SimplePartWidget::slotSaveContents()
{
    QString saveFileName = QFileDialog::getSaveFileName( this, tr("Save Attachment"),
                                                         AttachmentView::toRealFileName( part ), QString(),
                                                         0, QFileDialog::HideNameFilterDetails
                                                       );
    if ( saveFileName.isEmpty() )
        return;

    saving.setFileName( saveFileName );

    QNetworkRequest request;
    QUrl url;
    url.setScheme( QLatin1String("trojita-imap") );
    url.setHost( QLatin1String("msg") );
    url.setPath( part->pathToPart() );
    request.setUrl( url );
    reply = page()->networkAccessManager()->get( request );
    connect( reply, SIGNAL(finished()), this, SLOT(slotDataTransfered()) );
    connect( reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotTransferError()) );
    saved = false;
}

void SimplePartWidget::slotDataTransfered()
{
    Q_ASSERT( reply );
    if ( reply->error() == QNetworkReply::NoError ) {
        saving.open( QIODevice::WriteOnly );
        saving.write( reply->readAll() );
        saving.close();
        saved = true;
    }
}

void SimplePartWidget::slotTransferError()
{
    Q_ASSERT( reply );
    QMessageBox::critical( this, tr("Can't save attachment"),
                           tr("Unable to save the attachment. Error:\n%1").arg( reply->errorString() ) );
}

void SimplePartWidget::slotDeleteReply(QNetworkReply* reply)
{
    if ( reply == this->reply ) {
        if ( ! saved ) {
            slotDataTransfered();
        }
        delete reply;
        this->reply = 0;
    }
}

QString SimplePartWidget::quoteMe() const
{
    QString selection = selectedText();
    if ( selection.isEmpty() )
        return page()->mainFrame()->toPlainText();
    else
        return selection;
}

void SimplePartWidget::reloadContents()
{
    EmbeddedWebView::reload();
}

}

