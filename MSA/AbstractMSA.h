#ifndef ABSTRACTMSA_H
#define ABSTRACTMSA_H

#include <QByteArray>
#include <QObject>

namespace MSA {

class AbstractMSA : public QObject
{
    Q_OBJECT
public:
    AbstractMSA( QObject* parent );
    virtual ~AbstractMSA();
    virtual void sendMail( const QString& from, const QStringList& to, const QByteArray& data ) = 0;
public slots:
    virtual void cancel() = 0;
signals:
    void connecting();
    void sending();
    void sent();
    void error( const QString& message );
    void progressMax( int max );
    void progress( int num );
};

}

#endif // ABSTRACTMSA_H
