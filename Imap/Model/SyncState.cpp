#include "MailboxMetadata.h"

namespace Imap {
namespace Mailbox {

SyncState::SyncState():
        _exists(0), _recent(0), _unSeen(0), _uidNext(0), _uidValidity(0),
        _hasExists(false), _hasRecent(false), _hasUnSeen(false),
        _hasUidNext(false), _hasUidValidity(false), _hasFlags(false),
        _hasPermanentFlags(false)
{
}

bool SyncState::isComplete() const
{
    return _hasExists && _hasFlags && _hasPermanentFlags && _hasRecent && _hasUidValidity && _hasUnSeen;
}

uint SyncState::exists() const
{
    return _exists;
}

void SyncState::setExists( const uint exists )
{
    _exists = exists;
    _hasExists = true;
}

QStringList SyncState::flags() const
{
    return _flags;
}

void SyncState::setFlags( const QStringList& flags )
{
    _flags = flags;
    _hasFlags = true;
}

QStringList SyncState::permanentFlags() const
{
    return _permanentFlags;
}

void SyncState::setPermanentFlags( const QStringList& permanentFlags )
{
    _permanentFlags = permanentFlags;
    _hasPermanentFlags = true;
}

uint SyncState::recent() const
{
    return _recent;
}

void SyncState::setRecent( const uint recent )
{
    _recent = recent;
    _hasRecent = true;
}

uint SyncState::uidNext() const
{
    return _uidNext;
}

void SyncState::setUidNext( const uint uidNext )
{
    _uidNext = uidNext;
    _hasUidNext = true;
}

uint SyncState::uidValidity() const
{
    return _uidValidity;
}

void SyncState::setUidValidity( const uint uidValidity )
{
    _uidValidity = uidValidity;
    _hasUidValidity = true;
}

uint SyncState::unSeen() const
{
    return _unSeen;
}

void SyncState::setUnSeen( const uint unSeen )
{
    _unSeen = unSeen;
    _hasUnSeen = true;
}

}
}
