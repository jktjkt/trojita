#ifndef IMAP_CATENATEDATA_H
#define IMAP_CATENATEDATA_H

#include <QByteArray>
#include <QPair>

namespace Imap {
namespace Mailbox {

/** @short How to use a message part in a CATENATE command */
typedef enum {
    CATENATE_TEXT,
    CATENATE_URL
} CatenatePartType;

typedef QPair<CatenatePartType, QByteArray> CatenatePair;

}
}

#endif // IMAP_CATENATEDATA_H
