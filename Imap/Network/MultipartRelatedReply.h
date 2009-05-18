#ifndef MULTIPARTRELATEDREPLY_H
#define MULTIPARTRELATEDREPLY_H

#include "FormattingReply.h"

namespace Imap {
namespace Network {

class MultipartRelatedReply : public FormattingReply
{
Q_OBJECT
public:
    MultipartRelatedReply( QObject* parent, Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg, Imap::Mailbox::TreeItemPart* _part );
protected slots:
    virtual void mainReplyFinished();
protected:
    virtual void everythingFinished();
};

}
}

#endif // MULTIPARTRELATEDREPLY_H
