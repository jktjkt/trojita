#include "Model.h"

namespace Imap {
namespace Mailbox {

Model::SyncState::SyncState():
        _exists(0), _recent(0), _unSeen(0), _uidNext(0), _uidValidity(0),
        _hasExists(false), _hasRecent(false), _hasUnSeen(false),
        _hasUidNext(false), _hasUidValidity(false), _hasFlags(false),
        _hasPermanentFlags(false)
{
}

bool Model::SyncState::isComplete() const
{
    return _hasExists && _hasFlags && _hasPermanentFlags && _hasRecent && _hasUidValidity && _hasUnSeen;
}

uint Model::SyncState::exists() const
{
    return _exists;
}

void Model::SyncState::setExists( const uint exists )
{
    _exists = exists;
    _hasExists = true;
}

QStringList Model::SyncState::flags() const
{
    return _flags;
}

void Model::SyncState::setFlags( const QStringList& flags )
{
    _flags = flags;
    _hasFlags = true;
}

QStringList Model::SyncState::permanentFlags() const
{
    return _permanentFlags;
}

void Model::SyncState::setPermanentFlags( const QStringList& permanentFlags )
{
    _permanentFlags = permanentFlags;
    _hasPermanentFlags = true;
}

uint Model::SyncState::recent() const
{
    return _recent;
}

void Model::SyncState::setRecent( const uint recent )
{
    _recent = recent;
    _hasRecent = true;
}

uint Model::SyncState::uidNext() const
{
    return _uidNext;
}

void Model::SyncState::setUidNext( const uint uidNext )
{
    _uidNext = uidNext;
    _hasUidNext = true;
}

uint Model::SyncState::uidValidity() const
{
    return _uidValidity;
}

void Model::SyncState::setUidValidity( const uint uidValidity )
{
    _uidValidity = uidValidity;
    _hasUidValidity = true;
}

uint Model::SyncState::unSeen() const
{
    return _unSeen;
}

void Model::SyncState::setUnSeen( const uint unSeen )
{
    _unSeen = unSeen;
    _hasUnSeen = true;
}

}
}
