#include "Parser.h"

namespace Imap {

Parser::Parser( QObject* parent, QAbstractSocket * const socket ): QObject(parent), _socket(socket)
{
}

CommandHandle Parser::noop()
{
    return CommandHandle();
}

}
#include "Parser.moc"
