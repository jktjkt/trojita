#include "Utils.h"
#include <cmath>

namespace Imap {
namespace Mailbox {

QString PrettySize::prettySize( uint bytes )
{
    if ( bytes == 0 )
        return tr("0");
    int order = std::log( bytes ) / std::log( 1024 );
    QString suffix;
    if ( order <= 0 )
        return QString::number( bytes );
    else if ( order == 1 )
        suffix = tr("kB");
    else if ( order == 2 )
        suffix = tr("MB");
    else if ( order == 3 )
        suffix = tr("GB");
    else
        suffix = tr("TB"); // shame on you for such mails
    return tr("%1 %2").arg( QString::number(
            bytes / ( std::pow( 1024.0, order ) ),
            'f', 1 ), suffix );
}


}

}

#include "Utils.moc"
