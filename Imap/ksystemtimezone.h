/*
   This file is part of the KDE libraries
   Copyright (c) 2005-2007 David Jarvie <djarvie@kde.org>

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

/** @file
 * System time zone functions
 * @author David Jarvie <djarvie@kde.org>.
 * @author S.R.Haque <srhaque@iee.org>.
 */

#ifndef _KSYSTEMTIMEZONE_H
#define _KSYSTEMTIMEZONE_H

#include <kdecore_export.h>
#include "ktimezone.h"

#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QByteArray>

class KSystemTimeZoneSource;
class KSystemTimeZonePrivate;
class KSystemTimeZonesPrivate;
class KSystemTimeZoneSourcePrivate;
class KSystemTimeZoneDataPrivate;

/**
 * The KSystemTimeZones class represents the system time zone database, consisting
 * of a collection of individual system time zone definitions, indexed by name.
 * Each individual time zone is defined in a KSystemTimeZone instance. Additional
 * time zones (of any class derived from KTimeZone) may be added if desired.
 *
 * At initialisation, KSystemTimeZones reads the zone.tab file to obtain the list
 * of system time zones, and creates a KSystemTimeZone instance for each one.
 *
 * Note that KSystemTimeZones is not derived from KTimeZones, but instead contains
 * a KTimeZones instance which holds the system time zone database. Convenience
 * static methods are defined to access its data, or alternatively you can access
 * the KTimeZones instance directly via the timeZones() method.
 *
 * As an example, find the local time in Oman corresponding to the local system
 * time of 12:15:00 on 13th November 1999:
 * \code
 * QDateTime sampleTime(QDate(1999,11,13), QTime(12,15,0), Qt::LocalTime);
 * KTimeZone local = KSystemTimeZones::local();
 * KTimeZone oman  = KSystemTimeZones::zone("Asia/Muscat");
 * QDateTime omaniTime = local.convert(oman, sampleTime);
 * \endcode
 *
 * @warning The time zones in the KSystemTimeZones collection are by default
 * instances of the KSystemTimeZone class, which uses the standard system
 * libraries to access time zone data, and whose functionality is limited to
 * what these libraries provide. For guaranteed accuracy for past time change
 * dates and time zone abbreviations, you should use KSystemTimeZones::readZone()
 * or the KTzfileTimeZone class instead, which provide accurate information from
 * the time zone definition files (but are likely to incur more overhead).
 *
 * @short System time zone access
 * @see KTimeZones, KSystemTimeZone, KSystemTimeZoneSource, KTzfileTimeZone
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 * @author S.R.Haque <srhaque@iee.org>.
 */
class KDECORE_EXPORT KSystemTimeZones : public QObject
{
    Q_OBJECT
public:
    ~KSystemTimeZones();

    /**
     * Returns the unique KTimeZones instance containing the system time zones
     * collection. It is first created if it does not already exist.
     *
     * @return time zones.
     */
    static  KTimeZones *timeZones();

    /**
     * Returns all the time zones defined in this collection.
     *
     * @return time zone collection
     */
    static const KTimeZones::ZoneMap zones();

    /**
     * Returns the time zone with the given name.
     *
     * The time zone definition is obtained using system library calls, and may
     * not contain historical data. If you need historical time change data,
     * use the potentially slower method readZone().
     *
     * @param name name of time zone
     * @return time zone (usually a KSystemTimeZone instance), or invalid if not found
     * @see readZone()
     */
    static KTimeZone zone(const QString &name);

    /**
     * Returns the time zone with the given name, containing the full time zone
     * definition read directly from the system time zone database. This may
     * incur a higher overhead than zone(), but will provide whatever historical
     * data the system holds.
     *
     * @param name name of time zone
     * @return time zone (usually a KTzfileTimeZone instance), or invalid if not found
     * @see zone()
     */
    static KTimeZone readZone(const QString &name);

    /**
     * Returns the current local system time zone.
     *
     * The idea of this routine is to provide a robust lookup of the local time
     * zone. The problem is that on Unix systems, there are a variety of mechanisms
     * for setting this information, and no well defined way of getting it. For
     * example, if you set your time zone to "Europe/London", then the tzname[]
     * maintained by tzset() typically returns { "GMT", "BST" }. The point of
     * this routine is to actually return "Europe/London" (or rather, the
     * corresponding KTimeZone).
     *
     * Note that depending on how the system stores its current time zone, this
     * routine may return a synonym of the expected time zone. For example,
     * "Europe/London", "Europe/Guernsey" and some other time zones are all
     * identical and there may be no way for the routine to distinguish which
     * of these is the correct zone name from the user's point of view.
     *
     * @return local system time zone. If necessary, we will use a series of
     *         heuristics which end by returning UTC. We will never return NULL.
     *         Note that if UTC is returned as a default, it may not belong to the
     *         the collection returned by KSystemTimeZones::zones().
     */
    static KTimeZone local();

    /**
     * Returns the location of the system time zone zoneinfo database.
     *
     * @return path of directory containing the zoneinfo database
     */
    static QString zoneinfoDir();

private Q_SLOTS:
    // Connected to D-Bus signals
    void configChanged();
    void zonetabChanged(const QString &zonetab);
    void zoneDefinitionChanged(const QString &zone);

private:
    KSystemTimeZones();

    KSystemTimeZonesPrivate * const d;
    friend class KSystemTimeZonesPrivate;
};

/**
 * The KSystemTimeZone class represents a time zone in the system database.
 *
 * It works in partnership with the KSystemTimeZoneSource class which reads and parses the
 * time zone definition files.
 *
 * Typically, instances are created and accessed via the KSystemTimeZones class.
 *
 * @warning The KSystemTimeZone class uses the standard system libraries to
 * access time zone data, and its functionality is limited to what these libraries
 * provide. On many systems, dates earlier than 1970 are not handled, and on
 * non-GNU systems there is no guarantee that the time zone abbreviation returned
 * for a given date will be correct if the abbreviations applicable then were
 * not those currently in use. Consider using KSystemTimeZones::readZone() or the
 * KTzfileTimeZone class instead, which provide accurate information from the time
 * zone definition files (but are likely to incur more overhead).
 *
 * @short System time zone
 * @see KSystemTimeZones, KSystemTimeZoneSource, KSystemTimeZoneData, KTzfileTimeZone
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 */
class KDECORE_EXPORT KSystemTimeZone : public KTimeZone  //krazy:exclude=dpointer (no d-pointer for KTimeZone derived classes)
{
public:

    /**
     * Creates a time zone.
     *
     * @param source      tzfile reader and parser
     * @param name        time zone's unique name
     * @param countryCode ISO 3166 2-character country code, empty if unknown
     * @param latitude    in degrees (between -90 and +90), UNKNOWN if not known
     * @param longitude   in degrees (between -180 and +180), UNKNOWN if not known
     * @param comment     description of the time zone, if any
     */
    KSystemTimeZone(KSystemTimeZoneSource *source, const QString &name,
        const QString &countryCode = QString(), float latitude = UNKNOWN, float longitude = UNKNOWN,
        const QString &comment = QString());

    ~KSystemTimeZone();

private:
    // d-pointer is in KSystemTimeZoneBackend.
    // This is a requirement for classes inherited from KTimeZone.
};


/**
 * Backend class for KSystemTimeZone class.
 *
 * This class implements KSystemTimeZone's constructors and virtual methods. A
 * backend class is required for all classes inherited from KTimeZone to
 * allow KTimeZone virtual methods to work together with reference counting of
 * private data.
 *
 * @short Backend class for KSystemTimeZone class
 * @see KTimeZoneBackend, KSystemTimeZone, KTimeZone
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 */
class KDECORE_EXPORT KSystemTimeZoneBackend : public KTimeZoneBackend  //krazy:exclude=dpointer (non-const d-pointer for KTimeZoneBackend-derived classes)
{
public:
    /** Implements KSystemTimeZone::KSystemTimeZone(). */
    KSystemTimeZoneBackend(KSystemTimeZoneSource *source, const QString &name,
        const QString &countryCode, float latitude, float longitude, const QString &comment);

    ~KSystemTimeZoneBackend();

    /**
     * Creates a copy of this instance.
     *
     * @return new copy
     */
    virtual KTimeZoneBackend *clone() const;

    /**
     * Returns the class name of the data represented by this instance.
     *
     * @return "KSystemTimeZone"
     */
    virtual QByteArray type() const;

    /**
     * Implements KSystemTimeZone::offsetAtZoneTime().
     *
     * Returns the offset of this time zone to UTC at the given local date/time.
     * Because of daylight savings time shifts, the date/time may occur twice. Optionally,
     * the offsets at both occurrences of @p dateTime are calculated.
     *
     * The offset is the number of seconds which you must add to UTC to get
     * local time in this time zone.
     *
     * @param caller calling KSystemTimeZone object
     * @param zoneDateTime the date/time at which the offset is to be calculated. This
     *                     is interpreted as a local time in this time zone. An error
     *                     occurs if @p zoneDateTime.timeSpec() is not Qt::LocalTime.
     * @param secondOffset if non-null, and the @p zoneDateTime occurs twice, receives the
     *                     UTC offset for the second occurrence. Otherwise, it is set
     *                     the same as the return value.
     * @return offset in seconds. If @p zoneDateTime occurs twice, it is the offset at the
     *         first occurrence which is returned.
     */
    virtual int offsetAtZoneTime(const KTimeZone *caller, const QDateTime &zoneDateTime, int *secondOffset) const;

    /**
     * Implements KSystemTimeZone::offsetAtUtc().
     *
     * Returns the offset of this time zone to UTC at the given UTC date/time.
     *
     * The offset is the number of seconds which you must add to UTC to get
     * local time in this time zone.
     *
     * Note that system times are represented using time_t. An error occurs if the date
     * falls outside the range supported by time_t.
     *
     * @param caller calling KSystemTimeZone object
     * @param utcDateTime the UTC date/time at which the offset is to be calculated.
     *                    An error occurs if @p utcDateTime.timeSpec() is not Qt::UTC.
     * @return offset in seconds, or 0 if error
     */
    virtual int offsetAtUtc(const KTimeZone *caller, const QDateTime &utcDateTime) const;

    /**
     * Implements KSystemTimeZone::offset().
     *
     * Returns the offset of this time zone to UTC at a specified UTC time.
     *
     * The offset is the number of seconds which you must add to UTC to get
     * local time in this time zone.
     *
     * @param caller calling KSystemTimeZone object
     * @param t the UTC time at which the offset is to be calculated, measured in seconds
     *          since 00:00:00 UTC 1st January 1970 (as returned by time(2))
     * @return offset in seconds, or 0 if error
     */
    virtual int offset(const KTimeZone *caller, time_t t) const;

    /**
     * Implements KSystemTimeZone::isDstAtUtc().
     *
     * Returns whether daylight savings time is in operation at the given UTC date/time.
     *
     * Note that system times are represented using time_t. An error occurs if the date
     * falls outside the range supported by time_t.
     *
     * @param caller calling KSystemTimeZone object
     * @param utcDateTime the UTC date/time. An error occurs if
     *                    @p utcDateTime.timeSpec() is not Qt::UTC.
     * @return @c true if daylight savings time is in operation, @c false otherwise
     */
    virtual bool isDstAtUtc(const KTimeZone *caller, const QDateTime &utcDateTime) const;

    /**
     * Implements KSystemTimeZone::isDst().
     *
     * Returns whether daylight savings time is in operation at a specified UTC time.
     *
     * @param caller calling KSystemTimeZone object
     * @param t the UTC time, measured in seconds since 00:00:00 UTC 1st January 1970
     *          (as returned by time(2))
     * @return @c true if daylight savings time is in operation, @c false otherwise
     */
    virtual bool isDst(const KTimeZone *caller, time_t t) const;

private:
    KSystemTimeZonePrivate *d;   // non-const
};


/**
 * A class to read and parse system time zone data.
 *
 * Access is performed via the system time zone library functions.
 *
 * @short Reads and parses system time zone data
 * @see KSystemTimeZones, KSystemTimeZone, KSystemTimeZoneData
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 */
class KDECORE_EXPORT KSystemTimeZoneSource : public KTimeZoneSource
{
public:
    /**
     * Constructs a system time zone source.
     */
    KSystemTimeZoneSource();
    virtual ~KSystemTimeZoneSource();

    /**
     * Extract detailed information for one time zone, via the system time zone
     * library functions.
     *
     * @param zone the time zone for which data is to be extracted
     * @return a KSystemTimeZoneData instance containing the parsed data.
     *         The caller is responsible for deleting the KTimeZoneData instance.
     *         Null is returned on error.
     */
    virtual KTimeZoneData *parse(const KTimeZone &zone) const;

    /**
     * Use in conjunction with endParseBlock() to improve efficiency when calling parse()
     * for a group of KSystemTimeZone instances in succession.
     * Call startParseBlock() before the first parse(), and call endParseBlock() after the last.
     *
     * The effect of calling these methods is to save and restore the TZ environment variable
     * only once before and after the group of parse() calls, rather than before and
     * after every call. So, between calls to startParseBlock() and endParseBlock(), do not
     * call any functions which rely directly or indirectly on the local time zone setting.
     */
    static void startParseBlock();

    /**
     * @see startParseBlock()
     */
    static void endParseBlock();

private:
    KSystemTimeZoneSourcePrivate * const d;
};


/**
 * The parsed system time zone data returned by KSystemTimeZoneSource.
 *
 * @short Parsed system time zone data
 * @see KSystemTimeZoneSource, KSystemTimeZone
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 */
class KSystemTimeZoneData : public KTimeZoneData
{
    friend class KSystemTimeZoneSource;

public:
    KSystemTimeZoneData();
    /** Copy constructor; no special ownership assumed. */
    KSystemTimeZoneData(const KSystemTimeZoneData &);
    virtual ~KSystemTimeZoneData();

    /** Assignment; no special ownership assumed. Everything is value based. */
    KSystemTimeZoneData &operator=(const KSystemTimeZoneData &);

    /**
     * Creates a new copy of this object.
     * The caller is responsible for deleting the copy.
     * Derived classes must reimplement this method to return a copy of the
     * calling instance
     *
     * @return copy of this instance. This is a KSystemTimeZoneData pointer.
     */
    virtual KTimeZoneData *clone() const;

    /**
     * Returns the complete list of time zone abbreviations.
     *
     * @return the list of abbreviations
     */
    virtual QList<QByteArray> abbreviations() const;
    virtual QByteArray abbreviation(const QDateTime &utcDateTime) const;

    /**
     * Returns the complete list of UTC offsets for the time zone. For system
     * time zones, significant processing would be required to obtain such a
     * list, so instead an empty list is returned.
     *
     * @return empty list
     */
    virtual QList<int> utcOffsets() const;

private:
    KSystemTimeZoneDataPrivate * const d;
};

#endif
