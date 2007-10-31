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

CommandHandle Parser::login( const QString& username, const QString& password )
{
    QStringList cmd( "LOGIN" );
    cmd << username << password;
    return queueCommand( cmd );
}

CommandHandle Parser::select( const QString& mailbox )
{
    QStringList cmd( "SELECT" );
    cmd << mailbox;
    return queueCommand( cmd );
}

CommandHandle Parser::examine( const QString& mailbox )
{
    QStringList cmd( "EXAMINE" );
    cmd << mailbox;
    return queueCommand( cmd );
}

CommandHandle Parser::create( const QString& mailbox )
{
    QStringList cmd( "CREATE" );
    cmd << mailbox;
    return queueCommand( cmd );
}

CommandHandle Parser::deleteMailbox( const QString& mailbox )
{
    QStringList cmd( "DELETE" );
    cmd << mailbox;
    return queueCommand( cmd );
}

CommandHandle Parser::rename( const QString& oldName, const QString& newName )
{
    QStringList cmd( "RENAME" );
    cmd << oldName << newName;
    return queueCommand( cmd );
}

CommandHandle Parser::subscribe( const QString& mailbox )
{
    QStringList cmd( "SUBSCRIBE" );
    cmd << mailbox;
    return queueCommand( cmd );
}

CommandHandle Parser::unsubscribe( const QString& mailbox )
{
    QStringList cmd( "UNSUBSCRIBE" );
    cmd << mailbox;
    return queueCommand( cmd );
}

CommandHandle Parser::queueCommand( const QStringList& cmd )
{
    // FIXME :)
    return CommandHandle();
}

}
#include "Parser.moc"
