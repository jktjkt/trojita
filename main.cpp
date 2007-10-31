#include <QTcpSocket>
#include "Imap/Parser.h"

int main( int argc, char** argv) {
    QAbstractSocket* sock = new QTcpSocket();
    Imap::Parser parser( 0, sock );
}
