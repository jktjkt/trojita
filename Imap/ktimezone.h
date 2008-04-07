/*
   This file is part of the KDE libraries
   Copyright (c) 2005-2007 David Jarvie <djarvie@kde.org>
   Copyright (c) 2005 S.R.Haque <srhaque@iee.org>.

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
 * Time zone functions
 * @author David Jarvie <djarvie@kde.org>.
 * @author S.R.Haque <srhaque@iee.org>.
 */

#ifndef _KTIMEZONES_H
#define _KTIMEZONES_H

#include <kdecore_export.h>

#include <sys/time.h>
#include <ctime>

#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QSharedDataPointer>

class KTimeZone;
class KTimeZoneBackend;
class KTimeZoneData;
class KTimeZoneSource;
class KTimeZonesPrivate;
class KTimeZonePrivate;
class KTimeZoneSourcePrivate;
class KTimeZoneDataPrivate;
class KTimeZoneTransitionPrivate;
class KTimeZoneLeapSecondsPrivate;

/** @defgroup timezones Time zone classes
 *
 * The time zone classes provide a framework for accessing time zone data, and
 * converting times and dates between different time zones. They provide access
 * to the system time zone database, and also allow developers to derive classes
 * to access custom sources of time zone information such as calendar files.
 *
 * A time zone is represented by the KTimeZone class. This provides access to
 * the time zone's detailed definition and contains methods to convert times to
 * and from that zone. In order to save processing, KTimeZone obtains its time
 * zone details only when they are actually required. Each KTimeZone class has
 * a corresponding KTimeZoneBackend backend class which implements reference
 * counting of the time zone's data.
 *
 * A collection of time zones is represented by the KTimeZones class, which acts
 * as a container of KTimeZone objects. Within any KTimeZones object, each
 * KTimeZone instance is uniquely identified by its name. Typically, each
 * individual source of time zone information would be represented by a different
 * KTimeZones object. This scheme allows conflicting time zone definitions
 * between the different sources to be handled, since KTimeZone names need only
 * be unique within a single KTimeZones object. Note that KTimeZone instances do
 * not have to belong to any KTimeZones container.
 *
 * Time zone source data can come in all sorts of different forms: TZFILE format
 * for a UNIX system time zone database, definitions within calendar files, calls
 * to libraries (e.g. libc), etc. The KTimeZoneSource class provides reading and
 * parsing functions to access such data, handing off the parsed data for a
 * specific time zone in a KTimeZoneData object. Both of these are base classes
 * from which should be derived other classes which know about the particular
 * access method and data format (KTimeZoneSource) and which details are actually
 * provided (KTimeZoneData). When a KTimeZone instance needs its time zone's
 * definition, it calls KTimeZoneSource::parse() and receives the data back in a
 * KTimeZoneData object which it keeps for reference.
 *
 * KTimeZoneData holds the definitions of the different daylight saving time and
 * standard time phases in KTimeZone::Phase objects, and the timed sequence of
 * daylight saving time changes in KTimeZone::Transition objects. Leap seconds
 * adjustments are held in KTimeZone::LeapSeconds objects. You can access this
 * data directly via KTimeZone and KTimeZoneData methods if required.
 *
 * The mapping of the different classes to external data is as follows:
 *
 * - Each different source data format or access method is represented by a
 *   different KTimeZoneSource class.
 *
 * - Each different set of data provided from source data is represented by a
 *   different KTimeZoneData class. For example, some time zone sources provide
 *   only the absolute basic information about time zones, i.e. name, transition
 *   times and offsets from UTC. Others provide information on leap second
 *   adjustments, while still others might contain information on which countries
 *   use the time zone. To allow for this variation, KTimeZoneData is made
 *   available for inheritance. When the necessary information is not available,
 *   the KTimeZone::Phase, KTimeZone::Transition and KTimeZone::LeapSeconds data
 *   will be empty.
 *
 * - Each KTimeZoneData class will have a corresponding KTimeZone class, and
 *   related KTimeZoneBackend class, which can interpret its data.
 *
 * - Each different source database will typically be represented by a different
 *   KTimeZones instance, to avoid possible conflicts between time zone definitions.
 *   If it is known that two source databases are definitely compatible, they can
 *   be grouped together into the same KTimeZones instance.
 *
 *
 * \section sys System time zones
 *
 * Access to system time zones is provided by the KSystemTimeZones class, which
 * reads the zone.tab file to obtain the list of system time zones, and creates a
 * KSystemTimeZone instance for each one. KSystemTimeZone has a
 * KSystemTimeZoneBackend backend class, and uses the KSystemTimeZoneSource
 * and KSystemTimeZoneData classes to obtain time zone data via libc library
 * functions.
 *
 * Normally, KSystemTimeZoneSource and KSystemTimeZoneData operate in the
 * background and you will not need to use them directly.
 *
 * @warning The KSystemTimeZone class uses the standard system libraries to
 * access time zone data, and its functionality is limited to what these libraries
 * provide. On many systems, dates earlier than 1902 are not handled, and on
 * non-GNU systems there is no guarantee that the time zone abbreviation returned
 * for a given date will be correct if the abbreviations applicable then were
 * not those currently in use. The KSystemTimeZones::readZone() method overcomes
 * these restrictions by reading the time zone definition directly from the
 * system time zone database files.
 *
 * \section tzfile Tzfile access
 *
 * The KTzfileTimeZone class provides access to tzfile(5) time zone definition
 * files, which are used to form the time zone database on UNIX systems. Usually,
 * for current information, it is easier to use the KSystemTimeZones class to
 * access system tzfile data. However, for dealing with past data the
 * KTzfileTimeZone class provides better guarantees of accurary, although it
 * cannot handle dates earlier than 1902. It also provides more detailed
 * information, and allows you to read non-system tzfile files. Alternatively,
 * the KSystemTimeZones::readZone() method uses the KTzfileTimeZone class to
 * read system time zone definition files.
 *
 * KTzfileTimeZone has a KTzfileTimeZoneBackend backend class, and uses the
 * KTzfileTimeZoneSource and KTzfileTimeZoneData classes to obtain time zone
 * data from tzfile files.
 *
 *
 * \section deriving Handling time zone data from other sources
 *
 * To implement time zone classes to access a new time zone data source, you need
 * as a minimum to derive a new class from KTimeZoneSource, and implement one or
 * more parse() methods. If you can know in advance what KTimeZone instances to create
 * without having to parse the source data, you should reimplement the virtual method
 * KTimeZoneSource::parse(const KTimeZone&). Otherwise, you need to define your
 * own parse() methods with appropriate signatures, to both read and parse the new
 * data, and create new KTimeZone instances.
 *
 * If the data for each time zone which is available from the new source happens
 * to be the same as for another source for which KTimeZone classes already exist,
 * you could simply use the existing KTimeZone, KTimeZoneBackend and KTimeZoneData
 * derived classes to receive the parsed data from your new KTimeZoneSource class:
 *
 * \code
 * class NewTimeZoneSource : public KTimeZoneSource
 * {
 *     public:
 *         NewTimeZoneSource(...);  // parameters might include location of data source ...
 *         ~NewTimeZoneSource();
 *
 *         // Option 1: reimplement KTimeZoneSource::parse() if you can
 *         // pre-create the KTimeZone instances.
 *         KTimeZoneData *parse(const KTimeZone &zone) const;
 *
 *         // Option 2: implement new parse() methods if you don't know
 *         // in advance what KTimeZone instances to create.
 *         void parse(..., KTimeZones *zones) const;
 *         NewTimeZone *parse(...) const;
 * };
 *
 * // Option 1:
 * KTimeZoneData *NewTimeZoneSource::parse(const KTimeZone &zone) const
 * {
 *     QString zoneName = zone.name();
 *     ExistingTimeZoneData* data = new ExistingTimeZoneData();
 *
 *     // Read the data for 'zoneName' from the new data source.
 *
 *     // Parse what we have read, and write it into 'data'.
 *     // Compile the sequence of daylight savings changes and leap
 *     // seconds adjustments (if available) and write into 'data'.
 *
 *     return data;
 * }
 * \endcode
 *
 * If the data from the new source is different from what any existing
 * KTimeZoneData class contains, you will need to implement new KTimeZone,
 * KTimeZoneBackend and KTimeZoneData classes in addition to the KTimeZoneSource
 * class illustrated above:
 *
 * \code
 * class NewTimeZone : public KTimeZone
 * {
 *     public:
 *         NewTimeZone(NewTimeZoneSource *source, const QString &name, ...);
 *         ~NewTimeZone();
 *
 *         // Methods implementing KTimeZone virtual methods are implemented
 *         // in NewTimeZoneBackend, not here.
 *
 *         // Anything else which you need
 *     private:
 *         // No d-pointer !
 * };
 *
 * class NewTimeZoneBackend : public KTimeZoneBackend
 * {
 *     public:
 *         NewTimeZoneBackend(NewTimeZoneSource *source, const QString &name, ...);
 *         ~NewTimeZoneBackend();
 *
 *         KTimeZoneBackend *clone() const;
 *
 *         // Virtual methods which need to be reimplemented
 *         int offsetAtZoneTime(const KTimeZone *caller, const QDateTime &zoneDateTime, int *secondOffset = 0) const;
 *         int offsetAtUtc(const KTimeZone *caller, const QDateTime &utcDateTime) const;
 *         int offset(const KTimeZone *caller, time_t t) const;
 *         int isDstAtUtc(const KTimeZone *caller, const QDateTime &utcDateTime) const;
 *         bool isDst(const KTimeZone *caller, time_t t) const;
 *
 *         // Anything else which you need
 *     private:
 *         NewTimeZonePrivate *d;   // non-const !
 * };
 *
 * class NewTimeZoneData : public KTimeZoneData
 * {
 *         friend class NewTimeZoneSource;
 *
 *     public:
 *         NewTimeZoneData();
 *         ~NewTimeZoneData();
 *
 *         // Virtual methods which need to be reimplemented
 *         KTimeZoneData *clone() const;
 *         QList<QByteArray> abbreviations() const;
 *         QByteArray abbreviation(const QDateTime &utcDateTime) const;
 *
 *         // Data members containing whatever is read by NewTimeZoneSource
 * };
 * \endcode
 *
 * Here is a guide to implementing the offset() and offsetAtUtc() methods, in
 * the case where the source data does not use time_t for its time measurement:
 *
 * \code
 * int NewTimeZoneBackend::offsetAtUtc(const KTimeZone *caller, const QDateTime &utcDateTime) const
 * {
 *     // Access this time zone's data. If we haven't already read it,
 *     // force a read from source now.
 *     NewTimeZoneData *zdata = caller->data(true);
 *
 *     // Use 'zdata' contents to work out the UTC offset
 *
 *     return offset;
 * }
 *
 * int NewTimeZoneBackend::offset(const KTimeZone *caller, time_t t) const
 * {
 *     return offsetAtUtc(KTimeZone::fromTime_t(t));
 * }
 * \endcode
 *
 * The other NewTimeZoneBackend methods would work in an analogous way to
 * NewTimeZoneBackend::offsetAtUtc() and NewTimeZoneBackend::offset().
 */

/**
 * The KTimeZones class represents a time zone database which consists of a
 * collection of individual time zone definitions.
 *
 * Each individual time zone is defined in a KTimeZone instance, which provides
 * generic support for private or system time zones. The time zones in the
 * collection are indexed by name, which must be unique within the collection.
 *
 * Different time zone sources could define the same time zone differently. (For
 * example, a calendar file originating from another system might hold its own
 * time zone definitions, which may not necessarily be identical to your own
 * system's definitions.) In order to keep conflicting definitions separate,
 * it will often be necessary when dealing with multiple time zone sources to
 * create a separate KTimeZones instance for each source collection.
 *
 * If you want to access system time zones, use the KSystemTimeZones class.
 *
 * @short Represents a time zone database or collection
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 * @author S.R.Haque <srhaque@iee.org>.
 */
class KDECORE_EXPORT KTimeZones
{
public:
    KTimeZones();
    ~KTimeZones();

    /**
     * Returns the time zone with the given name.
     *
     * @param name name of time zone
     * @return time zone, or 0 if not found
     */
    KTimeZone zone(const QString &name) const;

    typedef QMap<QString, KTimeZone> ZoneMap;

    /**
     * Returns all the time zones defined in this collection.
     *
     * @return time zone collection
     */
    const ZoneMap zones() const;

    /**
     * Adds a time zone to the collection.
     * The time zone's name must be unique within the collection.
     *
     * @param zone time zone to add
     * @return @c true if successful, @c false if zone's name duplicates one already in the collection
     * @see remove()
     */
    bool add(const KTimeZone &zone);

    /**
     * Removes a time zone from the collection.
     *
     * @param zone time zone to remove
     * @return the time zone which was removed, or invalid if not found
     * @see clear(), add()
     */
    KTimeZone remove(const KTimeZone &zone);

    /**
     * Removes a time zone from the collection.
     *
     * @param name name of time zone to remove
     * @return the time zone which was removed, or invalid if not found
     * @see clear(), add()
     */
    KTimeZone remove(const QString &name);

    /**
     * Clears the collection.
     *
     * @see remove()
     */
    void clear();

private:
    KTimeZones(const KTimeZones &);              // prohibit copying
    KTimeZones &operator=(const KTimeZones &);   // prohibit copying

    KTimeZonesPrivate * const d;
};


/**
 * Base class representing a time zone.
 *
 * The KTimeZone base class contains general descriptive data about the time zone, and
 * provides an interface for methods to read and parse time zone definitions, and to
 * translate between UTC and local time. Derived classes must implement these methods,
 * and may also hold the actual details of the dates and times of daylight savings
 * changes, offsets from UTC, etc. They should be tailored to deal with the type and
 * format of data held by a particular type of time zone database.
 *
 * If this base class is instantiated as a valid instance, it always represents the
 * UTC time zone.
 *
 * KTimeZone is designed to work in partnership with KTimeZoneSource. KTimeZone
 * provides access to individual time zones, while classes derived from
 * KTimeZoneSource read and parse a particular format of time zone definition.
 * Because time zone sources can differ in what information they provide about time zones,
 * the parsed data retured by KTimeZoneSource can vary between different sources,
 * resulting in the need to create different KTimeZone classes to handle the data.
 *
 * KTimeZone instances are often grouped into KTimeZones collections.
 *
 * Copying KTimeZone instances is very efficient since the class data is explicitly
 * shared, meaning that only a pointer to the data is actually copied. To achieve
 * this, each class inherited from KTimeZone must have a corresponding backend
 * class derived from KTimeZoneBackend.
 *
 * @note Classes derived from KTimeZone should not have their own d-pointer. The
 * d-pointer is instead contained in their backend class (derived from
 * KTimeZoneBackend). This allows KTimeZone's reference-counting of private data to
 * take care of the derived class's data as well, ensuring that instance data is
 * not deleted while any references to the class instance remains. All virtual
 * methods which override KTimeZone virtual methods must be defined in the
 * backend class instead.
 *
 * @short Base class representing a time zone
 * @see KTimeZoneBackend, KTimeZoneSource, KTimeZoneData
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 * @author S.R.Haque <srhaque@iee.org>.
 */
class KDECORE_EXPORT KTimeZone  //krazy:exclude=dpointer (has non-const d-pointer to Backend class)
{
public:

    /*
     * Time zone phase.
     *
     * A phase can be daylight savings time or standard time. It holds the
     * UTC offset and time zone abbreviation (e.g. EST, GMT).
     *
     * @short Time zone phase
     * @author David Jarvie <djarvie@kde.org>.
     */
    class KDECORE_EXPORT Phase
    {
    public:
        Phase();

        /**
         * Constructor.
         *
         * @param utcOffset number of seconds to add to UTC to get local time in this phase
         * @param abbreviations time zone abbreviation for this phase. If translations exist,
         *                      concatenate all abbreviations as null-terminated strings.
         * @param dst true if daylight savings time, false if standard time
         * @param comment optional comment
         */
        Phase(int utcOffset, const QByteArray &abbreviations, bool dst,
              const QString &comment = QString());

        /**
         * Constructor.
         *
         * @param utcOffset number of seconds to add to UTC to get local time in this phase
         * @param abbreviations time zone abbreviation for this phase, plus any translations
         * @param dst true if daylight savings time, false if standard time
         * @param comment optional comment
         */
        Phase(int utcOffset, const QList<QByteArray> &abbreviations, bool dst,
              const QString &comment = QString());

        Phase(const Phase &rhs);
        ~Phase();
        Phase &operator=(const Phase &rhs);
        bool operator==(const Phase &rhs) const;
        bool operator!=(const Phase &rhs) const  { return !operator==(rhs); }

        /**
         * Return the UTC offset in seconds during this phase.
         * The UTC offset is the number of seconds which you must add to UTC
         * to get local time.
         *
         * @return offset in seconds to add to UTC
         */
        int utcOffset() const;

        /**
         * Return the time zone abbreviations which apply to this phase.
         *
         * More than one abbreviation may be returned, to allow for possible translations.
         *
         * @return time zone abbreviations
         */
        QList<QByteArray> abbreviations() const;

        /**
         * Return whether daylight savings time applies during this phase.
         *
         * @return true if daylight savings are in operation, false otherwise
         */
        bool isDst() const;

        /**
         * Return the comment (if any) applying to this phase.
         *
         * @return comment
         */
        QString comment() const;

    private:
        QSharedDataPointer<class KTimeZonePhasePrivate> d;
    };


    /*
     * Time zone daylight saving time transition.
     *
     * A Transition instance holds details of a transition to daylight saving time or
     * standard time, including the UTC time of the change.
     *
     * @short Time zone transition
     * @author David Jarvie <djarvie@kde.org>.
     */
    class KDECORE_EXPORT Transition
    {
    public:
        Transition();
        Transition(const QDateTime &dt, const Phase &phase);
        Transition(const KTimeZone::Transition &t);
        ~Transition();
        Transition &operator=(const KTimeZone::Transition &t);

        /**
         * Return the UTC time of the transition.
         *
         * @return UTC time
         */
        QDateTime time() const;

        /**
         * Return the time zone phase which takes effect after the transition.
         *
         * @return time zone phase
         */
        Phase phase() const;

        /**
         * Compare the date/time values of two transitions.
         *
         * @param rhs other instance
         * @return @c true if this Transition is earlier than @p rhs
         */
        bool operator<(const Transition &rhs) const;

    private:
        KTimeZoneTransitionPrivate *const d;
    };


    /*
     * Leap seconds adjustment for a time zone.
     *
     * This class defines a leap seconds adjustment for a time zone by its UTC time of
     * occurrence and the cumulative number of leap seconds to be added at that time.
     *
     * @short Leap seconds adjustment for a time zone
     * @see KTimeZone, KTimeZoneData
     * @ingroup timezones
     * @author David Jarvie <djarvie@kde.org>.
     */
    class KDECORE_EXPORT LeapSeconds
    {
    public:
        LeapSeconds();
        LeapSeconds(const QDateTime &utcTime, int leapSeconds, const QString &comment = QString());
        LeapSeconds(const LeapSeconds &c);
        ~LeapSeconds();
        LeapSeconds &operator=(const LeapSeconds &c);
        bool operator<(const LeapSeconds &c) const;    // needed by qSort()

        /**
         * Return whether this instance holds valid data.
         *
         * @return true if valid, false if invalid
         */
        bool isValid() const;

        /**
         * Return the UTC date/time when this change occurred.
         *
         * @return date/time
         */
        QDateTime dateTime() const;

        /**
         * Return the cumulative number of leap seconds to be added after this
         * change occurs.
         *
         * @return number of leap seconds
         */
        int leapSeconds() const;

        /**
         * Return the comment (if any) applying to this change.
         *
         * @return comment
         */
        QString comment() const;

    private:
        KTimeZoneLeapSecondsPrivate *const d;
    };


    /**
     * Constructs a null time zone. A null time zone is invalid.
     *
     * @see isValid()
     */
    KTimeZone();

    /**
     * Constructs a UTC time zone.
     *
     * @param name name of the UTC time zone
     */
    explicit KTimeZone(const QString &name);

    KTimeZone(const KTimeZone &tz);
    KTimeZone &operator=(const KTimeZone &tz);

    virtual ~KTimeZone();

    /**
     * Checks whether this is the same instance as another one.
     * Note that only the pointers to the time zone data are compared, not the
     * contents. So it will only return equality if one instance was copied
     * from the other.
     *
     * @param rhs other instance
     * @return true if the same instance, else false
     */
    bool operator==(const KTimeZone &rhs) const;
    bool operator!=(const KTimeZone &rhs) const  { return !operator==(rhs); }

    /**
     * Returns the class name of the data represented by this instance.
     * If a derived class object has been assigned to this instance, this
     * method will return the name of that class.
     *
     * @return "KTimeZone" or the class name of a derived class
     */
    QByteArray type() const;

    /**
     * Checks whether the instance is valid.
     *
     * @return true if valid, false if invalid
     */
    bool isValid() const;

    /**
     * Returns the name of the time zone.
     * If it is held in a KTimeZones container, the name is the time zone's unique
     * identifier within that KTimeZones instance.
     *
     * @return name in system-dependent format
     */
    QString name() const;

    /**
     * Returns the two-letter country code of the time zone.
     *
     * @return upper case ISO 3166 2-character country code, empty if unknown
     */
    QString countryCode() const;

    /**
     * Returns the latitude of the time zone.
     *
     * @return latitude in degrees, UNKNOWN if not known
     */
    float latitude() const;

    /**
     * Returns the latitude of the time zone.
     *
     * @return latitude in degrees, UNKNOWN if not known
     */
    float longitude() const;

    /**
     * Returns any comment for the time zone.
     *
     * @return comment, may be empty
     */
    QString comment() const;

    /**
     * Returns the list of time zone abbreviations used by the time zone.
     * This may include historical ones which are no longer in use or have
     * been superseded.
     *
     * @return list of abbreviations
     * @see abbreviation()
     */
    QList<QByteArray> abbreviations() const;

    /**
     * Returns the time zone abbreviation current at a specified time.
     *
     * @param utcDateTime UTC date/time. An error occurs if
     *                    @p utcDateTime.timeSpec() is not Qt::UTC.
     * @return time zone abbreviation, or empty string if error
     * @see abbreviations()
     */
    QByteArray abbreviation(const QDateTime &utcDateTime) const;

    /**
     * Returns the complete list of UTC offsets used by the time zone. This may
     * include historical ones which are no longer in use or have been
     * superseded.
     *
     * A UTC offset is the number of seconds which you must add to UTC to get
     * local time in this time zone.
     *
     * If due to the nature of the source data for the time zone, compiling a
     * complete list would require significant processing, an empty list is
     * returned instead.
     *
     * @return sorted list of UTC offsets, or empty list if not readily available.
     */
    QList<int> utcOffsets() const;

    /**
     * Converts a date/time, which is interpreted as being local time in this
     * time zone, into local time in another time zone.
     *
     * @param newZone other time zone which the time is to be converted into
     * @param zoneDateTime local date/time. An error occurs if
     *                     @p zoneDateTime.timeSpec() is not Qt::LocalTime.
     * @return converted date/time, or invalid date/time if error
     * @see toUtc(), toZoneTime()
     */
    QDateTime convert(const KTimeZone &newZone, const QDateTime &zoneDateTime) const;

    /**
     * Converts a date/time, which is interpreted as local time in this time
     * zone, into UTC.
     *
     * Because of daylight savings time shifts, the date/time may occur twice. In
     * such cases, this method returns the UTC time for the first occurrence.
     * If you need the UTC time of the second occurrence, use offsetAtZoneTime().
     *
     * @param zoneDateTime local date/time. An error occurs if
     *                     @p zoneDateTime.timeSpec() is not Qt::LocalTime.
     * @return UTC date/time, or invalid date/time if error
     * @see toZoneTime(), convert()
     */
    QDateTime toUtc(const QDateTime &zoneDateTime) const;

    /**
     * Converts a UTC date/time into local time in this time zone.
     *
     * Because of daylight savings time shifts, some local date/time values occur
     * twice. The @p secondOccurrence parameter may be used to determine whether
     * the time returned is the first or second occurrence of that time.
     *
     * @param utcDateTime UTC date/time. An error occurs if
     *                    @p utcDateTime.timeSpec() is not Qt::UTC.
     * @param secondOccurrence if non-null, returns @p true if the return value
     *                    is the second occurrence of that time, else @p false
     * @return local date/time, or invalid date/time if error
     * @see toUtc(), convert()
     */
    QDateTime toZoneTime(const QDateTime &utcDateTime, bool *secondOccurrence = 0) const;

    /**
     * Returns the current offset of this time zone to UTC or the local
     * system time zone. The offset is the number of seconds which you must
     * add to UTC or the local system time to get local time in this time zone.
     *
     * Take care if you cache the results of this routine; that would
     * break if the result were stored across a daylight savings change.
     *
     * @param basis Qt::UTC to return the offset to UTC, Qt::LocalTime
     *                  to return the offset to local system time
     * @return offset in seconds
     * @see offsetAtZoneTime(), offsetAtUtc()
     */
    int currentOffset(Qt::TimeSpec basis = Qt::UTC) const;

    /**
     * Returns the offset of this time zone to UTC at the given local date/time.
     * Because of daylight savings time shifts, the date/time may occur twice. Optionally,
     * the offsets at both occurrences of @p dateTime are calculated.
     *
     * The offset is the number of seconds which you must add to UTC to get
     * local time in this time zone.
     *
     * @param zoneDateTime the date/time at which the offset is to be calculated. This
     *                     is interpreted as a local time in this time zone. An error
     *                     occurs if @p zoneDateTime.timeSpec() is not Qt::LocalTime.
     * @param secondOffset if non-null, and the @p zoneDateTime occurs twice, receives the
     *                     UTC offset for the second occurrence. Otherwise, it is set
     *                     the same as the return value.
     * @return offset in seconds. If @p zoneDateTime occurs twice, it is the offset at the
     *         first occurrence which is returned. If @p zoneDateTime does not exist because
     *         of daylight savings time shifts, InvalidOffset is returned. If any other error
     *         occurs, 0 is returned.
     * @see offsetAtUtc(), currentOffset()
     */
    virtual int offsetAtZoneTime(const QDateTime &zoneDateTime, int *secondOffset = 0) const;

    /**
     * Returns the offset of this time zone to UTC at the given UTC date/time.
     *
     * The offset is the number of seconds which you must add to UTC to get
     * local time in this time zone.
     *
     * If a derived class needs to work in terms of time_t (as when accessing the
     * system time functions, for example), it should override both this method and
     * offset() so as to implement its offset calculations in offset(), and
     * reimplement this method simply as
     * \code
     *     offset(toTime_t(utcDateTime));
     * \endcode
     *
     * @param utcDateTime the UTC date/time at which the offset is to be calculated.
     *                    An error occurs if @p utcDateTime.timeSpec() is not Qt::UTC.
     * @return offset in seconds, or 0 if error
     * @see offset(), offsetAtZoneTime(), currentOffset()
     */
    virtual int offsetAtUtc(const QDateTime &utcDateTime) const;

    /**
     * Returns the offset of this time zone to UTC at a specified UTC time.
     *
     * The offset is the number of seconds which you must add to UTC to get
     * local time in this time zone.
     *
     * Note that time_t has a more limited range than QDateTime, so consider using
     * offsetAtUtc() instead.
     *
     * @param t the UTC time at which the offset is to be calculated, measured in seconds
     *          since 00:00:00 UTC 1st January 1970 (as returned by time(2))
     * @return offset in seconds, or 0 if error
     * @see offsetAtUtc()
     */
    virtual int offset(time_t t) const;

    /**
     * Returns whether daylight savings time is in operation at the given UTC date/time.
     *
     * If a derived class needs to work in terms of time_t (as when accessing the
     * system time functions, for example), it should override both this method and
     * isDst() so as to implement its offset calculations in isDst(), and reimplement
     * this method simply as
     * \code
     *     isDst(toTime_t(utcDateTime));
     * \endcode
     *
     * @param utcDateTime the UTC date/time. An error occurs if
     *                    @p utcDateTime.timeSpec() is not Qt::UTC.
     * @return @c true if daylight savings time is in operation, @c false otherwise
     * @see isDst()
     */
    virtual bool isDstAtUtc(const QDateTime &utcDateTime) const;

    /**
     * Returns whether daylight savings time is in operation at a specified UTC time.
     *
     * Note that time_t has a more limited range than QDateTime, so consider using
     * isDstAtUtc() instead.
     *
     * @param t the UTC time, measured in seconds since 00:00:00 UTC 1st January 1970
     *          (as returned by time(2))
     * @return @c true if daylight savings time is in operation, @c false otherwise
     * @see isDstAtUtc()
     */
    virtual bool isDst(time_t t) const;

    /**
     * Return all daylight savings time phases for the time zone.
     *
     * Note that some time zone data sources (such as system time zones accessed
     * via the system libraries) may not allow a list of daylight savings time
     * changes to be compiled easily. In such cases, this method will return an
     * empty list.
     *
     * @return list of phases
     */
    QList<Phase> phases() const;

    /**
     * Return whether daylight saving transitions are available for the time zone.
     *
     * The base class returns @c false.
     *
     * @return @c true if transitions are available, @c false if not
     * @see transitions(), transition()
     */
    virtual bool hasTransitions() const;

    /**
     * Return all daylight saving transitions, in time order. If desired, the
     * transitions returned may be restricted to a specified time range.
     *
     * Note that some time zone data sources (such as system time zones accessed
     * via the system libraries) may not allow a list of daylight saving time
     * changes to be compiled easily. In such cases, this method will return an
     * empty list.
     *
     * @param start start UTC date/time, or invalid date/time to return all transitions
     *              up to @p end. @p start.timeSpec() must be Qt::UTC, else
     *              @p start will be considered invalid.
     * @param end end UTC date/time, or invalid date/time for no end. @p end.timeSpec()
     *                must be Qt::UTC, else @p end will be considered invalid.
     * @return list of transitions, in time order
     * @see hasTransitions(), transition(), transitionTimes()
     */
    QList<KTimeZone::Transition> transitions(const QDateTime &start = QDateTime(), const QDateTime &end = QDateTime()) const;

    /**
     * Find the last daylight savings time transition at or before a given
     * UTC or local time.
     *
     * Because of daylight savings time shifts, a local time may occur twice or
     * may not occur at all. In the former case, the transitions at or before
     * both occurrences of @p dt may optionally be calculated and returned in
     * @p secondTransition. The latter case may optionally be detected by use of
     * @p validTime.
     *
     * @param dt date/time. @p dt.timeSpec() may be set to Qt::UTC or Qt::LocalTime.
     * @param secondTransition if non-null, and the @p dt occurs twice, receives the
     *                     transition for the second occurrence. Otherwise, it is set
     *                     the same as the return value.
     * @param validTime if non-null, is set to false if @p dt does not occur, or
     *                  to true otherwise
     * @return time zone transition, or null either if @p dt is either outside the
     *         defined range of the transition data or if @p dt does not occur
     * @see transitionIndex(), hasTransitions(), transitions()
     */
    const KTimeZone::Transition *transition(const QDateTime &dt, const Transition **secondTransition = 0, bool *validTime = 0) const;

    /**
     * Find the index to the last daylight savings time transition at or before
     * a given UTC or local time. The return value is the index into the transition
     * list returned by transitions().
     *
     * Because of daylight savings time shifts, a local time may occur twice or
     * may not occur at all. In the former case, the transitions at or before
     * both occurrences of @p dt may optionally be calculated and returned in
     * @p secondIndex. The latter case may optionally be detected by use of
     * @p validTime.
     *
     * @param dt date/time. @p dt.timeSpec() may be set to Qt::UTC or Qt::LocalTime.
     * @param secondIndex if non-null, and the @p dt occurs twice, receives the
     *                    index to the transition for the second occurrence. Otherwise,
     *                    it is set the same as the return value.
     * @param validTime if non-null, is set to false if @p dt does not occur, or
     *                  to true otherwise
     * @return index into the time zone transition list, or -1 either if @p dt is
     *         either outside the defined range of the transition data or if @p dt
     *         does not occur
     * @see transition(), transitions(), hasTransitions()
     */
    int transitionIndex(const QDateTime &dt, int *secondIndex = 0, bool *validTime = 0) const;

    /**
     * Return the times of all daylight saving transitions to a given time zone
     * phase, in time order. If desired, the times returned may be restricted to
     * a specified time range.
     *
     * Note that some time zone data sources (such as system time zones accessed
     * via the system libraries) may not allow a list of daylight saving time
     * changes to be compiled easily. In such cases, this method will return an
     * empty list.
     *
     * @param phase time zone phase
     * @param start start UTC date/time, or invalid date/time to return all transitions
     *              up to @p end. @p start.timeSpec() must be Qt::UTC, else
     *              @p start will be considered invalid.
     * @param end end UTC date/time, or invalid date/time for no end. @p end.timeSpec()
     *                must be Qt::UTC, else @p end will be considered invalid.
     * @return ordered list of transition times
     * @see hasTransitions(), transition(), transitions()
     */
    QList<QDateTime> transitionTimes(const Phase &phase, const QDateTime &start = QDateTime(), const QDateTime &end = QDateTime()) const;

    /**
     * Return all leap second adjustments, in time order.
     *
     * Note that some time zone data sources (such as system time zones accessed
     * via the system libraries) may not provide information on leap second
     * adjustments. In such cases, this method will return an empty list.
     *
     * @return list of adjustments
     */
    QList<LeapSeconds> leapSecondChanges() const;

    /**
     * Returns the source reader/parser for the time zone's source database.
     *
     * @return reader/parser
     */
    KTimeZoneSource *source() const;

    /**
     * Extracts time zone detail information for this time zone from the source database.
     *
     * @return @c false if the parse encountered errors, @c true otherwise
     */
    bool parse() const;

    /**
     * Returns the detailed parsed data for the time zone.
     * This will return null unless either parse() has been called beforehand, or
     * @p create is true.
     *
     * @param create true to parse the zone's data first if not already parsed
     * @return pointer to data, or null if data has not been parsed
     */
    const KTimeZoneData *data(bool create = false) const;

    /**
     * Update the definition of the time zone to be identical to another
     * KTimeZone instance. A prerequisite is that the two instances must
     * have the same name.
     *
     * The main purpose of this method is to allow updates of the time zone
     * definition by derived classes without invalidating pointers to the
     * instance (particularly pointers held by KDateTime objects). Note
     * that the KTimeZoneData object and KTimeZoneSource pointer are not
     * updated: the caller class should do this itself by calling setData().
     *
     * @param other time zone whose definition is to be used
     * @return true if definition was updated (i.e. names are the same)
     *
     * @see setData()
     */
    bool updateBase(const KTimeZone &other);

    /**
     * Converts a UTC time, measured in seconds since 00:00:00 UTC 1st January 1970
     * (as returned by time(2)), to a UTC QDateTime value.
     * QDateTime::setTime_t() is limited to handling @p t >= 0, since its parameter
     * is unsigned. This method takes a parameter of time_t which is signed.
     *
     * @return converted time
     * @see toTime_t()
     */
    static QDateTime fromTime_t(time_t t);

    /**
     * Converts a UTC QDateTime to a UTC time, measured in seconds since 00:00:00 UTC
     * 1st January 1970 (as returned by time(2)).
     * QDateTime::toTime_t() returns an unsigned value. This method returns a time_t
     * value, which is signed.
     *
     * @param utcDateTime date/time. An error occurs if @p utcDateTime.timeSpec() is
     *                    not Qt::UTC.
     * @return converted time, or -1 if the date is out of range for time_t or
     *         @p utcDateTime.timeSpec() is not Qt::UTC
     * @see fromTime_t()
     */
    static time_t toTime_t(const QDateTime &utcDateTime);

    /**
     * Returns a standard UTC time zone, with name "UTC".
     *
     * @note The KTimeZone returned by this method does not belong to any
     * KTimeZones collection. Any KTimeZones instance may contain its own UTC
     * KTimeZone defined by its time zone source data, but that will be a
     * different instance than this KTimeZone.
     *
     * @return UTC time zone
     */
    static KTimeZone utc();

    /** Indicates an invalid UTC offset. This is returned by offsetAtZoneTime() when
     *  the local time does not occur due to a shift to daylight savings time.
     */
    static const int InvalidOffset;

    /** Indicates an invalid time_t value.
     */
    static const time_t InvalidTime_t;

    /**
     * A representation for unknown locations; this is a float
     * that does not represent a real latitude or longitude.
     */
    static const float UNKNOWN;

protected:
    KTimeZone(KTimeZoneBackend *impl);

    /**
     * Sets the detailed parsed data for the time zone, and optionally
     * a new time zone source object.
     *
     * @param data parsed data
     * @param source if non-null, the new source object for the time zone
     *
     * @see data()
     */
    void setData(KTimeZoneData *data, KTimeZoneSource *source = 0);

private:
    KTimeZoneBackend *d;
};


/**
 * Base backend class for KTimeZone classes.
 *
 * KTimeZone and each class inherited from it must have a corresponding
 * backend class to implement its constructors and its virtual methods,
 * and to provide a virtual clone() method. This allows KTimeZone virtual methods
 * to work together with reference counting of private data.
 *
 * @note Classes derived from KTimeZoneBackend should not normally implement their
 * own copy constructor or assignment operator, and must have a non-const d-pointer.
 *
 * @short Base backend class for KTimeZone classes
 * @see KTimeZone
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 */
class KDECORE_EXPORT KTimeZoneBackend  //krazy:exclude=dpointer (non-const d-pointer for KTimeZoneBackend-derived classes)
{
public:
    /** Implements KTimeZone::KTimeZone(). */
    KTimeZoneBackend();
    /** Implements KTimeZone::KTimeZone(const QString&). */
    explicit KTimeZoneBackend(const QString &name);

    KTimeZoneBackend(const KTimeZoneBackend &other);
    KTimeZoneBackend &operator=(const KTimeZoneBackend &other);
    virtual ~KTimeZoneBackend();

    /**
     * Creates a copy of this instance.
     *
     * @note Every inherited class must reimplement clone().
     *
     * @return new copy
     */
    virtual KTimeZoneBackend *clone() const;

    /**
     * Returns the class name of the data represented by this instance.
     *
     * @note Every inherited class must reimplement type().
     *
     * @return "KTimeZone" for this base class.
     */
    virtual QByteArray type() const;

    /**
     * Implements KTimeZone::offsetAtZoneTime().
     *
     * @param caller calling KTimeZone object
     */
    virtual int offsetAtZoneTime(const KTimeZone* caller, const QDateTime &zoneDateTime, int *secondOffset) const;
    /**
     * Implements KTimeZone::offsetAtUtc().
     *
     * @param caller calling KTimeZone object
     */
    virtual int offsetAtUtc(const KTimeZone* caller, const QDateTime &utcDateTime) const;
    /**
     * Implements KTimeZone::offset().
     *
     * @param caller calling KTimeZone object
     */
    virtual int offset(const KTimeZone* caller, time_t t) const;
    /**
     * Implements KTimeZone::isDstAtUtc().
     *
     * @param caller calling KTimeZone object
     */
    virtual bool isDstAtUtc(const KTimeZone* caller, const QDateTime &utcDateTime) const;
    /**
     * Implements KTimeZone::isDst().
     *
     * @param caller calling KTimeZone object
     */
    virtual bool isDst(const KTimeZone* caller, time_t t) const;
    /**
     * Implements KTimeZone::hasTransitions().
     *
     * @param caller calling KTimeZone object
     */
    virtual bool hasTransitions(const KTimeZone* caller) const;

protected:
    /**
     * Constructs a time zone.
     *
     * @param source      reader/parser for the database containing this time zone. This will
     *                    be an instance of a class derived from KTimeZoneSource.
     * @param name        in system-dependent format. The name must be unique within any
     *                    KTimeZones instance which contains this KTimeZone.
     * @param countryCode ISO 3166 2-character country code, empty if unknown
     * @param latitude    in degrees (between -90 and +90), UNKNOWN if not known
     * @param longitude   in degrees (between -180 and +180), UNKNOWN if not known
     * @param comment     description of the time zone, if any
     */
    KTimeZoneBackend(KTimeZoneSource *source, const QString &name,
                     const QString &countryCode = QString(), float latitude = KTimeZone::UNKNOWN,
                     float longitude = KTimeZone::UNKNOWN, const QString &comment = QString());

private:
    KTimeZonePrivate *d;   // non-const
    friend class KTimeZone;
};

/**
 * Base class representing a source of time zone information.
 *
 * Derive subclasses from KTimeZoneSource to read and parse time zone details
 * from a time zone database or other source of time zone information. If can know
 * in advance what KTimeZone instances to create without having to parse the source
 * data, you should reimplement the virtual method parse(const KTimeZone&). Otherwise,
 * you need to define your own parse() methods with appropriate signatures, to both
 * read and parse the new data, and create new KTimeZone instances.
 *
 * KTimeZoneSource itself may be used as a dummy source which returns empty
 * time zone details.
 *
 * @short Base class representing a source of time zone information
 * @see KTimeZone, KTimeZoneData
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 * @author S.R.Haque <srhaque@iee.org>.
 */
class KDECORE_EXPORT KTimeZoneSource
{
public:
    KTimeZoneSource();
    virtual ~KTimeZoneSource();

    /**
     * Extracts detail information for one time zone from the source database.
     *
     * In this base class, the method always succeeds and returns an empty data
     * instance. Derived classes should reimplement this method to return an
     * appropriate data class derived from KTimeZoneData. Some source databases
     * may not be compatible with this method of parsing. In these cases, they
     * should use the constructor KTimeZoneSource(false) and calling this method
     * will produce a fatal error.
     *
     * @param zone the time zone for which data is to be extracted.
     * @return an instance of a class derived from KTimeZoneData containing
     *         the parsed data. The caller is responsible for deleting the
     *         KTimeZoneData instance.
     *         Null is returned on error.
     */
    virtual KTimeZoneData *parse(const KTimeZone &zone) const;

    /**
     * Return whether the source database supports the ad hoc extraction of data for
     * individual time zones using parse(const KTimeZone&).
     *
     * @return true if parse(const KTimeZone&) works, false if parsing must be
     *         performed by other methods
     */
    bool useZoneParse() const;

protected:
    /**
     * Constructor for use by derived classes, which specifies whether the
     * source database supports the ad hoc extraction of data for individual
     * time zones using parse(const KTimeZone&).
     *
     * If parse(const KTimeZone&) cannot be used, KTimeZone derived classes
     * which use this KTimeZoneSource derived class must create a
     * KTimeZoneData object at construction time so that KTimeZone::data()
     * will always return a data object.
     *
     * Note the default constructor is equivalent to KTimeZoneSource(true), i.e.
     * it is only necessary to use this constructor if parse(const KTimeZone&)
     * does not work.
     *
     * @param useZoneParse true if parse(const KTimeZone&) works, false if
     *                     parsing must be performed by other methods
     */
    explicit KTimeZoneSource(bool useZoneParse);

private:
    KTimeZoneSourcePrivate * const d;
};


/**
 * Base class for the parsed data returned by a KTimeZoneSource class.
 *
 * It contains all the data available from the KTimeZoneSource class,
 * including, when available, a complete list of daylight savings time
 * changes and leap seconds adjustments.
 *
 * This base class can be instantiated, but contains no data.
 *
 * @short Base class for parsed time zone data
 * @see KTimeZone, KTimeZoneSource
 * @ingroup timezones
 * @author David Jarvie <djarvie@kde.org>.
 */
class KDECORE_EXPORT KTimeZoneData
{
    friend class KTimeZone;

public:
    KTimeZoneData();
    KTimeZoneData(const KTimeZoneData &c);
    virtual ~KTimeZoneData();
    KTimeZoneData &operator=(const KTimeZoneData &c);

    /**
     * Creates a new copy of this object.
     * The caller is responsible for deleting the copy.
     * Derived classes must reimplement this method to return a copy of the
     * calling instance.
     *
     * @return copy of this instance
     */
    virtual KTimeZoneData *clone() const;

    /**
     * Returns the complete list of time zone abbreviations. This may include
     * translations.
     *
     * @return the list of abbreviations.
     *         In this base class, it consists of the single string "UTC".
     * @see abbreviation()
     */
    virtual QList<QByteArray> abbreviations() const;

    /**
     * Returns the time zone abbreviation current at a specified time.
     *
     * @param utcDateTime UTC date/time. An error occurs if
     *                    @p utcDateTime.timeSpec() is not Qt::UTC.
     * @return time zone abbreviation, or empty string if error
     * @see abbreviations()
     */
    virtual QByteArray abbreviation(const QDateTime &utcDateTime) const;

    /**
     * Returns the complete list of UTC offsets for the time zone, if the time
     * zone's source makes such information readily available. If compiling a
     * complete list would require significant processing, an empty list is
     * returned instead.
     *
     * @return sorted list of UTC offsets, or empty list if not readily available.
     *         In this base class, it consists of the single value 0.
     */
    virtual QList<int> utcOffsets() const;

    /**
     * Returns the UTC offset to use before the start of data for the time zone.
     *
     * @return UTC offset
     */
    int previousUtcOffset() const;

    /**
     * Return all daylight savings time phases.
     *
     * Note that some time zone data sources (such as system time zones accessed
     * via the system libraries) may not allow a list of daylight savings time
     * changes to be compiled easily. In such cases, this method will return an
     * empty list.
     *
     * @return list of phases
     */
    QList<KTimeZone::Phase> phases() const;

    /**
     * Return whether daylight saving transitions are available for the time zone.
     *
     * The base class returns @c false.
     *
     * @return @c true if transitions are available, @c false if not
     * @see transitions(), transition()
     */
    virtual bool hasTransitions() const;

    /**
     * Return all daylight saving transitions, in time order. If desired, the
     * transitions returned may be restricted to a specified time range.
     *
     * Note that some time zone data sources (such as system time zones accessed
     * via the system libraries) may not allow a list of daylight saving time
     * changes to be compiled easily. In such cases, this method will return an
     * empty list.
     *
     * @param start start date/time, or invalid date/time to return all transitions up
     *              to @p end. @p start.timeSpec() must be Qt::UTC, else
     *              @p start will be considered invalid.
     * @param end end date/time, or invalid date/time for no end. @p end.timeSpec()
     *                must be Qt::UTC, else @p end will be considered invalid.
     * @return list of transitions, in time order
     * @see hasTransitions(), transition(), transitionTimes()
     */
    QList<KTimeZone::Transition> transitions(const QDateTime &start = QDateTime(), const QDateTime &end = QDateTime()) const;

    /**
     * Find the last daylight savings time transition at or before a given
     * UTC or local time.
     *
     * Because of daylight savings time shifts, a local time may occur twice or
     * may not occur at all. In the former case, the transitions at or before
     * both occurrences of @p dt may optionally be calculated and returned in
     * @p secondTransition. The latter case may optionally be detected by use of
     * @p validTime.
     *
     * @param dt date/time. @p dt.timeSpec() may be set to Qt::UTC or Qt::LocalTime.
     * @param secondTransition if non-null, and the @p dt occurs twice, receives the
     *                     transition for the second occurrence. Otherwise, it is set
     *                     the same as the return value.
     * @param validTime if non-null, is set to false if @p dt does not occur, or
     *                  to true otherwise
     * @return time zone transition, or null either if @p dt is either outside the
     *         defined range of the transition data or if @p dt does not occur
     * @see transitionIndex(), hasTransitions(), transitions()
     */
    const KTimeZone::Transition *transition(const QDateTime &dt, const KTimeZone::Transition **secondTransition = 0, bool *validTime = 0) const;

    /**
     * Find the index to the last daylight savings time transition at or before
     * a given UTC or local time. The return value is the index into the transition
     * list returned by transitions().
     *
     * Because of daylight savings time shifts, a local time may occur twice or
     * may not occur at all. In the former case, the transitions at or before
     * both occurrences of @p dt may optionally be calculated and returned in
     * @p secondIndex. The latter case may optionally be detected by use of
     * @p validTime.
     *
     * @param dt date/time. @p dt.timeSpec() may be set to Qt::UTC or Qt::LocalTime.
     * @param secondIndex if non-null, and the @p dt occurs twice, receives the
     *                    index to the transition for the second occurrence. Otherwise,
     *                    it is set the same as the return value.
     * @param validTime if non-null, is set to false if @p dt does not occur, or
     *                  to true otherwise
     * @return index into the time zone transition list, or -1 either if @p dt is
     *         either outside the defined range of the transition data or if @p dt
     *         does not occur
     * @see transition(), transitions(), hasTransitions()
     */
    int transitionIndex(const QDateTime &dt, int *secondIndex = 0, bool *validTime = 0) const;

    /**
     * Return the times of all daylight saving transitions to a given time zone
     * phase, in time order. If desired, the times returned may be restricted to
     * a specified time range.
     *
     * Note that some time zone data sources (such as system time zones accessed
     * via the system libraries) may not allow a list of daylight saving time
     * changes to be compiled easily. In such cases, this method will return an
     * empty list.
     *
     * @param phase time zone phase
     * @param start start UTC date/time, or invalid date/time to return all transitions
     *              up to @p end. @p start.timeSpec() must be Qt::UTC, else
     *              @p start will be considered invalid.
     * @param end end UTC date/time, or invalid date/time for no end. @p end.timeSpec()
     *                must be Qt::UTC, else @p end will be considered invalid.
     * @return ordered list of transition times
     * @see hasTransitions(), transition(), transitions()
     */
    QList<QDateTime> transitionTimes(const KTimeZone::Phase &phase, const QDateTime &start = QDateTime(), const QDateTime &end = QDateTime()) const;

    /**
     * Return all leap second adjustments, in time order.
     *
     * Note that some time zone data sources (such as system time zones accessed
     * via the system libraries) may not provide information on leap second
     * adjustments. In such cases, this method will return an empty list.
     *
     * @return list of adjustments
     */
    QList<KTimeZone::LeapSeconds> leapSecondChanges() const;

    /**
     * Find the leap second adjustment which is applicable at a given UTC time.
     *
     * @param utc UTC date/time. An error occurs if @p utc.timeSpec() is not Qt::UTC.
     * @return leap second adjustment, or invalid if @p utc is earlier than the
     *         first leap second adjustment or @p utc is a local time
     */
    KTimeZone::LeapSeconds leapSecondChange(const QDateTime &utc) const;

protected:
    /**
     * Initialise the daylight savings time phase list.
     *
     * @param phases list of phases
     * @param previousUtcOffset UTC offset to use before the start of the first
     *                          phase
     * @see phases()
     */
    void setPhases(const QList<KTimeZone::Phase> &phases, int previousUtcOffset);

    /**
     * Initialise the daylight savings time transition list.
     *
     * @param transitions list of transitions
     * @see transitions()
     */
    void setTransitions(const QList<KTimeZone::Transition> &transitions);

    /**
     * Initialise the leap seconds adjustment list.
     *
     * @param adjusts list of adjustments
     * @see leapSecondChanges()
     */
    void setLeapSecondChanges(const QList<KTimeZone::LeapSeconds> &adjusts);

private:
    KTimeZoneDataPrivate * const d;
};

#endif
