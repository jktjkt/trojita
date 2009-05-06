#ifndef AUTHENTICATEDHANDLER_H
#define AUTHENTICATEDHANDLER_H

#include "Model.h"

namespace Imap {
namespace Mailbox {

class AuthenticatedHandler : public ModelStateHandler
{
    Q_OBJECT
public:
    AuthenticatedHandler( Model* _m );
    virtual void handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp );
    virtual void handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp );
    virtual void handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp );
    virtual void handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp );
    virtual void handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp );
    virtual void handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp );
    virtual void handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp );
    virtual void handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp );
};

}
}

#endif // AUTHENTICATEDHANDLER_H
