#ifndef SELECTEDHANDLER_H
#define SELECTEDHANDLER_H

#include "Model.h"

namespace Imap {
namespace Mailbox {

class SelectedHandler : public ModelStateHandler
{
    Q_OBJECT
public:
    SelectedHandler( Model* _m );
    virtual void handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp );
    virtual void handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp );
    virtual void handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp );
    virtual void handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp );
    virtual void handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp );
    virtual void handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp );
};

}
}

#endif // SELECTEDHANDLER_H
