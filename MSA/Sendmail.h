#ifndef SENDMAIL_H
#define SENDMAIL_H

#include "AbstractMSA.h"

#include <QProcess>
#include <QStringList>

namespace MSA {

class Sendmail : public AbstractMSA
{
    Q_OBJECT
public:
    Sendmail( QObject* parent, const QString& _command, const QStringList& _args );
    virtual ~Sendmail();
    virtual void sendMail( const QString& from, const QStringList& to, const QByteArray& data );
private slots:
    void handleError( QProcess::ProcessError e );
    void handleBytesWritten( qint64 bytes );
    void handleStarted();
public slots:
    virtual void cancel();
private:
    QProcess* proc;
    QString command;
    QStringList args;
    QByteArray dataToSend;
    int writtenSoFar;
};

}

#endif // SENDMAIL_H
