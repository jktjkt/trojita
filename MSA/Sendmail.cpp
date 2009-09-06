#include "Sendmail.h"

namespace MSA {

Sendmail::Sendmail( QObject* parent, const QString& _command, const QStringList& _args ):
        AbstractMSA( parent ), command(_command), args(_args)
{
    proc = new QProcess( this );
    connect( proc, SIGNAL(started()), this, SLOT(handleStarted()) );
    connect( proc, SIGNAL(finished(int)), this, SIGNAL(sent()) );
    connect( proc, SIGNAL(error(QProcess::ProcessError)),
             this, SLOT(handleError(QProcess::ProcessError)) );
    connect( proc, SIGNAL(bytesWritten(qint64)), this, SLOT(handleBytesWritten(qint64)) );
}

Sendmail::~Sendmail()
{
    proc->kill();
    proc->waitForFinished();
}

void Sendmail::sendMail( const QString& from, const QStringList& to, const QByteArray& data )
{
    // FIXME: from
    emit progressMax( data.size() );
    emit progress( 0 );
    // FIXME: support for passing the from argument, perhaps via the formatting options?
    QStringList myArgs = args;
    for ( QStringList::const_iterator it = to.begin(); it != to.end(); ++it ) {
        myArgs << *it;
    }
    writtenSoFar = 0;
    emit connecting();
    proc->start( command, myArgs );
    dataToSend = data;
}

void Sendmail::cancel()
{
    proc->terminate();
}

void Sendmail::handleStarted()
{
    emit sending();
    proc->write( dataToSend );
    proc->closeWriteChannel();
}

void Sendmail::handleError( QProcess::ProcessError e )
{
    // FIXME: nicer errors...
    emit error( tr("The sendmail process has failed" ) );
}

void Sendmail::handleBytesWritten( qint64 bytes )
{
    writtenSoFar += bytes;
    emit progress( writtenSoFar );
}

}

#include "Sendmail.moc"
