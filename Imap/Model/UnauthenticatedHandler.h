#ifndef UNAUTHENTICATEDHANDLER_H
#define UNAUTHENTICATEDHANDLER_H

#include "Model.h"

namespace Imap {
namespace Mailbox {

class UnauthenticatedHandler : public ModelStateHandler
{
    Q_OBJECT
public:
    UnauthenticatedHandler( Model* _m );
    virtual void handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp );
    virtual void handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp );
    virtual void handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp );
    virtual void handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp );
    virtual void handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp );
    virtual void handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp );
    void handleResponseCode( Imap::Parser* ptr, const Imap::Responses::State* const resp );
};

}
}

#endif // UNAUTHENTICATEDHANDLER_H
