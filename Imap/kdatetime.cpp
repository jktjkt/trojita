/*
    This file is part of the KDE libraries
    Copyright (c) 2005-2008 David Jarvie <djarvie@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kdatetime.h"

#include <config.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <QtCore/QDateTime>
#include <QtCore/QRegExp>
#include <QtCore/QStringList>
#include <QtCore/QSharedData>

#include <kglobal.h>
#include <klocale.h>
#include <kcalendarsystemgregorian.h>
#include <ksystemtimezone.h>
#include <kdebug.h>

#ifdef Q_OS_WIN
#include <windows.h>    // SYSTEMTIME
#endif


static const char shortDay[][4] = {
    "Mon", "Tue", "Wed",
    "Thu", "Fri", "Sat",
    "Sun"
};
static const char longDay[][10] = {
    "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday",
    "Sunday"
};
static const char shortMonth[][4] = {
    "Jan", "Feb", "Mar", "Apr",
    "May", "Jun", "Jul", "Aug",
    "Sep", "Oct", "Nov", "Dec"
};
static const char longMonth[][10] = {
    "January", "February", "March",
    "April", "May", "June",
    "July", "August", "September",
    "October", "November", "December"
};


// The reason for the KDateTime being invalid, returned from KDateTime::fromString()
enum Status {
    stValid = 0,   // either valid, or really invalid
    stTooEarly     // invalid (valid date before QDate range)
};


static QDateTime fromStr(const QString& string, const QString& format, int& utcOffset,
                         QString& zoneName, QByteArray& zoneAbbrev, bool& dateOnly, Status&);
static int matchDay(const QString &string, int &offset, KCalendarSystem*);
static int matchMonth(const QString &string, int &offset, KCalendarSystem*);
static bool getUTCOffset(const QString &string, int &offset, bool colon, int &result);
static int getAmPm(const QString &string, int &offset, KLocale*);
static bool getNumber(const QString &string, int &offset, int mindigits, int maxdigits, int minval, int maxval, int &result);
static int findString_internal(const QString &string, const char *ptr, int count, int &offset, int disp);
template<int disp> static inline
int findString(const QString &string, const char array[][disp], int count, int &offset)
{ return findString_internal(string, array[0], count, offset, disp); }
static QDate checkDate(int year, int month, int day, Status&);

static const int MIN_YEAR = -4712;        // minimum year which QDate allows
static const int NO_NUMBER = 0x8000000;   // indicates that no number is present in string conversion functinos

#ifdef COMPILING_TESTS
KDECORE_EXPORT int KDateTime_utcCacheHit  = 0;
KDECORE_EXPORT int KDateTime_zoneCacheHit = 0;
#endif

/*----------------------------------------------------------------------------*/

class KDateTimeSpecPrivate
{
  public:
    KDateTimeSpecPrivate()  {}
    // *** NOTE: This structure is replicated in KDateTimePrivate. Any changes must be copied there.
    KTimeZone tz;            // if type == TimeZone, the instance's time zone.
    int       utcOffset;     // if type == OffsetFromUTC, the offset from UTC
    KDateTime::SpecType type;  // time spec type
};


KDateTime::Spec::Spec()
  : d(new KDateTimeSpecPrivate)
{
    d->type = KDateTime::Invalid;
}

KDateTime::Spec::Spec(const KTimeZone &tz)
  : d(new KDateTimeSpecPrivate())
{
    setType(tz);
}

KDateTime::Spec::Spec(SpecType type, int utcOffset)
  : d(new KDateTimeSpecPrivate())
{
    setType(type, utcOffset);
}

KDateTime::Spec::Spec(const Spec& spec)
  : d(new KDateTimeSpecPrivate())
{
    operator=(spec);
}

KDateTime::Spec::~Spec()
{
    delete d;
}

KDateTime::Spec &KDateTime::Spec::operator=(const Spec& spec)
{
    d->type = spec.d->type;
    if (d->type == KDateTime::TimeZone)
        d->tz = spec.d->tz;
    else if (d->type == KDateTime::OffsetFromUTC)
        d->utcOffset = spec.d->utcOffset;
    return *this;
}

void KDateTime::Spec::setType(SpecType type, int utcOffset)
{
    switch (type)
    {
        case KDateTime::OffsetFromUTC:
            d->utcOffset = utcOffset;
            // fall through to UTC
        case KDateTime::UTC:
        case KDateTime::ClockTime:
            d->type = type;
            break;
        case KDateTime::LocalZone:
            d->tz = KSystemTimeZones::local();
            d->type = KDateTime::TimeZone;
            break;
        case KDateTime::TimeZone:
        default:
            d->type = KDateTime::Invalid;
            break;
    }
}

void KDateTime::Spec::setType(const KTimeZone &tz)
{
    if (tz == KTimeZone::utc())
        d->type = KDateTime::UTC;
    else if (tz.isValid())
    {
        d->type = KDateTime::TimeZone;
        d->tz   = tz;
    }
    else
        d->type = KDateTime::Invalid;
}

KTimeZone KDateTime::Spec::timeZone() const
{
    if (d->type == KDateTime::TimeZone)
        return d->tz;
    if (d->type == KDateTime::UTC)
        return KTimeZone::utc();
    return KTimeZone();
}

bool KDateTime::Spec::isUtc() const
{
    if (d->type == KDateTime::UTC
    ||  (d->type == KDateTime::OffsetFromUTC  &&  d->utcOffset == 0))
        return true;
    return false;
}

KDateTime::Spec       KDateTime::Spec::UTC()                         { return Spec(KDateTime::UTC); }
KDateTime::Spec       KDateTime::Spec::ClockTime()                   { return Spec(KDateTime::ClockTime); }
KDateTime::Spec       KDateTime::Spec::LocalZone()                   { return Spec(KDateTime::LocalZone); }
KDateTime::Spec       KDateTime::Spec::OffsetFromUTC(int utcOffset)  { return Spec(KDateTime::OffsetFromUTC, utcOffset); }
KDateTime::SpecType   KDateTime::Spec::type() const                  { return d->type; }
bool KDateTime::Spec::isValid() const         { return d->type != KDateTime::Invalid; }
bool KDateTime::Spec::isLocalZone() const     { return d->type == KDateTime::TimeZone  &&  d->tz == KSystemTimeZones::local(); }
bool KDateTime::Spec::isClockTime() const     { return d->type == KDateTime::ClockTime; }
bool KDateTime::Spec::isOffsetFromUtc() const { return d->type == KDateTime::OffsetFromUTC; }
int  KDateTime::Spec::utcOffset() const       { return d->type == KDateTime::OffsetFromUTC ? d->utcOffset : 0; }

bool KDateTime::Spec::operator==(const Spec &other) const
{
    if (d->type != other.d->type
    ||  (d->type == KDateTime::TimeZone  &&  d->tz != other.d->tz)
    ||  (d->type == KDateTime::OffsetFromUTC  &&  d->utcOffset != other.d->utcOffset))
            return false;
    return true;
}

bool KDateTime::Spec::equivalentTo(const Spec &other) const
{
    if (d->type == other.d->type)
    {
        if (d->type == KDateTime::TimeZone  &&  d->tz != other.d->tz
        ||  (d->type == KDateTime::OffsetFromUTC  &&  d->utcOffset != other.d->utcOffset))
            return false;
        return true;
    }
    else
    {
        if (d->type == KDateTime::UTC  &&  other.d->type == KDateTime::OffsetFromUTC  &&  other.d->utcOffset == 0
        ||  (other.d->type == KDateTime::UTC  &&  d->type == KDateTime::OffsetFromUTC  &&  d->utcOffset == 0))
            return true;
        return false;
    }
}

QDataStream & operator<<(QDataStream &s, const KDateTime::Spec &spec)
{
    // The specification type is encoded in order to insulate from changes
    // to the SpecType enum.
    switch (spec.type())
    {
        case KDateTime::UTC:
            s << static_cast<quint8>('u');
            break;
        case KDateTime::OffsetFromUTC:
            s << static_cast<quint8>('o') << spec.utcOffset();
            break;
        case KDateTime::TimeZone:
#ifdef __GNUC__
#warning TODO: write full time zone data?
#endif
            s << static_cast<quint8>('z') << (spec.timeZone().isValid() ? spec.timeZone().name() : QString());
            break;
        case KDateTime::ClockTime:
            s << static_cast<quint8>('c');
            break;
        case KDateTime::Invalid:
        default:
            s << static_cast<quint8>(' ');
            break;
    }
    return s;
}

QDataStream & operator>>(QDataStream &s, KDateTime::Spec &spec)
{
    // The specification type is encoded in order to insulate from changes
    // to the SpecType enum.
    quint8 t;
    s >> t;
    switch (static_cast<char>(t))
    {
        case 'u':
            spec.setType(KDateTime::UTC);
            break;
        case 'o':
        {
            int utcOffset;
            s >> utcOffset;
            spec.setType(KDateTime::OffsetFromUTC, utcOffset);
            break;
        }
        case 'z':
        {
            QString zone;
            s >> zone;
            KTimeZone tz = KSystemTimeZones::zone(zone);
#ifdef __GNUC__
#warning TODO: read full time zone data?
#endif
            spec.setType(tz);
            break;
        }
        case 'c':
            spec.setType(KDateTime::ClockTime);
            break;
        default:
            spec.setType(KDateTime::Invalid);
            break;
    }
    return s;
}


/*----------------------------------------------------------------------------*/

K_GLOBAL_STATIC_WITH_ARGS(KDateTime::Spec, s_fromStringDefault, (KDateTime::ClockTime))

class KDateTimePrivate : public QSharedData
{
  public:
    KDateTimePrivate()
        : QSharedData(),
          specType(KDateTime::Invalid),
          status(stValid),
          utcCached(true),
          convertedCached(false),
          m2ndOccurrence(false),
          mDateOnly(false)
    {
    }

    KDateTimePrivate(const QDateTime &d, const KDateTime::Spec &s, bool donly = false)
        : QSharedData(),
          mDt(d),
          specType(s.type()),
          status(stValid),
          utcCached(false),
          convertedCached(false),
          m2ndOccurrence(false),
          mDateOnly(donly)
    {
        switch (specType)
        {
            case KDateTime::TimeZone:
                specZone = s.timeZone();
                break;
            case KDateTime::OffsetFromUTC:
                specUtcOffset= s.utcOffset();
                break;
            case KDateTime::Invalid:
                utcCached = true;
                // fall through to UTC
            case KDateTime::UTC:
            default:
                break;
        }
    }

    KDateTimePrivate(const KDateTimePrivate &rhs)
        : QSharedData(rhs),
          mDt(rhs.mDt),
          specZone(rhs.specZone),
          specUtcOffset(rhs.specUtcOffset),
          ut(rhs.ut),
          converted(rhs.converted),
          specType(rhs.specType),
          status(rhs.status),
          utcCached(rhs.utcCached),
          convertedCached(rhs.convertedCached),
          m2ndOccurrence(rhs.m2ndOccurrence),
          mDateOnly(rhs.mDateOnly),
          converted2ndOccur(rhs.converted2ndOccur)
    {}

    ~KDateTimePrivate()  {}
    const QDateTime& dt() const              { return mDt; }
    const QDate   date() const               { return mDt.date(); }
    KDateTime::Spec spec() const;
    QDateTime utc() const                    { return QDateTime(ut.date, ut.time, Qt::UTC); }
    bool      dateOnly() const               { return mDateOnly; }
    bool      secondOccurrence() const       { return m2ndOccurrence; }
    void      setDt(const QDateTime &dt)     { mDt = dt; utcCached = convertedCached = m2ndOccurrence = false; }
    void      setDtFromUtc(const QDateTime &utcdt);
    void      setDate(const QDate &d)        { mDt.setDate(d); utcCached = convertedCached = m2ndOccurrence = false; }
    void      setTime(const QTime &t)        { mDt.setTime(t); utcCached = convertedCached = mDateOnly = m2ndOccurrence = false; }
    void      setDtTimeSpec(Qt::TimeSpec s)  { mDt.setTimeSpec(s); utcCached = convertedCached = m2ndOccurrence = false; }
    void      setSpec(const KDateTime::Spec&);
    void      setDateOnly(bool d);
    int       timeZoneOffset() const;
    QDateTime toUtc(const KTimeZone &local = KTimeZone()) const;
    QDateTime toZone(const KTimeZone &zone, const KTimeZone &local = KTimeZone()) const;
    void      newToZone(KDateTimePrivate *newd, const KTimeZone &zone, const KTimeZone &local = KTimeZone()) const;
    bool      equalSpec(const KDateTimePrivate&) const;
    void      clearCache()                   { utcCached = convertedCached = false; }
    void      setDt(const QDateTime &dt, const QDateTime &utcDt)
    {
        mDt = dt;
        ut.date = utcDt.date();
        ut.time = utcDt.time();
        utcCached = true;
        convertedCached = false;
        m2ndOccurrence = false;
    }
    void      setUtc(const QDateTime &dt) const
    {
        ut.date = dt.date();
        ut.time = dt.time();
        utcCached = true;
        convertedCached = false;
    }

    /* Initialise the date/time for specType = UTC, from a time zone time,
     * and cache the time zone time.
     */
    void      setUtcFromTz(const QDateTime &dt, const KTimeZone &tz)
    {
        if (specType == KDateTime::UTC)
        {
            mDt               = tz.toUtc(dt);
            utcCached         = false;
            converted.date    = dt.date();
            converted.time    = dt.time();
            converted.tz      = tz;
            convertedCached   = true;
            converted2ndOccur = false;   // KTimeZone::toUtc() returns the first occurrence
        }
    }

    // Default time spec used by fromString()
    static KDateTime::Spec& fromStringDefault()
    {
        return *s_fromStringDefault;
    }


    static QTime         sod;               // start of day (00:00:00)

    /* Because some applications create thousands of instances of KDateTime, this
     * data structure is designed to minimize memory usage. Ensure that all small
     * members are kept together at the end!
     *
     * N.B. This class does not own any KTimeZone instances pointed to by data members.
     */
private:
    QDateTime             mDt;
public:
    KTimeZone             specZone;    // if specType == TimeZone, the instance's time zone
                                       // if specType == ClockTime, the local time zone used to calculate the cached UTC time (mutable)
    int                   specUtcOffset; // if specType == OffsetFromUTC, the offset from UTC
    mutable struct ut {                // cached UTC equivalent of 'mDt'. Saves space compared to storing QDateTime.
        QDate             date;
        QTime             time;
    } ut;
private:
    mutable struct converted {         // cached conversion to another time zone (if 'tz' is valid)
        QDate             date;
        QTime             time;
        KTimeZone         tz;
    } converted;
public:
    KDateTime::SpecType   specType          : 3; // time spec type
    Status                status            : 2; // reason for invalid status
    mutable bool          utcCached         : 1; // true if 'ut' is valid
    mutable bool          convertedCached   : 1; // true if 'converted' is valid
    mutable bool          m2ndOccurrence    : 1; // this is the second occurrence of a time zone time
private:
    bool                  mDateOnly         : 1; // true to ignore the time part
    mutable bool          converted2ndOccur : 1; // this is the second occurrence of 'converted' time
};


QTime KDateTimePrivate::sod(0,0,0);

KDateTime::Spec KDateTimePrivate::spec() const
{
    if (specType == KDateTime::TimeZone)
        return KDateTime::Spec(specZone);
    else
        return KDateTime::Spec(specType, specUtcOffset);
}

void KDateTimePrivate::setSpec(const KDateTime::Spec &other)
{
    if (specType == other.type())
    {
        switch (specType)
        {
            case KDateTime::TimeZone:
            {
                KTimeZone tz = other.timeZone();
                if (specZone == tz)
                    return;
                specZone = tz;
                break;
            }
            case KDateTime::OffsetFromUTC:
            {
                int offset = other.utcOffset();
                if (specUtcOffset == offset)
                    return;
                specUtcOffset = offset;
                break;
            }
            default:
                return;
        }
        utcCached = false;
    }
    else
    {
        specType = other.type();
        switch (specType)
        {
            case KDateTime::TimeZone:
                specZone = other.timeZone();
                break;
            case KDateTime::OffsetFromUTC:
                specUtcOffset = other.utcOffset();
                break;
            case KDateTime::Invalid:
                ut.date = QDate();   // cache an invalid UTC value
                utcCached = true;
                // fall through to UTC
            case KDateTime::UTC:
            default:
                break;
        }
    }
    convertedCached = false;
    setDtTimeSpec((specType == KDateTime::UTC) ? Qt::UTC : Qt::LocalTime);  // this clears cached UTC value
}

bool KDateTimePrivate::equalSpec(const KDateTimePrivate &other) const
{
    if (specType != other.specType
    ||  (specType == KDateTime::TimeZone  &&  specZone != other.specZone)
    ||  (specType == KDateTime::OffsetFromUTC  &&  specUtcOffset != other.specUtcOffset))
            return false;
    return true;
}

void KDateTimePrivate::setDateOnly(bool dateOnly)
{
    if (dateOnly != mDateOnly)
    {
        mDateOnly = dateOnly;
        if (dateOnly  &&  mDt.time() != sod)
        {
            mDt.setTime(sod);
            utcCached = false;
            convertedCached = false;
        }
        m2ndOccurrence = false;
    }
}

/* Sets the date/time to a given UTC date/time. The time spec is not changed. */
void KDateTimePrivate::setDtFromUtc(const QDateTime &utcdt)
{
    switch (specType)
    {
        case KDateTime::UTC:
            setDt(utcdt);
            break;
        case KDateTime::OffsetFromUTC:
        {
            QDateTime local = utcdt.addSecs(specUtcOffset);
            local.setTimeSpec(Qt::LocalTime);
            setDt(local, utcdt);
            break;
        }
        case KDateTime::TimeZone:
        {
            bool second;
            setDt(specZone.toZoneTime(utcdt, &second), utcdt);
            m2ndOccurrence = second;
            break;
        }
        case KDateTime::ClockTime:
            specZone = KSystemTimeZones::local();
            setDt(specZone.toZoneTime(utcdt), utcdt);
            break;
        default:    // invalid
            break;
    }
}

/*
 * Returns the UTC offset for the date/time, provided that it is a time zone type.
 */
int KDateTimePrivate::timeZoneOffset() const
{
    if (specType != KDateTime::TimeZone)
        return KTimeZone::InvalidOffset;
    if (utcCached)
    {
        QDateTime dt = mDt;
        dt.setTimeSpec(Qt::UTC);
        return utc().secsTo(dt);
    }
    int secondOffset;
    if (!specZone.isValid()) {
        return KTimeZone::InvalidOffset;
    }
    int offset = specZone.offsetAtZoneTime(mDt, &secondOffset);
    if (m2ndOccurrence)
    {
        m2ndOccurrence = (secondOffset != offset);   // cancel "second occurrence" flag if not applicable
        offset = secondOffset;
    }
    if (offset == KTimeZone::InvalidOffset)
    {
        ut.date = QDate();
        utcCached = true;
        convertedCached = false;
    }
    else
    {
        // Calculate the UTC time from the offset and cache it
        QDateTime utcdt = mDt;
        utcdt.setTimeSpec(Qt::UTC);
        setUtc(utcdt.addSecs(-offset));
    }
    return offset;
}

/*
 * Returns the date/time converted to UTC.
 * Depending on which KTimeZone class is involved, conversion to UTC may require
 * significant calculation, so the calculated UTC value is cached.
 */
QDateTime KDateTimePrivate::toUtc(const KTimeZone &local) const
{
    KTimeZone loc(local);
    if (utcCached)
    {
        // Return cached UTC value
        if (specType == KDateTime::ClockTime)
        {
            // ClockTime uses the dynamic current local system time zone.
            // Check for a time zone change before using the cached UTC value.
            if (!local.isValid())
                loc = KSystemTimeZones::local();
            if (specZone == loc)
            {
//                kDebug() << "toUtc(): cached -> " << utc() << endl,
#ifdef COMPILING_TESTS
                ++KDateTime_utcCacheHit;
#endif
                return utc();
            }
        }
        else
        {
//            kDebug() << "toUtc(): cached -> " << utc() << endl,
#ifdef COMPILING_TESTS
            ++KDateTime_utcCacheHit;
#endif
            return utc();
        }
    }

    // No cached UTC value, so calculate it
    switch (specType)
    {
        case KDateTime::UTC:
            return mDt;
        case KDateTime::OffsetFromUTC:
        {
            if (!mDt.isValid())
                break;
            QDateTime dt = QDateTime(mDt.date(), mDt.time(), Qt::UTC).addSecs(-specUtcOffset);
            setUtc(dt);
//            kDebug() << "toUtc(): calculated -> " << dt << endl,
            return dt;
        }
        case KDateTime::ClockTime:
        {
            if (!mDt.isValid())
                break;
            if (!loc.isValid())
                loc = KSystemTimeZones::local();
            const_cast<KDateTimePrivate*>(this)->specZone = loc;
            QDateTime dt(specZone.toUtc(mDt));
            setUtc(dt);
//            kDebug() << "toUtc(): calculated -> " << dt << endl,
            return dt;
        }
        case KDateTime::TimeZone:
            if (!mDt.isValid())
                break;
            timeZoneOffset();   // calculate offset and cache UTC value
//            kDebug() << "toUtc(): calculated -> " << utc() << endl,
            return utc();
        default:
            break;
    }

    // Invalid - mark it cached to avoid having to process it again
    ut.date = QDate();    // (invalid)
    utcCached = true;
    convertedCached = false;
//    kDebug() << "toUtc(): invalid";
    return mDt;
}

/* Convert this value to another time zone.
 * The value is cached to save having to repeatedly calculate it.
 * The caller should check for an invalid date/time.
 */
QDateTime KDateTimePrivate::toZone(const KTimeZone &zone, const KTimeZone &local) const
{
    if (convertedCached  &&  converted.tz == zone)
    {
        // Converted value is already cached
#ifdef COMPILING_TESTS
//        kDebug() << "KDateTimePrivate::toZone(" << zone->name() << "): " << mDt << " cached";
        ++KDateTime_zoneCacheHit;
#endif
        return QDateTime(converted.date, converted.time, Qt::LocalTime);
    }
    else
    {
        // Need to convert the value
        bool second;
        QDateTime result = zone.toZoneTime(toUtc(local), &second);
        converted.date    = result.date();
        converted.time    = result.time();
        converted.tz      = zone;
        convertedCached   = true;
        converted2ndOccur = second;
        return result;
    }
}

/* Convert this value to another time zone, and write it into the specified instance.
 * The value is cached to save having to repeatedly calculate it.
 * The caller should check for an invalid date/time.
 */
void KDateTimePrivate::newToZone(KDateTimePrivate *newd, const KTimeZone &zone, const KTimeZone &local) const
{
    newd->mDt            = toZone(zone, local);
    newd->specZone       = zone;
    newd->specType       = KDateTime::TimeZone;
    newd->utcCached      = utcCached;
    newd->mDateOnly      = mDateOnly;
    newd->m2ndOccurrence = converted2ndOccur;
    switch (specType)
    {
        case KDateTime::UTC:
            newd->ut.date = mDt.date();   // cache the UTC value
            newd->ut.time = mDt.time();
            break;
        case KDateTime::TimeZone:
            // This instance is also type time zone, so cache its value in the new instance
            newd->converted.date    = mDt.date();
            newd->converted.time    = mDt.time();
            newd->converted.tz      = specZone;
            newd->convertedCached   = true;
            newd->converted2ndOccur = m2ndOccurrence;
            newd->ut                = ut;
            return;
        default:
            newd->ut = ut;
            break;
    }
    newd->convertedCached = false;
}


/*----------------------------------------------------------------------------*/

KDateTime::KDateTime()
  : d(new KDateTimePrivate)
{
}

KDateTime::KDateTime(const QDate &date, const Spec &spec)
: d(new KDateTimePrivate(QDateTime(date, KDateTimePrivate::sod, Qt::LocalTime), spec, true))
{
    if (spec.type() == UTC)
        d->setDtTimeSpec(Qt::UTC);
}

KDateTime::KDateTime(const QDate &date, const QTime &time, const Spec &spec)
  : d(new KDateTimePrivate(QDateTime(date, time, Qt::LocalTime), spec))
{
    if (spec.type() == UTC)
        d->setDtTimeSpec(Qt::UTC);
}

KDateTime::KDateTime(const QDateTime &dt, const Spec &spec)
  : d(new KDateTimePrivate(dt, spec))
{
    // If the supplied date/time is UTC and we need local time, or vice versa, convert it.
    if (spec.type() == UTC)
    {
        if (dt.timeSpec() == Qt::LocalTime)
            d->setUtcFromTz(dt, KSystemTimeZones::local());   // set time & cache local time
    }
    else if (dt.timeSpec() == Qt::UTC)
        d->setDtFromUtc(dt);
}

KDateTime::KDateTime(const QDateTime &dt)
  : d(new KDateTimePrivate(dt, (dt.timeSpec() == Qt::LocalTime ? Spec(LocalZone) : Spec(UTC))))
{
}

KDateTime::KDateTime(const KDateTime &other)
  : d(other.d)
{
}

KDateTime::~KDateTime()
{
}

KDateTime &KDateTime::operator=(const KDateTime &other)
{
    d = other.d;
    return *this;
}

void      KDateTime::detach()                   { d.detach(); }
bool      KDateTime::isNull() const             { return d->dt().isNull(); }
bool      KDateTime::isValid() const            { return d->specType != Invalid  &&  d->dt().isValid(); }
bool      KDateTime::outOfRange() const         { return d->status == stTooEarly; }
bool      KDateTime::isDateOnly() const         { return d->dateOnly(); }
bool      KDateTime::isLocalZone() const        { return d->specType == TimeZone  &&  d->specZone == KSystemTimeZones::local(); }
bool      KDateTime::isClockTime() const        { return d->specType == ClockTime; }
bool      KDateTime::isUtc() const              { return d->specType == UTC || (d->specType == OffsetFromUTC && d->specUtcOffset == 0); }
bool      KDateTime::isOffsetFromUtc() const    { return d->specType == OffsetFromUTC; }
bool      KDateTime::isSecondOccurrence() const { return d->specType == TimeZone && d->secondOccurrence(); }
QDate     KDateTime::date() const               { return d->date(); }
QTime     KDateTime::time() const               { return d->dt().time(); }
QDateTime KDateTime::dateTime() const           { return d->dt(); }

KDateTime::Spec     KDateTime::timeSpec() const  { return d->spec(); }
KDateTime::SpecType KDateTime::timeType() const  { return d->specType; }

KTimeZone KDateTime::timeZone() const
{
    switch (d->specType)
    {
        case TimeZone:
            return d->specZone;
        case UTC:
            return KTimeZone::utc();
        default:
            return KTimeZone();
    }
}

int KDateTime::utcOffset() const
{
    switch (d->specType)
    {
        case TimeZone:
            return d->timeZoneOffset();   // calculate offset and cache UTC value
        case OffsetFromUTC:
            return d->specUtcOffset;
        default:
            return 0;
    }
}

KDateTime KDateTime::toUtc() const
{
    if (!isValid())
        return KDateTime();
    if (d->specType == UTC)
        return *this;
    if (d->dateOnly())
        return KDateTime(d->date(), UTC);
    QDateTime udt = d->toUtc();
    if (!udt.isValid())
        return KDateTime();
    return KDateTime(udt, UTC);
}

KDateTime KDateTime::toOffsetFromUtc() const
{
    if (!isValid())
        return KDateTime();
    int offset = 0;
    switch (d->specType)
    {
        case OffsetFromUTC:
            return *this;
        case UTC:
        {
            if (d->dateOnly())
                return KDateTime(d->date(), Spec(OffsetFromUTC, 0));
            QDateTime qdt = d->dt();
            qdt.setTimeSpec(Qt::LocalTime);
            return KDateTime(qdt, Spec(OffsetFromUTC, 0));
        }
        case TimeZone:
            offset = d->timeZoneOffset();   // calculate offset and cache UTC value
            break;
        case ClockTime:
            offset = KSystemTimeZones::local().offsetAtZoneTime(d->dt());
            break;
        default:
            return KDateTime();
    }
    if (d->dateOnly())
        return KDateTime(d->date(), Spec(OffsetFromUTC, offset));
    return KDateTime(d->dt(), Spec(OffsetFromUTC, offset));
}

KDateTime KDateTime::toOffsetFromUtc(int utcOffset) const
{
    if (!isValid())
        return KDateTime();
    if (d->specType == OffsetFromUTC  &&   d->specUtcOffset == utcOffset)
        return *this;
    if (d->dateOnly())
        return KDateTime(d->date(), Spec(OffsetFromUTC, utcOffset));
    return KDateTime(d->toUtc(), Spec(OffsetFromUTC, utcOffset));
}

KDateTime KDateTime::toLocalZone() const
{
    if (!isValid())
        return KDateTime();
    KTimeZone local = KSystemTimeZones::local();
    if (d->specType == TimeZone  &&  d->specZone == local)
        return *this;    // it's already local zone. Preserve UTC cache, if any
    if (d->dateOnly())
        return KDateTime(d->date(), Spec(local));
    switch (d->specType)
    {
        case TimeZone:
        case OffsetFromUTC:
        case UTC:
        {
            KDateTime result;
            d->newToZone(result.d, local, local);  // cache the time zone conversion
            return result;
        }
        case ClockTime:
            return KDateTime(d->dt(), Spec(local));
        default:
            return KDateTime();
    }
}

KDateTime KDateTime::toClockTime() const
{
    if (!isValid())
        return KDateTime();
    if (d->specType == ClockTime)
        return *this;
    if (d->dateOnly())
        return KDateTime(d->date(), Spec(ClockTime));
    KDateTime result = toLocalZone();
    result.d->specType = ClockTime;   // cached value (if any) is unaffected
    return result;
}

KDateTime KDateTime::toZone(const KTimeZone &zone) const
{
    if (!zone.isValid()  ||  !isValid())
        return KDateTime();
    if (d->specType == TimeZone  &&  d->specZone == zone)
        return *this;    // preserve UTC cache, if any
    if (d->dateOnly())
        return KDateTime(d->date(), Spec(zone));
    KDateTime result;
    d->newToZone(result.d, zone);  // cache the time zone conversion
    return result;
}

KDateTime KDateTime::toTimeSpec(const KDateTime &dt) const
{
    return toTimeSpec(dt.timeSpec());
}

KDateTime KDateTime::toTimeSpec(const Spec &spec) const
{
    if (spec == d->spec())
        return *this;
    if (!isValid())
        return KDateTime();
    if (d->dateOnly())
        return KDateTime(d->date(), spec);
    if (spec.type() == TimeZone)
    {
        KDateTime result;
        d->newToZone(result.d, spec.timeZone());  // cache the time zone conversion
        return result;
    }
    return KDateTime(d->toUtc(), spec);
}

uint KDateTime::toTime_t() const
{
    return d->toUtc().toTime_t();
}

void KDateTime::setTime_t(qint64 seconds)
{
    d->setSpec(UTC);
    int days = static_cast<int>(seconds / 86400);
    int secs = static_cast<int>(seconds % 86400);
    QDateTime dt;
    dt.setTimeSpec(Qt::UTC);   // prevent QDateTime::setTime_t() converting to local time
    dt.setTime_t(0);
    d->setDt(dt.addDays(days).addSecs(secs));
}

void KDateTime::setDateOnly(bool dateOnly)
{
    d->setDateOnly(dateOnly);
}

void KDateTime::setDate(const QDate &date)
{
    d->setDate(date);
}

void KDateTime::setTime(const QTime &time)
{
    d->setTime(time);
}

void KDateTime::setDateTime(const QDateTime &dt)
{
    d->clearCache();
    d->setDateOnly(false);
    if (dt.timeSpec() == Qt::LocalTime)
    {
        if (d->specType == UTC)
            d->setUtcFromTz(dt, KSystemTimeZones::local());   // set time & cache local time
        else
            d->setDt(dt);
    }
    else
        d->setDtFromUtc(dt);   // a UTC time has been supplied
}

void KDateTime::setTimeSpec(const Spec &other)
{
    d->setSpec(other);
}

void KDateTime::setSecondOccurrence(bool second)
{
    if (d->specType == KDateTime::TimeZone  &&  second != d->m2ndOccurrence)
    {
        d->m2ndOccurrence = second;
        d->clearCache();
        if (second)
        {
            // Check whether a second occurrence is actually possible, and
            // if not, reset m2ndOccurrence.
            d->timeZoneOffset();   // check, and cache UTC value
        }
    }
}

KDateTime KDateTime::addMSecs(qint64 msecs) const
{
    if (!msecs)
        return *this;  // retain cache - don't create another instance
    if (!isValid())
        return KDateTime();
    if (d->dateOnly())
    {
        KDateTime result(*this);
        result.d->setDate(d->date().addDays(static_cast<int>(msecs / 86400000)));
        return result;
    }
    qint64 secs = msecs / 1000;
    int oldms = d->dt().time().msec();
    int ms = oldms  +  static_cast<int>(msecs % 1000);
    if (msecs >= 0)
    {
        if (ms >= 1000)
        {
            ++secs;
            ms -= 1000;
        }
    }
    else
    {
        if (ms < 0)
        {
            --secs;
            ms += 1000;
        }
    }
    KDateTime result = addSecs(secs);
    QTime t = result.time();
    result.d->setTime(QTime(t.hour(), t.minute(), t.second(), ms));
    return result;
}

KDateTime KDateTime::addSecs(qint64 secs) const
{
    if (!secs)
        return *this;  // retain cache - don't create another instance
    if (!isValid())
        return KDateTime();
    int days    = static_cast<int>(secs / 86400);
    int seconds = static_cast<int>(secs % 86400);
    if (d->dateOnly())
    {
        KDateTime result(*this);
        result.d->setDate(d->date().addDays(days));
        return result;
    }
    if (d->specType == ClockTime)
    {
        QDateTime qdt = d->dt();
        qdt.setTimeSpec(Qt::UTC);    // set time as UTC to avoid daylight savings adjustments in addSecs()
        qdt = qdt.addDays(days).addSecs(seconds);
        qdt.setTimeSpec(Qt::LocalTime);
        return KDateTime(qdt, Spec(ClockTime));
    }
    return KDateTime(d->toUtc().addDays(days).addSecs(seconds), d->spec());
}

KDateTime KDateTime::addDays(int days) const
{
    if (!days)
        return *this;  // retain cache - don't create another instance
    KDateTime result(*this);
    result.d->setDate(d->date().addDays(days));
    return result;
}

KDateTime KDateTime::addMonths(int months) const
{
    if (!months)
        return *this;  // retain cache - don't create another instance
    KDateTime result(*this);
    result.d->setDate(d->date().addMonths(months));
    return result;
}

KDateTime KDateTime::addYears(int years) const
{
    if (!years)
        return *this;  // retain cache - don't create another instance
    KDateTime result(*this);
    result.d->setDate(d->date().addYears(years));
    return result;
}

int KDateTime::secsTo(const KDateTime &t2) const
{
    return static_cast<int>(secsTo_long(t2));
}

qint64 KDateTime::secsTo_long(const KDateTime &t2) const
{
    if (!isValid() || !t2.isValid())
        return 0;
    if (d->dateOnly())
    {
        QDate dat = t2.d->dateOnly() ? t2.d->date() : t2.toTimeSpec(d->spec()).d->date();
        return static_cast<qint64>(d->date().daysTo(dat)) * 86400;
    }
    if (t2.d->dateOnly())
        return static_cast<qint64>(toTimeSpec(t2.d->spec()).d->date().daysTo(t2.d->date())) * 86400;

    QDateTime dt1, dt2;
    if (d->specType == ClockTime  &&  t2.d->specType == ClockTime)
    {
        // Set both times as UTC to avoid daylight savings adjustments in secsTo()
        dt1 = d->dt();
        dt1.setTimeSpec(Qt::UTC);
        dt2 = t2.d->dt();
        dt2.setTimeSpec(Qt::UTC);
        return dt1.secsTo(dt2);
    }
    else
    {
        dt1 = d->toUtc();
        dt2 = t2.d->toUtc();
    }
    return static_cast<qint64>(dt1.date().daysTo(dt2.date())) * 86400
         + dt1.time().secsTo(dt2.time());
}

int KDateTime::daysTo(const KDateTime &t2) const
{
    if (!isValid() || !t2.isValid())
        return 0;
    if (d->dateOnly())
    {
        QDate dat = t2.d->dateOnly() ? t2.d->date() : t2.toTimeSpec(d->spec()).d->date();
        return d->date().daysTo(dat);
    }
    if (t2.d->dateOnly())
        return toTimeSpec(t2.d->spec()).d->date().daysTo(t2.d->date());

    QDate dat;
    switch (d->specType)
    {
        case UTC:
            dat = t2.d->toUtc().date();
            break;
        case OffsetFromUTC:
            dat = t2.d->toUtc().addSecs(d->specUtcOffset).date();
            break;
        case TimeZone:
            dat = t2.d->toZone(d->specZone).date();   // this caches the converted time in t2
            break;
        case ClockTime:
        {
            KTimeZone local = KSystemTimeZones::local();
            dat = t2.d->toZone(local, local).date();   // this caches the converted time in t2
            break;
        }
        default:    // invalid
            return 0;
    }
    return d->date().daysTo(dat);
}

KDateTime KDateTime::currentLocalDateTime()
{
    return KDateTime(QDateTime::currentDateTime(), Spec(KSystemTimeZones::local()));
}

KDateTime KDateTime::currentUtcDateTime()
{
#ifdef Q_OS_WIN
    SYSTEMTIME st;
    memset(&st, 0, sizeof(SYSTEMTIME));
    GetSystemTime(&st);
    return KDateTime(QDate(st.wYear, st.wMonth, st.wDay),
                     QTime(st.wHour, st.wMinute, st.wSecond, st.wMilliseconds),
                     UTC);
#else
    time_t t;
    ::time(&t);
    KDateTime result;
    result.setTime_t(static_cast<qint64>(t));
    return result;
#endif
}

KDateTime KDateTime::currentDateTime(const Spec &spec)
{
    return currentUtcDateTime().toTimeSpec(spec);
}

KDateTime::Comparison KDateTime::compare(const KDateTime &other) const
{
    QDateTime start1, start2;
    bool conv = (!d->equalSpec(*other.d) || d->secondOccurrence() != other.d->secondOccurrence());
    if (conv)
    {
        // Different time specs or one is a time which occurs twice,
        // so convert to UTC before comparing
        start1 = d->toUtc();
        start2 = other.d->toUtc();
    }
    else
    {
        // Same time specs, so no need to convert to UTC
        start1 = d->dt();
        start2 = other.d->dt();
    }
    if (d->dateOnly() || other.d->dateOnly())
    {
        // At least one of the instances is date-only, so we need to compare
        // time periods rather than just times.
        QDateTime end1, end2;
        if (conv)
        {
            if (d->dateOnly())
            {
                KDateTime kdt(*this);
                kdt.setTime(QTime(23,59,59,999));
                end1 = kdt.d->toUtc();
            }
            else
                end1 = start1;
            if (other.d->dateOnly())
            {
                KDateTime kdt(other);
                kdt.setTime(QTime(23,59,59,999));
                end2 = kdt.d->toUtc();
            }
            else
                end2 = start2;
        }
        else
        {
            if (d->dateOnly())
                end1 = QDateTime(d->date(), QTime(23,59,59,999), Qt::LocalTime);
            else
                end1 = d->dt();
            if (other.d->dateOnly())
                end2 = QDateTime(other.d->date(), QTime(23,59,59,999), Qt::LocalTime);
            else
                end2 = other.d->dt();
        }
        if (start1 == start2)
            return !d->dateOnly() ? AtStart : (end1 == end2) ? Equal
                 : (end1 < end2) ? static_cast<Comparison>(AtStart|Inside)
                 : static_cast<Comparison>(AtStart|Inside|AtEnd|After);
        if (start1 < start2)
            return (end1 < start2) ? Before
                 : (end1 == end2) ? static_cast<Comparison>(Before|AtStart|Inside|AtEnd)
                 : (end1 == start2) ? static_cast<Comparison>(Before|AtStart)
                 : (end1 < end2) ? static_cast<Comparison>(Before|AtStart|Inside) : Outside;
        else
            return (start1 > end2) ? After
                 : (start1 == end2) ? (end1 == end2 ? AtEnd : static_cast<Comparison>(AtEnd|After))
                 : (end1 == end2) ? static_cast<Comparison>(Inside|AtEnd)
                 : (end1 < end2) ? Inside : static_cast<Comparison>(Inside|AtEnd|After);
    }
    return (start1 == start2) ? Equal : (start1 < start2) ? Before : After;
}

bool KDateTime::operator==(const KDateTime &other) const
{
    if (d == other.d)
        return true;   // the two instances share the same data
    if (d->dateOnly() != other.d->dateOnly())
        return false;
    if (d->equalSpec(*other.d))
    {
        // Both instances are in the same time zone, so compare directly
        if (d->dateOnly())
            return d->date() == other.d->date();
        else
            return d->secondOccurrence() == other.d->secondOccurrence()
               &&  d->dt() == other.d->dt();
    }
    if (d->dateOnly())
    {
        // Date-only values are equal if both the start and end of day times are equal.
        // Don't waste time converting to UTC if the dates aren't very close.
        if (qAbs(d->date().daysTo(other.d->date())) > 2)
            return false;
        if (d->toUtc() != other.d->toUtc())
            return false;    // start-of-day times differ
        KDateTime end1(*this);
        end1.setTime(QTime(23,59,59,999));
        KDateTime end2(other);
        end2.setTime(QTime(23,59,59,999));
        return end1.d->toUtc() == end2.d->toUtc();
    }
    return d->toUtc() == other.d->toUtc();
}

bool KDateTime::operator<(const KDateTime &other) const
{
    if (d == other.d)
        return false;   // the two instances share the same data
    if (d->equalSpec(*other.d))
    {
        // Both instances are in the same time zone, so compare directly
        if (d->dateOnly() || other.d->dateOnly())
            return d->date() < other.d->date();
        if (d->secondOccurrence() == other.d->secondOccurrence())
            return d->dt() < other.d->dt();
        // One is the second occurrence of a date/time, during a change from
        // daylight saving to standard time, so only do a direct comparison
        // if the dates are more than 1 day apart.
        int diff = d->dt().date().daysTo(other.d->dt().date());
        if (diff > 1)
            return true;
        if (diff < -1)
            return false;
    }
    if (d->dateOnly())
    {
        // This instance is date-only, so we need to compare the end of its
        // day with the other value. Note that if the other value is date-only,
        // we want to compare with the start of its day, which will happen
        // automatically.
        KDateTime kdt(*this);
        kdt.setTime(QTime(23,59,59,999));
        return kdt.d->toUtc() < other.d->toUtc();
    }
    return d->toUtc() < other.d->toUtc();
}

QString KDateTime::toString(const QString &format) const
{
    if (!isValid())
        return QString();
    enum { TZNone, UTCOffsetShort, UTCOffset, UTCOffsetColon, TZAbbrev, TZName };
    KLocale *locale = KGlobal::locale();
    KCalendarSystemGregorian calendar(locale);
    QString result;
    QString s;
    int num, numLength, zone;
    bool escape = false;
    ushort flag = 0;
    for (int i = 0, end = format.length();  i < end;  ++i)
    {
        zone = TZNone;
        num = NO_NUMBER;
        numLength = 0;    // no leading zeroes
        ushort ch = format[i].unicode();
        if (!escape)
        {
            if (ch == '%')
                escape = true;
            else
                result += format[i];
            continue;
        }
        if (!flag)
        {
            switch (ch)
            {
                case '%':
                    result += QLatin1Char('%');
                    break;
                case ':':
                    flag = ch;
                    break;
                case 'Y':     // year
                    num = d->date().year();
                    numLength = 4;
                    break;
                case 'y':     // year, 2 digits
                    num = d->date().year() % 100;
                    numLength = 2;
                    break;
                case 'm':     // month, 01 - 12
                    numLength = 2;
                    num = d->date().month();
                    break;
                case 'B':     // month name, translated
                    result += calendar.monthName(d->date().month(), 2000, KCalendarSystem::LongName);
                    break;
                case 'b':     // month name, translated, short
                    result += calendar.monthName(d->date().month(), 2000, KCalendarSystem::ShortName);
                    break;
                case 'd':     // day of month, 01 - 31
                    numLength = 2;
                    // fall through to 'e'
                case 'e':     // day of month, 1 - 31
                    num = d->date().day();
                    break;
                case 'A':     // week day name, translated
                    result += calendar.weekDayName(d->date().dayOfWeek(), KCalendarSystem::LongDayName);
                    break;
                case 'a':     // week day name, translated, short
                    result += calendar.weekDayName(d->date().dayOfWeek(), KCalendarSystem::ShortDayName);
                    break;
                case 'H':     // hour, 00 - 23
                    numLength = 2;
                    // fall through to 'k'
                case 'k':     // hour, 0 - 23
                    num = d->dt().time().hour();
                    break;
                case 'I':     // hour, 01 - 12
                    numLength = 2;
                    // fall through to 'l'
                case 'l':     // hour, 1 - 12
                    num = (d->dt().time().hour() + 11) % 12 + 1;
                    break;
                case 'M':     // minutes, 00 - 59
                    num = d->dt().time().minute();
                    numLength = 2;
                    break;
                case 'S':     // seconds, 00 - 59
                    num = d->dt().time().second();
                    numLength = 2;
                    break;
                case 'P':     // am/pm
                {
                    bool am = (d->dt().time().hour() < 12);
                    QString ap = ki18n(am ? "am" : "pm").toString(locale);
                    if (ap.isEmpty())
                        result += am ? QLatin1String("am") : QLatin1String("pm");
                    else
                        result += ap;
                    break;
                }
                case 'p':     // AM/PM
                {
                    bool am = (d->dt().time().hour() < 12);
                    QString ap = ki18n(am ? "am" : "pm").toString(locale).toUpper();
                    if (ap.isEmpty())
                        result += am ? QLatin1String("AM") : QLatin1String("PM");
                    else
                        result += ap;
                    break;
                }
                case 'z':     // UTC offset in hours and minutes
                    zone = UTCOffset;
                    break;
                case 'Z':     // time zone abbreviation
                    zone = TZAbbrev;
                    break;
                default:
                    result += QLatin1Char('%');
                    result += format[i];
                    break;
            }
        }
        else if (flag == ':')
        {
            // It's a "%:" sequence
            switch (ch)
            {
                case 'A':     // week day name in English
                    result += longDay[d->date().dayOfWeek() - 1];
                    break;
                case 'a':     // week day name in English, short
                    result += shortDay[d->date().dayOfWeek() - 1];
                    break;
                case 'B':     // month name in English
                    result += longMonth[d->date().month() - 1];
                    break;
                case 'b':     // month name in English, short
                    result += shortMonth[d->date().month() - 1];
                    break;
                case 'm':     // month, 1 - 12
                    num = d->date().month();
                    break;
                case 'P':     // am/pm
                    result += (d->dt().time().hour() < 12) ? QLatin1String("am") : QLatin1String("pm");
                    break;
                case 'p':     // AM/PM
                    result += (d->dt().time().hour() < 12) ? QLatin1String("AM") : QLatin1String("PM");
                    break;
                case 'S':     // seconds with ':' prefix, only if non-zero
                {
                    int sec = d->dt().time().second();
                    if (sec || d->dt().time().msec())
                    {
                        result += QLatin1Char(':');
                        num = sec;
                        numLength = 2;
                    }
                    break;
                }
                case 's':     // milliseconds
                    result += s.sprintf("%03d", d->dt().time().msec());
                    break;
                case 'u':     // UTC offset in hours
                    zone = UTCOffsetShort;
                    break;
                case 'z':     // UTC offset in hours and minutes, with colon
                    zone = UTCOffsetColon;
                    break;
                case 'Z':     // time zone name
                    zone = TZName;
                    break;
                default:
                    result += QLatin1String("%:");
                    result += format[i];
                    break;
            }
            flag = 0;
        }
        if (!flag)
            escape = false;

        // Append any required number or time zone information
        if (num != NO_NUMBER)
        {
            if (!numLength)
                result += QString::number(num);
            else if (numLength == 2 || numLength == 4)
            {
                if (num < 0)
                {
                    num = -num;
                    result += '-';
                }
                result += s.sprintf((numLength == 2 ? "%02d" : "%04d"), num);
            }
        }
        else if (zone != TZNone)
        {
            KTimeZone tz;
            int offset;
            switch (d->specType)
            {
                case UTC:
                case TimeZone:
                    tz = (d->specType == TimeZone) ? d->specZone : KTimeZone::utc();
                    // fall through to OffsetFromUTC
                case OffsetFromUTC:
                    offset = (d->specType == TimeZone) ? d->timeZoneOffset()
                           : (d->specType == OffsetFromUTC) ? d->specUtcOffset : 0;
                    offset /= 60;
                    switch (zone)
                    {
                        case UTCOffsetShort:  // UTC offset in hours
                        case UTCOffset:       // UTC offset in hours and minutes
                        case UTCOffsetColon:  // UTC offset in hours and minutes, with colon
                        {
                            if (offset >= 0)
                                result += QLatin1Char('+');
                            else
                            {
                                result += QLatin1Char('-');
                                offset = -offset;
                            }
                            QString s;
                            result += s.sprintf(((zone == UTCOffsetColon) ? "%02d:" : "%02d"), offset/60);
                            if (ch != 'u'  ||  offset % 60)
                                result += s.sprintf("%02d", offset % 60);
                            break;
                        }
                        case TZAbbrev:     // time zone abbreviation
                            if (tz.isValid()  &&  d->specType != OffsetFromUTC)
                                result += tz.abbreviation(d->toUtc());
                            break;
                        case TZName:       // time zone name
                            if (tz.isValid()  &&  d->specType != OffsetFromUTC)
                                result += tz.name();
                            break;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    return result;
}

QString KDateTime::toString(TimeFormat format) const
{
    QString result;
    if (!isValid())
        return result;

    QString s;
    char tzsign = '+';
    int offset = 0;
    const char *tzcolon = "";
    KTimeZone tz;
    switch (format)
    {
        case RFCDateDay:
            result += shortDay[d->date().dayOfWeek() - 1];
            result += QLatin1String(", ");
            // fall through to RFCDate
        case RFCDate:
        {
            char seconds[8] = { 0 };
            if (d->dt().time().second())
                sprintf(seconds, ":%02d", d->dt().time().second());
            result += s.sprintf("%02d %s ", d->date().day(), shortMonth[d->date().month() - 1]);
            int year = d->date().year();
            if (year < 0)
            {
                result += QLatin1Char('-');
                year = -year;
            }
            result += s.sprintf("%04d %02d:%02d%s ",
                                year, d->dt().time().hour(), d->dt().time().minute(), seconds);
            if (d->specType == ClockTime)
                tz = KSystemTimeZones::local();
            break;
        }
        case ISODate:
        {
            // QDateTime::toString(Qt::ISODate) doesn't output fractions of a second
            int year = d->date().year();
            if (year < 0)
            {
                result += QLatin1Char('-');
                year = -year;
            }
            QString s;
            result += s.sprintf("%04d-%02d-%02d",
                                year, d->date().month(), d->date().day());
            if (!d->dateOnly()  ||  d->specType != ClockTime)
            {
                result += s.sprintf("T%02d:%02d:%02d",
                                    d->dt().time().hour(), d->dt().time().minute(), d->dt().time().second());
                if (d->dt().time().msec())
                {
                    // Comma is preferred by ISO8601 as the decimal point symbol,
                    // so use it unless '.' is the symbol used in this locale or we don't have a locale.
                    KLocale *locale = KGlobal::locale();
                    result += (locale && locale->decimalSymbol() == QLatin1String(".")) ? QLatin1Char('.') : QLatin1Char(',');
                    result += s.sprintf("%03d", d->dt().time().msec());
                }
            }
            if (d->specType == UTC)
                return result + QLatin1Char('Z');
            if (d->specType == ClockTime)
                return result;
            tzcolon = ":";
            break;
        }
            // fall through to QtTextDate
        case QtTextDate:
        case LocalDate:
        {
            Qt::DateFormat qtfmt = (format == QtTextDate) ? Qt::TextDate : Qt::LocalDate;
            if (d->dateOnly())
                result = d->date().toString(qtfmt);
            else
                result = d->dt().toString(qtfmt);
            if (result.isEmpty()  ||  d->specType == ClockTime)
                return result;
            result += QLatin1Char(' ');
            break;
        }
        default:
            return result;
    }

    // Return the string with UTC offset ±hhmm appended
    if (d->specType == OffsetFromUTC  ||  d->specType == TimeZone  ||  tz.isValid())
    {
        if (d->specType == TimeZone)
            offset = d->timeZoneOffset();   // calculate offset and cache UTC value
        else
            offset = tz.isValid() ? tz.offsetAtZoneTime(d->dt()) : d->specUtcOffset;
        if (offset < 0)
        {
            offset = -offset;
            tzsign = '-';
        }
    }
    offset /= 60;
    return result + s.sprintf("%c%02d%s%02d", tzsign, offset/60, tzcolon, offset%60);
}

KDateTime KDateTime::fromString(const QString &string, TimeFormat format, bool *negZero)
{
    if (negZero)
        *negZero = false;
    QString str = string.trimmed();
    if (str.isEmpty())
        return KDateTime();

    switch (format)
    {
        case RFCDateDay: // format is Wdy, DD Mon YYYY hh:mm:ss ±hhmm
        case RFCDate:    // format is [Wdy,] DD Mon YYYY hh:mm[:ss] ±hhmm
        {
            int nyear  = 6;   // indexes within string to values
            int nmonth = 4;
            int nday   = 2;
            int nwday  = 1;
            int nhour  = 7;
            int nmin   = 8;
            int nsec   = 9;
            // Also accept obsolete form "Weekday, DD-Mon-YY HH:MM:SS ±hhmm"
            QRegExp rx("^(?:([A-Z][a-z]+),\\s*)?(\\d{1,2})(\\s+|-)([^-\\s]+)(\\s+|-)(\\d{2,4})\\s+(\\d\\d):(\\d\\d)(?::(\\d\\d))?\\s+(\\S+)$");
            QStringList parts;
            if (!str.indexOf(rx))
            {
                // Check that if date has '-' separators, both separators are '-'.
                parts = rx.capturedTexts();
                bool h1 = (parts[3] == QLatin1String("-"));
                bool h2 = (parts[5] == QLatin1String("-"));
                if (h1 != h2)
                    break;
            }
            else
            {
                // Check for the obsolete form "Wdy Mon DD HH:MM:SS YYYY"
                rx = QRegExp("^([A-Z][a-z]+)\\s+(\\S+)\\s+(\\d\\d)\\s+(\\d\\d):(\\d\\d):(\\d\\d)\\s+(\\d\\d\\d\\d)$");
                if (str.indexOf(rx))
                    break;
                nyear  = 7;
                nmonth = 2;
                nday   = 3;
                nwday  = 1;
                nhour  = 4;
                nmin   = 5;
                nsec   = 6;
                parts = rx.capturedTexts();
            }
            bool ok[4];
            int day    = parts[nday].toInt(&ok[0]);
            int year   = parts[nyear].toInt(&ok[1]);
            int hour   = parts[nhour].toInt(&ok[2]);
            int minute = parts[nmin].toInt(&ok[3]);
            if (!ok[0] || !ok[1] || !ok[2] || !ok[3])
                break;
            int second = 0;
            if (!parts[nsec].isEmpty())
            {
                second = parts[nsec].toInt(&ok[0]);
                if (!ok[0])
                    break;
            }
            bool leapSecond = (second == 60);
            if (leapSecond)
                second = 59;   // apparently a leap second - validate below, once time zone is known
            int month = 0;
            for ( ;  month < 12  &&  parts[nmonth] != shortMonth[month];  ++month) ;
            int dayOfWeek = -1;
            if (!parts[nwday].isEmpty())
            {
                // Look up the weekday name
                while (++dayOfWeek < 7  &&  shortDay[dayOfWeek] != parts[nwday]) ;
                if (dayOfWeek >= 7)
                    for (dayOfWeek = 0;  dayOfWeek < 7  &&  longDay[dayOfWeek] != parts[nwday];  ++dayOfWeek) ;
            }
            if (month >= 12 || dayOfWeek >= 7
            ||  (dayOfWeek < 0  &&  format == RFCDateDay))
                break;
            int i = parts[nyear].size();
            if (i < 4)
            {
                // It's an obsolete year specification with less than 4 digits
                year += (i == 2  &&  year < 50) ? 2000 : 1900;
            }

            // Parse the UTC offset part
            int offset = 0;           // set default to '-0000'
            bool negOffset = false;
            if (parts.count() > 10)
            {
                rx = QRegExp("^([+-])(\\d\\d)(\\d\\d)$");
                if (!parts[10].indexOf(rx))
                {
                    // It's a UTC offset ±hhmm
                    parts = rx.capturedTexts();
                    offset = parts[2].toInt(&ok[0]) * 3600;
                    int offsetMin = parts[3].toInt(&ok[1]);
                    if (!ok[0] || !ok[1] || offsetMin > 59)
                        break;
                    offset += offsetMin * 60;
                    negOffset = (parts[1] == QLatin1String("-"));
                    if (negOffset)
                        offset = -offset;
                }
                else
                {
                    // Check for an obsolete time zone name
                    QByteArray zone = parts[10].toLatin1();
                    if (zone.length() == 1  &&  isalpha(zone[0])  &&  toupper(zone[0]) != 'J')
                        negOffset = true;    // military zone: RFC 2822 treats as '-0000'
                    else if (zone != "UT" && zone != "GMT")    // treated as '+0000'
                    {
                        offset = (zone == "EDT")                  ? -4*3600
                               : (zone == "EST" || zone == "CDT") ? -5*3600
                               : (zone == "CST" || zone == "MDT") ? -6*3600
                               : (zone == "MST" || zone == "PDT") ? -7*3600
                               : (zone == "PST")                  ? -8*3600
                               : 0;
                        if (!offset)
                        {
                            // Check for any other alphabetic time zone
                            bool nonalpha = false;
                            for (int i = 0, end = zone.size();  i < end && !nonalpha;  ++i)
                                nonalpha = !isalpha(zone[i]);
                            if (nonalpha)
                                break;
                            // TODO: Attempt to recognize the time zone abbreviation?
                            negOffset = true;    // unknown time zone: RFC 2822 treats as '-0000'
                        }
                    }
                }
            }
            Status invalid = stValid;
            QDate qdate = checkDate(year, month+1, day, invalid);   // convert date, and check for out-of-range
            if (!qdate.isValid())
                break;
            KDateTime result(qdate, QTime(hour, minute, second), Spec(OffsetFromUTC, offset));
            if (!result.isValid()
            ||  (dayOfWeek >= 0  &&  result.date().dayOfWeek() != dayOfWeek+1))
                break;    // invalid date/time, or weekday doesn't correspond with date
            if (!offset)
            {
                if (negOffset && negZero)
                    *negZero = true;   // UTC offset given as "-0000"
                result.setTimeSpec(UTC);
            }
            if (leapSecond)
            {
                // Validate a leap second time. Leap seconds are inserted after 23:59:59 UTC.
                // Convert the time to UTC and check that it is 00:00:00.
                if ((hour*3600 + minute*60 + 60 - offset + 86400*5) % 86400)   // (max abs(offset) is 100 hours)
                    break;    // the time isn't the last second of the day
            }
            if (invalid)
            {
                KDateTime dt;            // date out of range - return invalid KDateTime ...
                dt.d->status = invalid;  // ... with reason for error
                return dt;
            }
            return result;
        }
        case ISODate:
        {
            /*
             * Extended format: [±]YYYY-MM-DD[Thh[:mm[:ss.s]][TZ]]
             * Basic format:    [±]YYYYMMDD[Thh[mm[ss.s]][TZ]]
             * Extended format: [±]YYYY-DDD[Thh[:mm[:ss.s]][TZ]]
             * Basic format:    [±]YYYYDDD[Thh[mm[ss.s]][TZ]]
             * In the first three formats, the year may be expanded to more than 4 digits.
             *
             * QDateTime::fromString(Qt::ISODate) is a rather limited implementation
             * of parsing ISO 8601 format date/time strings, so it isn't used here.
             * This implementation isn't complete either, but it's better.
             *
             * ISO 8601 allows truncation, but for a combined date & time, the date part cannot
             * be truncated from the right, and the time part cannot be truncated from the left.
             * In other words, only the outer parts of the string can be omitted.
             * The standard does not actually define how to interpret omitted parts - it is up
             * to those interchanging the data to agree on a scheme.
             */
            bool dateOnly = false;
            // Check first for the extended format of ISO 8601
            QRegExp rx("^([+-])?(\\d{4,})-(\\d\\d\\d|\\d\\d-\\d\\d)[T ](\\d\\d)(?::(\\d\\d)(?::(\\d\\d)(?:(?:\\.|,)(\\d+))?)?)?(Z|([+-])(\\d\\d)(?::(\\d\\d))?)?$");
            if (str.indexOf(rx))
            {
                // It's not the extended format - check for the basic format
                rx = QRegExp("^([+-])?(\\d{4,})(\\d{4})[T ](\\d\\d)(?:(\\d\\d)(?:(\\d\\d)(?:(?:\\.|,)(\\d+))?)?)?(Z|([+-])(\\d\\d)(\\d\\d)?)?$");
                if (str.indexOf(rx))
                {
                    rx = QRegExp("^([+-])?(\\d{4})(\\d{3})[T ](\\d\\d)(?:(\\d\\d)(?:(\\d\\d)(?:(?:\\.|,)(\\d+))?)?)?(Z|([+-])(\\d\\d)(\\d\\d)?)?$");
                    if (str.indexOf(rx))
                    {
                        // Check for date-only formats
                        dateOnly = true;
                        rx = QRegExp("^([+-])?(\\d{4,})-(\\d\\d\\d|\\d\\d-\\d\\d)$");
                        if (str.indexOf(rx))
                        {
                            // It's not the extended format - check for the basic format
                            rx = QRegExp("^([+-])?(\\d{4,})(\\d{4})$");
                            if (str.indexOf(rx))
                            {
                                rx = QRegExp("^([+-])?(\\d{4})(\\d{3})$");
                                if (str.indexOf(rx))
                                    break;
                            }
                        }
                    }
                }
            }
            QStringList parts = rx.capturedTexts();
            bool ok, ok1;
            QDate d;
            int hour   = 0;
            int minute = 0;
            int second = 0;
            int msecs  = 0;
            bool leapSecond = false;
            int year = parts[2].toInt(&ok);
            if (!ok)
                break;
            if (parts[1] == QLatin1String("-"))
                year = -year;
            if (!dateOnly)
            {
                hour = parts[4].toInt(&ok);
                if (!ok)
                    break;
                if (!parts[5].isEmpty())
                {
                    minute = parts[5].toInt(&ok);
                    if (!ok)
                        break;
                }
                if (!parts[6].isEmpty())
                {
                    second = parts[6].toInt(&ok);
                    if (!ok)
                        break;
                }
                leapSecond = (second == 60);
                if (leapSecond)
                    second = 59;   // apparently a leap second - validate below, once time zone is known
                if (!parts[7].isEmpty())
                {
                    QString ms = parts[7] + QLatin1String("00");
                    ms.truncate(3);
                    msecs = ms.toInt(&ok);
                    if (!ok)
                        break;
                }
            }
            int month, day;
            Status invalid = stValid;
            if (parts[3].length() == 3)
            {
                // A day of the year is specified
                day = parts[3].toInt(&ok);
                if (!ok || day < 1 || day > 366)
                    break;
                d = checkDate(year, 1, 1, invalid).addDays(day - 1);   // convert date, and check for out-of-range
                if (!d.isValid()  ||  (!invalid && d.year() != year))
                    break;
                day   = d.day();
                month = d.month();
            }
            else
            {
                // A month and day are specified
                month = parts[3].left(2).toInt(&ok);
                day   = parts[3].right(2).toInt(&ok1);
                if (!ok || !ok1)
                    break;
                d = checkDate(year, month, day, invalid);   // convert date, and check for out-of-range
                if (!d.isValid())
                    break;
            }
            if (dateOnly)
            {
                if (invalid)
                {
                    KDateTime dt;            // date out of range - return invalid KDateTime ...
                    dt.d->status = invalid;    // ... with reason for error
                    return dt;
                }
                return KDateTime(d, Spec(ClockTime));
            }
            if (hour == 24  && !minute && !second && !msecs)
            {
                // A time of 24:00:00 is allowed by ISO 8601, and means midnight at the end of the day
                d = d.addDays(1);
                hour = 0;
            }

            QTime t(hour, minute, second, msecs);
            if (!t.isValid())
                break;
            if (parts[8].isEmpty())
            {
                // No UTC offset is specified. Don't try to validate leap seconds.
                if (invalid)
                {
                    KDateTime dt;            // date out of range - return invalid KDateTime ...
                    dt.d->status = invalid;  // ... with reason for error
                    return dt;
                }
                return KDateTime(d, t, KDateTimePrivate::fromStringDefault());
            }
            int offset = 0;
            SpecType spec = (parts[8] == QLatin1String("Z")) ? UTC : OffsetFromUTC;
            if (spec == OffsetFromUTC)
            {
                offset = parts[10].toInt(&ok) * 3600;
                if (!ok)
                    break;
                if (!parts[11].isEmpty())
                {
                    offset += parts[11].toInt(&ok) * 60;
                    if (!ok)
                        break;
                }
                if (parts[9] == QLatin1String("-"))
                {
                    offset = -offset;
                    if (!offset && negZero)
                        *negZero = true;
                }
            }
            if (leapSecond)
            {
                // Validate a leap second time. Leap seconds are inserted after 23:59:59 UTC.
                // Convert the time to UTC and check that it is 00:00:00.
                if ((hour*3600 + minute*60 + 60 - offset + 86400*5) % 86400)   // (max abs(offset) is 100 hours)
                    break;    // the time isn't the last second of the day
            }
            if (invalid)
            {
                KDateTime dt;            // date out of range - return invalid KDateTime ...
                dt.d->status = invalid;  // ... with reason for error
                return dt;
            }
            return KDateTime(d, t, Spec(spec, offset));
        }
        case QtTextDate:    // format is Wdy Mth DD [hh:mm:ss] YYYY [±hhmm]
        {
            int offset = 0;
            QRegExp rx("^(\\S+\\s+\\S+\\s+\\d\\d\\s+(\\d\\d:\\d\\d:\\d\\d\\s+)?\\d\\d\\d\\d)\\s*(.*)$");
            if (str.indexOf(rx) < 0)
                break;
            QStringList parts = rx.capturedTexts();
            QDate     qd;
            QDateTime qdt;
            bool dateOnly = parts[2].isEmpty();
            if (dateOnly)
            {
                qd = QDate::fromString(parts[1], Qt::TextDate);
                if (!qd.isValid())
                    break;
            }
            else
            {
                qdt = QDateTime::fromString(parts[1], Qt::TextDate);
                if (!qdt.isValid())
                    break;
            }
            if (parts[3].isEmpty())
            {
                // No time zone offset specified, so return a local clock time
                if (dateOnly)
                    return KDateTime(qd, KDateTimePrivate::fromStringDefault());
                else
                {
                    // Do it this way to prevent UTC conversions changing the time
                    return KDateTime(qdt.date(), qdt.time(), KDateTimePrivate::fromStringDefault());
                }
            }
            rx = QRegExp("([+-])([\\d][\\d])(?::?([\\d][\\d]))?$");
            if (parts[3].indexOf(rx) < 0)
                break;

            // Extract the UTC offset at the end of the string
            bool ok;
            parts = rx.capturedTexts();
            offset = parts[2].toInt(&ok) * 3600;
            if (!ok)
                break;
            if (parts.count() > 3)
            {
                offset += parts[3].toInt(&ok) * 60;
                if (!ok)
                    break;
            }
            if (parts[1] == QLatin1String("-"))
            {
                offset = -offset;
                if (!offset && negZero)
                    *negZero = true;
            }
            if (dateOnly)
                return KDateTime(qd, Spec((offset ? OffsetFromUTC : UTC), offset));
            qdt.setTimeSpec(offset ? Qt::LocalTime : Qt::UTC);
            return KDateTime(qdt, Spec((offset ? OffsetFromUTC : UTC), offset));
        }
        case LocalDate:
        default:
            break;
    }
    return KDateTime();
}

KDateTime KDateTime::fromString(const QString &string, const QString &format,
                                const KTimeZones *zones, bool offsetIfAmbiguous)
{
    int     utcOffset;    // UTC offset in seconds
    bool    dateOnly = false;
    Status invalid = stValid;
    QString zoneName;
    QByteArray zoneAbbrev;
    QDateTime qdt = fromStr(string, format, utcOffset, zoneName, zoneAbbrev, dateOnly, invalid);
    if (!qdt.isValid())
        return KDateTime();
    if (zones)
    {
        // Try to find a time zone match
        bool zname = false;
        KTimeZone zone;
        if (!zoneName.isEmpty())
        {
            // A time zone name has been found.
            // Use the time zone with that name.
            zone = zones->zone(zoneName);
            zname = true;
        }
        else if (!invalid)
        {
            if (!zoneAbbrev.isEmpty())
            {
                // A time zone abbreviation has been found.
                // Use the time zone which contains it, if any, provided that the
                // abbreviation applies at the specified date/time.
                bool useUtcOffset = false;
                const KTimeZones::ZoneMap z = zones->zones();
                for (KTimeZones::ZoneMap::ConstIterator it = z.begin();  it != z.end();  ++it)
                {
                    if (it.value().abbreviations().contains(zoneAbbrev))
                    {
                        int offset2;
                        int offset = it.value().offsetAtZoneTime(qdt, &offset2);
                        QDateTime ut(qdt);
                        ut.setTimeSpec(Qt::UTC);
                        ut.addSecs(-offset);
                        if (it.value().abbreviation(ut) != zoneAbbrev)
                        {
                            if (offset == offset2)
                                continue;     // abbreviation doesn't apply at specified time
                            ut.addSecs(offset - offset2);
                            if (it.value().abbreviation(ut) != zoneAbbrev)
                                continue;     // abbreviation doesn't apply at specified time
                            offset = offset2;
                        }
                        // Found a time zone which uses this abbreviation at the specified date/time
                        if (zone.isValid())
                        {
                            // Abbreviation is used by more than one time zone
                            if (!offsetIfAmbiguous  ||  offset != utcOffset)
                                return KDateTime();
                            useUtcOffset = true;
                        }
                        else
                        {
                            zone = it.value();
                            utcOffset = offset;
                        }
                    }
                }
                if (useUtcOffset)
                {
                    zone = KTimeZone();
                    if (!utcOffset)
                        qdt.setTimeSpec(Qt::UTC);
                }
                else
                    zname = true;
            }
            else if (utcOffset  ||  qdt.timeSpec() == Qt::UTC)
            {
                // A UTC offset has been found.
                // Use the time zone which contains it, if any.
                // For a date-only value, use the start of the day.
                QDateTime dtUTC = qdt;
                dtUTC.setTimeSpec(Qt::UTC);
                dtUTC.addSecs(-utcOffset);
                const KTimeZones::ZoneMap z = zones->zones();
                for (KTimeZones::ZoneMap::ConstIterator it = z.begin();  it != z.end();  ++it)
                {
                    QList<int> offsets = it.value().utcOffsets();
                    if ((offsets.isEmpty() || offsets.contains(utcOffset))
                    &&  it.value().offsetAtUtc(dtUTC) == utcOffset)
                    {
                        // Found a time zone which uses this offset at the specified time
                        if (zone.isValid()  ||  !utcOffset)
                        {
                            // UTC offset is used by more than one time zone
                            if (!offsetIfAmbiguous)
                                return KDateTime();
                            if (invalid)
                            {
                                KDateTime dt;            // date out of range - return invalid KDateTime ...
                                dt.d->status = invalid;    // ... with reason for error
                                return dt;
                            }
                            if (dateOnly)
                                return KDateTime(qdt.date(), Spec(OffsetFromUTC, utcOffset));
                            qdt.setTimeSpec(Qt::LocalTime);
                            return KDateTime(qdt, Spec(OffsetFromUTC, utcOffset));
                        }
                        zone = it.value();
                    }
                }
            }
        }
        if (!zone.isValid() && zname)
            return KDateTime();    // an unknown zone name or abbreviation was found
        if (zone.isValid() && !invalid)
        {
            if (dateOnly)
                return KDateTime(qdt.date(), Spec(zone));
            return KDateTime(qdt, Spec(zone));
        }
    }

    // No time zone match was found
    if (invalid)
    {
        KDateTime dt;            // date out of range - return invalid KDateTime ...
        dt.d->status = invalid;    // ... with reason for error
        return dt;
    }
    KDateTime result;
    if (utcOffset)
    {
        qdt.setTimeSpec(Qt::LocalTime);
        result = KDateTime(qdt, Spec(OffsetFromUTC, utcOffset));
    }
    else if (qdt.timeSpec() == Qt::UTC)
        result = KDateTime(qdt, UTC);
    else
    {
        result = KDateTime(qdt, Spec(ClockTime));
        result.setTimeSpec(KDateTimePrivate::fromStringDefault());
    }
    if (dateOnly)
        result.setDateOnly(true);
    return result;
}

void KDateTime::setFromStringDefault(const Spec &spec)
{
    KDateTimePrivate::fromStringDefault() = spec;
}

QDataStream & operator<<(QDataStream &s, const KDateTime &dt)
{
    s << dt.dateTime() << dt.timeSpec() << quint8(dt.isDateOnly() ? 0x01 : 0x00);
    return s;
}

QDataStream & operator>>(QDataStream &s, KDateTime &kdt)
{
    QDateTime dt;
    KDateTime::Spec spec;
    quint8 flags;
    s >> dt >> spec >> flags;
    kdt.setDateTime(dt);
    kdt.setTimeSpec(spec);
    if (flags & 0x01)
        kdt.setDateOnly(true);
    return s;
}


/*
 * Extracts a QDateTime from a string, given a format string.
 * The date/time is set to Qt::UTC if a zero UTC offset is found,
 * otherwise it is Qt::LocalTime. If Qt::LocalTime is returned and
 * utcOffset == 0, that indicates that no UTC offset was found.
 */
QDateTime fromStr(const QString& string, const QString& format, int& utcOffset,
                  QString& zoneName, QByteArray& zoneAbbrev, bool& dateOnly, Status &status)
{
    status = stValid;
    QString str = string.simplified();
    int year      = NO_NUMBER;
    int month     = NO_NUMBER;
    int day       = NO_NUMBER;
    int dayOfWeek = NO_NUMBER;
    int hour      = NO_NUMBER;
    int minute    = NO_NUMBER;
    int second    = NO_NUMBER;
    int millisec  = NO_NUMBER;
    int ampm      = NO_NUMBER;
    int tzoffset  = NO_NUMBER;
    zoneName.clear();
    zoneAbbrev.clear();

    enum { TZNone, UTCOffset, UTCOffsetColon, TZAbbrev, TZName };
    KLocale *locale = KGlobal::locale();
    KCalendarSystemGregorian calendar(locale);
    int zone;
    int s = 0;
    int send = str.length();
    bool escape = false;
    ushort flag = 0;
    for (int f = 0, fend = format.length();  f < fend && s < send;  ++f)
    {
        zone = TZNone;
        ushort ch = format[f].unicode();
        if (!escape)
        {
            if (ch == '%')
                escape = true;
            else if (format[f].isSpace())
            {
                if (str[s].isSpace())
                    ++s;
            }
            else if (format[f] == str[s])
                ++s;
            else
                return QDateTime();
            continue;
        }
        if (!flag)
        {
            switch (ch)
            {
                case '%':
                    if (str[s++] != QLatin1Char('%'))
                        return QDateTime();
                    break;
                case ':':
                    flag = ch;
                    break;
                case 'Y':     // full year, 4 digits
                    if (!getNumber(str, s, 4, 4, NO_NUMBER, -1, year))
                        return QDateTime();
                    break;
                case 'y':     // year, 2 digits
                    if (!getNumber(str, s, 2, 2, 0, 99, year))
                        return QDateTime();
                    year += (year <= 50) ? 2000 : 1999;
                    break;
                case 'm':     // month, 2 digits, 01 - 12
                    if (!getNumber(str, s, 2, 2, 1, 12, month))
                        return QDateTime();
                    break;
                case 'B':
                case 'b':     // month name, translated or English
                {
                    int m = matchMonth(str, s, &calendar);
                    if (m <= 0  ||  month != NO_NUMBER && month != m)
                        return QDateTime();
                    month = m;
                    break;
                }
                case 'd':     // day of month, 2 digits, 01 - 31
                    if (!getNumber(str, s, 2, 2, 1, 31, day))
                        return QDateTime();
                    break;
                case 'e':     // day of month, 1 - 31
                    if (!getNumber(str, s, 1, 2, 1, 31, day))
                        return QDateTime();
                    break;
                case 'A':
                case 'a':     // week day name, translated or English
                {
                    int dow = matchDay(str, s, &calendar);
                    if (dow <= 0  ||  dayOfWeek != NO_NUMBER && dayOfWeek != dow)
                        return QDateTime();
                    dayOfWeek = dow;
                    break;
                }
                case 'H':     // hour, 2 digits, 00 - 23
                    if (!getNumber(str, s, 2, 2, 0, 23, hour))
                        return QDateTime();
                    break;
                case 'k':     // hour, 0 - 23
                    if (!getNumber(str, s, 1, 2, 0, 23, hour))
                        return QDateTime();
                    break;
                case 'I':     // hour, 2 digits, 01 - 12
                    if (!getNumber(str, s, 2, 2, 1, 12, hour))
                        return QDateTime();
                    break;
                case 'l':     // hour, 1 - 12
                    if (!getNumber(str, s, 1, 2, 1, 12, hour))
                        return QDateTime();
                    break;
                case 'M':     // minutes, 2 digits, 00 - 59
                    if (!getNumber(str, s, 2, 2, 0, 59, minute))
                        return QDateTime();
                    break;
                case 'S':     // seconds, 2 digits, 00 - 59
                    if (!getNumber(str, s, 2, 2, 0, 59, second))
                        return QDateTime();
                    break;
                case 's':     // seconds, 0 - 59
                    if (!getNumber(str, s, 1, 2, 0, 59, second))
                        return QDateTime();
                    break;
                case 'P':
                case 'p':     // am/pm
                {
                    int ap = getAmPm(str, s, locale);
                    if (!ap  ||  ampm != NO_NUMBER && ampm != ap)
                        return QDateTime();
                    ampm = ap;
                    break;
                }
                case 'z':     // UTC offset in hours and optionally minutes
                    zone = UTCOffset;
                    break;
                case 'Z':     // time zone abbreviation
                    zone = TZAbbrev;
                    break;
                case 't':     // whitespace
                    if (str[s++] != QLatin1Char(' '))
                        return QDateTime();
                    break;
                default:
                    if (s + 2 > send
                    ||  str[s++] != QLatin1Char('%')
                    ||  str[s++] != format[f])
                        return QDateTime();
                    break;
            }
        }
        else if (flag == ':')
        {
            // It's a "%:" sequence
            switch (ch)
            {
                case 'Y':     // full year, >= 4 digits
                    if (!getNumber(str, s, 4, 100, NO_NUMBER, -1, year))
                        return QDateTime();
                    break;
                case 'A':
                case 'a':     // week day name in English
                {
                    int dow = matchDay(str, s, 0);
                    if (dow <= 0  ||  dayOfWeek != NO_NUMBER && dayOfWeek != dow)
                        return QDateTime();
                    dayOfWeek = dow;
                    break;
                }
                case 'B':
                case 'b':     // month name in English
                {
                    int m = matchMonth(str, s, 0);
                    if (m <= 0  ||  month != NO_NUMBER && month != m)
                        return QDateTime();
                    month = m;
                    break;
                }
                case 'm':     // month, 1 - 12
                    if (!getNumber(str, s, 1, 2, 1, 12, month))
                        return QDateTime();
                    break;
                case 'P':
                case 'p':     // am/pm in English
                {
                    int ap = getAmPm(str, s, 0);
                    if (!ap  ||  ampm != NO_NUMBER && ampm != ap)
                        return QDateTime();
                    ampm = ap;
                    break;
                }
                case 'M':     // minutes, 0 - 59
                    if (!getNumber(str, s, 1, 2, 0, 59, minute))
                        return QDateTime();
                    break;
                case 'S':     // seconds with ':' prefix, defaults to zero
                    if (str[s] != QLatin1Char(':'))
                    {
                        second = 0;
                        break;
                    }
                    ++s;
                    if (!getNumber(str, s, 1, 2, 0, 59, second))
                        return QDateTime();
                    break;
                case 's':     // milliseconds, with decimal point prefix
                {
                    if (str[s] != QLatin1Char('.'))
                    {
                        // If no locale, try comma, it is preferred by ISO8601 as the decimal point symbol
                        QString dpt = locale == 0 ? "," : locale->decimalSymbol();
                        if (!str.mid(s).startsWith(dpt))
                            return QDateTime();
                        s += dpt.length() - 1;
                    }
                    ++s;
                    if (s >= send)
                        return QDateTime();
                    QString val = str.mid(s);
                    int i = 0;
                    for (int end = val.length();  i < end && val[i].isDigit();  ++i) ;
                    if (!i)
                        return QDateTime();
                    val.truncate(i);
                    val += QLatin1String("00");
                    val.truncate(3);
                    int ms = val.toInt();
                    if (millisec != NO_NUMBER && millisec != ms)
                        return QDateTime();
                    millisec = ms;
                    s += i;
                    break;
                }
                case 'u':     // UTC offset in hours and optionally minutes
                    zone = UTCOffset;
                    break;
                case 'z':     // UTC offset in hours and minutes, with colon
                    zone = UTCOffsetColon;
                    break;
                case 'Z':     // time zone name
                    zone = TZName;
                    break;
                default:
                    if (s + 3 > send
                    ||  str[s++] != QLatin1Char('%')
                    ||  str[s++] != QLatin1Char(':')
                    ||  str[s++] != format[f])
                        return QDateTime();
                    break;
            }
            flag = 0;
        }
        if (!flag)
            escape = false;

        if (zone != TZNone)
        {
            // Read time zone or UTC offset
            switch (zone)
            {
                case UTCOffset:
                case UTCOffsetColon:
                    if (!zoneAbbrev.isEmpty() || !zoneName.isEmpty())
                        return QDateTime();
                    if (!getUTCOffset(str, s, (zone == UTCOffsetColon), tzoffset))
                        return QDateTime();
                    break;
                case TZAbbrev:     // time zone abbreviation
                {
                    if (tzoffset != NO_NUMBER || !zoneName.isEmpty())
                        return QDateTime();
                    int start = s;
                    while (s < send && str[s].isLetterOrNumber())
                        ++s;
                    if (s == start)
                        return QDateTime();
                    QString z = str.mid(start, s - start);
                    if (!zoneAbbrev.isEmpty()  &&  z.toLatin1() != zoneAbbrev)
                        return QDateTime();
                    zoneAbbrev = z.toLatin1();
                    break;
                }
                case TZName:       // time zone name
                {
                    if (tzoffset != NO_NUMBER || !zoneAbbrev.isEmpty())
                        return QDateTime();
                    QString z;
                    if (f + 1 >= fend)
                    {
                        z = str.mid(s);
                        s = send;
                    }
                    else
                    {
                        // Get the terminating character for the zone name
                        QChar endchar = format[f + 1];
                        if (endchar == QLatin1Char('%')  &&  f + 2 < fend)
                        {
                            QChar endchar2 = format[f + 2];
                            if (endchar2 == QLatin1Char('n') || endchar2 == QLatin1Char('t'))
                                endchar = QLatin1Char(' ');
                        }
                        // Extract from the input string up to the terminating character
                        int start = s;
                        for ( ;  s < send && str[s] != endchar;  ++s) ;
                        if (s == start)
                            return QDateTime();
                        z = str.mid(start, s - start);
                    }
                    if (!zoneName.isEmpty()  &&  z != zoneName)
                        return QDateTime();
                    zoneName = z;
                    break;
                }
                default:
                    break;
            }
        }
    }

    if (year == NO_NUMBER)
        year = QDate::currentDate().year();
    if (month == NO_NUMBER)
        month = 1;
    QDate d = checkDate(year, month, (day > 0 ? day : 1), status);   // convert date, and check for out-of-range
    if (!d.isValid())
        return QDateTime();
    if (dayOfWeek != NO_NUMBER  &&  !status)
    {
        if (day == NO_NUMBER)
        {
            day = 1 + dayOfWeek - QDate(year, month, 1).dayOfWeek();
            if (day <= 0)
                day += 7;
        }
        else
        {
            if (QDate(year, month, day).dayOfWeek() != dayOfWeek)
                return QDateTime();
        }
    }
    if (day == NO_NUMBER)
        day = 1;
    dateOnly = (hour == NO_NUMBER && minute == NO_NUMBER && second == NO_NUMBER && millisec == NO_NUMBER);
    if (hour == NO_NUMBER)
        hour = 0;
    if (minute == NO_NUMBER)
        minute = 0;
    if (second == NO_NUMBER)
        second = 0;
    if (millisec == NO_NUMBER)
        millisec = 0;
    if (ampm != NO_NUMBER)
    {
        if (!hour || hour > 12)
            return QDateTime();
        if (ampm == 1  &&  hour == 12)
            hour = 0;
        else if (ampm == 2  &&  hour < 12)
            hour += 12;
    }

    QDateTime dt(d, QTime(hour, minute, second, millisec), (tzoffset == 0 ? Qt::UTC : Qt::LocalTime));

    utcOffset = (tzoffset == NO_NUMBER) ? 0 : tzoffset*60;

    return dt;
}


/*
 * Find which day name matches the specified part of a string.
 * 'offset' is incremented by the length of the match.
 * Reply = day number (1 - 7), or <= 0 if no match.
 */
int matchDay(const QString &string, int &offset, KCalendarSystem *calendar)
{
    int dayOfWeek;
    QString part = string.mid(offset);
    if (part.isEmpty())
        return -1;
    if (calendar)
    {
        // Check for localised day name first
        for (dayOfWeek = 1;  dayOfWeek <= 7;  ++dayOfWeek)
        {
            QString name = calendar->weekDayName(dayOfWeek, KCalendarSystem::LongDayName);
            if (part.startsWith(name, Qt::CaseInsensitive))
            {
                offset += name.length();
                return dayOfWeek;
            }
        }
        for (dayOfWeek = 1;  dayOfWeek <= 7;  ++dayOfWeek)
        {
            QString name = calendar->weekDayName(dayOfWeek, KCalendarSystem::ShortDayName);
            if (part.startsWith(name, Qt::CaseInsensitive))
            {
                offset += name.length();
                return dayOfWeek;
            }
        }
    }

    // Check for English day name
    dayOfWeek = findString(part, longDay, 7, offset);
    if (dayOfWeek < 0)
        dayOfWeek = findString(part, shortDay, 7, offset);
    return dayOfWeek + 1;
}

/*
 * Find which month name matches the specified part of a string.
 * 'offset' is incremented by the length of the match.
 * Reply = month number (1 - 12), or <= 0 if no match.
 */
int matchMonth(const QString &string, int &offset, KCalendarSystem *calendar)
{
    int month;
    QString part = string.mid(offset);
    if (part.isEmpty())
        return -1;
    if (calendar)
    {
        // Check for localised month name first
        for (month = 1;  month <= 12;  ++month)
        {
            QString name = calendar->monthName(month, 2000, KCalendarSystem::LongName);
            if (part.startsWith(name, Qt::CaseInsensitive))
            {
                offset += name.length();
                return month;
            }
        }
        for (month = 1;  month <= 12;  ++month)
        {
            QString name = calendar->monthName(month, 2000, KCalendarSystem::ShortName);
            if (part.startsWith(name, Qt::CaseInsensitive))
            {
                offset += name.length();
                return month;
            }
        }
    }
    // Check for English month name
    month = findString(part, longMonth, 12, offset);
    if (month < 0)
        month = findString(part, shortMonth, 12, offset);
    return month + 1;
}

/*
 * Read a UTC offset from the input string.
 */
bool getUTCOffset(const QString &string, int &offset, bool colon, int &result)
{
    int sign;
    int len = string.length();
    if (offset >= len)
        return false;
    switch (string[offset++].unicode())
    {
        case '+':
            sign = 1;
            break;
        case '-':
            sign = -1;
            break;
        default:
            return false;
    }
    int tzhour = NO_NUMBER;
    int tzmin  = NO_NUMBER;
    if (!getNumber(string, offset, 2, 2, 0, 99, tzhour))
        return false;
    if (colon)
    {
        if (offset >= len  ||  string[offset++] != QLatin1Char(':'))
            return false;
    }
    if (offset >= len  ||  !string[offset].isDigit())
        tzmin = 0;
    else
    {
        if (!getNumber(string, offset, 2, 2, 0, 59, tzmin))
            return false;
    }
    tzmin += tzhour * 60;
    if (result != NO_NUMBER  &&  result != tzmin)
        return false;
    result = tzmin;
    return true;
}

/*
 * Read an am/pm indicator from the input string.
 * 'offset' is incremented by the length of the match.
 * Reply = 1 (am), 2 (pm), or 0 if no match.
 */
int getAmPm(const QString &string, int &offset, KLocale *locale)
{
    QString part = string.mid(offset);
    int ap = 0;
    int n = 2;
    if (locale)
    {
        // Check localised form first
        QString aps = ki18n("am").toString(locale);
        if (part.startsWith(aps, Qt::CaseInsensitive))
        {
            ap = 1;
            n = aps.length();
        }
        else
        {
            aps = ki18n("pm").toString(locale);
            if (part.startsWith(aps, Qt::CaseInsensitive))
            {
                ap = 2;
                n = aps.length();
            }
        }
    }
    if (!ap)
    {
        if (part.startsWith(QLatin1String("am"), Qt::CaseInsensitive))
            ap = 1;
        else if (part.startsWith(QLatin1String("pm"), Qt::CaseInsensitive))
            ap = 2;
    }
    if (ap)
        offset += n;
    return ap;
}

/* Convert part of 'string' to a number.
 * If converted number differs from any current value in 'result', the function fails.
 * Reply = true if successful.
 */
bool getNumber(const QString& string, int& offset, int mindigits, int maxdigits, int minval, int maxval, int& result)
{
    int end = string.size();
    bool neg = false;
    if (minval == NO_NUMBER  &&  offset < end  &&  string[offset] == QLatin1Char('-'))
    {
        neg = true;
        ++offset;
    }
    if (offset + maxdigits > end)
        maxdigits = end - offset;
    int ndigits;
    for (ndigits = 0;  ndigits < maxdigits && string[offset + ndigits].isDigit();  ++ndigits) ;
    if (ndigits < mindigits)
        return false;
    bool ok;
    int n = string.mid(offset, ndigits).toInt(&ok);
    if (neg)
        n = -n;
    if (!ok  ||  result != NO_NUMBER && n != result  ||  minval != NO_NUMBER && n < minval  ||  (n > maxval && maxval >= 0))
        return false;
    result = n;
    offset += ndigits;
    return true;
}

int findString_internal(const QString &string, const char *array, int count, int &offset, int disp)
{
    for (int i = 0;  i < count;  ++i)
    {
        if (string.startsWith(array + i * disp, Qt::CaseInsensitive))
        {
            offset += qstrlen(array + i * disp);
            return i;
        }
    }
    return -1;
}

/*
 * Return the QDate for a given year, month and day.
 * If in error, check whether the reason is that the year is out of range.
 * If so, return a valid (but wrong) date but with 'status' set to the
 * appropriate error code. If no error, 'status' is set to stValid.
 */
QDate checkDate(int year, int month, int day, Status &status)
{
    status = stValid;
    QDate qdate(year, month, day);
    if (qdate.isValid())
        return qdate;

    // Invalid date - check whether it's simply out of range
    if (year < MIN_YEAR)
    {
        bool leap = (year % 4 == 0) && (year % 100 || year % 400 == 0);
        qdate.setYMD((leap ? 2000 : 2001), month, day);
        if (qdate.isValid())
            status = stTooEarly;
    }
    return qdate;
}

