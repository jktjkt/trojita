#include "MultipartMixedReply.h"

#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"

#include <QDebug>

namespace Imap {
namespace Network {

MultipartMixedReply::MultipartMixedReply( QObject* parent,
    Imap::Mailbox::Model* _model, Imap::Mailbox::TreeItemMessage* _msg,
    Imap::Mailbox::TreeItemPart* _part ):
FormattingReply( parent, _model, _msg, _part)
{
}

void MultipartMixedReply::mainReplyFinished()
{
    QByteArray buf;
    buf.append( "<html>" );
    for ( uint i = 0; i < part->childrenCount( model ); ++i ) {
        Imap::Mailbox::TreeItemPart* anotherPart = static_cast<Imap::Mailbox::TreeItemPart*>(
                part->child( i, model ) );
        buf.append( QString::fromAscii("<object type=\"application/x-trojita-imap-part\" "
                                       "data=\"trojita-imap://part%1\" "
                                       "width=\"660\" height=1000 style=\"border: 1px solid red !important\"></object><hr/>\n").arg( anotherPart->pathToPart() )  );
        //buf.append( "<iframe src=\"trojita-imap://part" + anotherPart->pathToPart() + "\"></iframe>\n" );
    }
    buf.append( "</html>" );
    setData( QLatin1String( "text/html" ), buf );
    everythingFinished();
}


}
}

#include "MultipartMixedReply.moc"
