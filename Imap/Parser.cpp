#include <QStringList>
#include "Parser.h"

namespace Imap {

Parser::Parser( QObject* parent, QAbstractSocket * const socket ): QObject(parent), _socket(socket)
{
}

CommandHandle Parser::capability()
{
    return queueCommand( QStringList("CAPABILITY") );
}

CommandHandle Parser::noop()
{
    return queueCommand( QStringList("NOOP") );
}

CommandHandle Parser::logout()
{
    return queueCommand( QStringList("LOGOUT") );
}

CommandHandle Parser::queueCommand( const QStringList& cmd )
{
    // FIXME :)
    return CommandHandle();
}

}
#include "Parser.moc"
