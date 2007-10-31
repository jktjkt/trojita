#include "Parser.h"

Imap::Parser::Parser( QObject* parent, QAbstractSocket * const socket ): QObject(parent), _socket(socket) {
}

#include "Parser.moc"
