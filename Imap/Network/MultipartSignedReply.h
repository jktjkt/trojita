#ifndef MULTIPARTSIGNEDREPLY_H
#define MULTIPARTSIGNEDREPLY_H

#include "FormattingReply.h"

namespace Imap {
namespace Network {

class MultipartSignedReply : public FormattingReply
{
Q_OBJECT
public:
    MultipartSignedReply( QObject* parent, Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg, Imap::Mailbox::TreeItemPart* _part );
protected slots:
    virtual void mainReplyFinished();
protected:
    virtual void everythingFinished();
};

}
}

#endif // MULTIPARTSIGNEDREPLY_H
