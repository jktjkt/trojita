#ifndef TAGGENERATOR_H
#define TAGGENERATOR_H

#include <QByteArray>

class TagGenerator {
    int number;
public:
    TagGenerator( int beforeStart=-1 ): number(beforeStart) {}
    QByteArray mk( const char * const what )
    {
        ++number;
        return last( what );
    }
    QByteArray last( const char * const what ) const
    {
        return QString::fromAscii("y%1 ").arg( number ).toAscii() + QByteArray(what);
    }
    void reset()
    {
        number = -1;
    }
};

#endif // TAGGENERATOR_H
