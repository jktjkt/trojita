#ifndef MULTIPARTALTERNATIVEREPLY_H
#define MULTIPARTALTERNATIVEREPLY_H

#include "FormattingReply.h"

namespace Imap {
namespace Network {

class MultipartAlternativeReply : public FormattingReply
{
Q_OBJECT
public:
    MultipartAlternativeReply( QObject* parent, Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg, Imap::Mailbox::TreeItemPart* _part );
protected slots:
    virtual void mainReplyFinished();
protected:
    virtual void everythingFinished();
};

}
}

#endif // MULTIPARTALTERNATIVEREPLY_H
