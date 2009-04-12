#ifndef MULTIPARTMIXEDREPLY_H
#define MULTIPARTMIXEDREPLY_H

#include "FormattingReply.h"

namespace Imap {
namespace Network {

class MultipartMixedReply : public FormattingReply
{
Q_OBJECT
public:
    MultipartMixedReply( QObject* parent, Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg, Imap::Mailbox::TreeItemPart* _part );
protected slots:
    virtual void mainReplyFinished();
};

}
}

#endif // MULTIPARTMIXEDREPLY_H
