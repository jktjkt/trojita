#include "AbstractMSA.h"

/** @short Implementations of the Mail Submission Agent interface */
namespace MSA {

AbstractMSA::AbstractMSA( QObject* parent ): QObject( parent )
{
}

AbstractMSA::~AbstractMSA()
{
}

}

#include "AbstractMSA.moc"
