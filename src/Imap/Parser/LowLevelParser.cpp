/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <limits>
#include <QPair>
#include <QStringList>
#include <QVariant>
#include <QDateTime>
#include "LowLevelParser.h"
#include "../Exceptions.h"
#include "Imap/Encoders.h"

namespace Imap
{
namespace LowLevelParser
{

template<typename T> T extractNumber(const QByteArray &line, int &start)
{
    if (start >= line.size())
        throw NoData("extractNumber: no data", line, start);

    const char *c_str = line.constData();
    c_str += start;

    if (*c_str < '0' || *c_str > '9')
        throw ParseError("extractNumber: not a number", line, start);

    T res = 0;
    // well, it's an inline function, but clang still won't cache its result by default
    const T absoluteMax = std::numeric_limits<T>::max();
    const T softLimit = (absoluteMax - 10) / 10;
    while (*c_str >= '0' && *c_str <= '9') {
        auto digit = *c_str - '0';
        if (res <= softLimit) {
            res *= 10;
            res += digit;
        } else {
            if (res > absoluteMax / 10)
                throw ParseError("extractNumber: out of range", line, start);
            res *= 10;
            if (res > absoluteMax - digit)
                throw ParseError("extractNumber: out of range", line, start);
            res += digit;
        }
        ++c_str;
        ++start;
    }
    return res;
}

uint getUInt(const QByteArray &line, int &start)
{
    return extractNumber<uint>(line, start);
}

quint64 getUInt64(const QByteArray &line, int &start)
{
    return extractNumber<quint64>(line, start);
}

#define C_STR_CHECK_FOR_ATOM_CHARS \
    *c_str > '\x20' && *c_str != '\x7f' /* SP and CTL */ \
        && *c_str != '(' && *c_str != ')' && *c_str != '{' /* explicitly forbidden */ \
        && *c_str != '%' && *c_str != '*' /* list-wildcards */ \
        && *c_str != '"' && *c_str != '\\' /* quoted-specials */ \
        && *c_str != ']' /* resp-specials */

bool startsWithNil(const QByteArray &line, int start)
{
    const char *c_str = line.constData();
    c_str += start;
    // Case-insensitive NIL. We cannot use strncasecmp because that function respects locale settings which
    // is absolutely not something we want to do here.
    if (!(start <= line.size() + 3 && (*c_str == 'N' || *c_str == 'n') && (*(c_str+1) == 'I' || *(c_str+1) == 'i')
            && (*(c_str+2) == 'L' || *(c_str+2) == 'l'))) {
        return false;
    }
    // At this point we know that it starts with a NIL. To prevent parsing ambiguity with atoms, we have to
    // check the next character.
    c_str += 3;
    // That macro already checks for NULL bytes and the input is guaranteed to be null-terminated, so we're safe here
    if (C_STR_CHECK_FOR_ATOM_CHARS) {
        // The next character is apparently a valid atom-char, so this cannot possibly be a NIL
        return false;
    }
    return true;
}

QByteArray getAtom(const QByteArray &line, int &start)
{
    if (start == line.size())
        throw NoData("getAtom: no data", line, start);

    const char *c_str = line.constData();
    c_str += start;
    const char * const old_str = c_str;

    while (C_STR_CHECK_FOR_ATOM_CHARS) {
        ++c_str;
    }

    auto size = c_str - old_str;
    if (!size)
        throw ParseError("getAtom: did not read anything", line, start);
    start += size;
    return QByteArray(old_str, size);
}

/** @short Special variation of getAtom which also accepts leading backslash */
QByteArray getPossiblyBackslashedAtom(const QByteArray &line, int &start)
{
    if (start == line.size())
        throw NoData("getPossiblyBackslashedAtom: no data", line, start);

    const char *c_str = line.constData();
    c_str += start;
    const char * const old_str = c_str;

    if (*c_str == '\\')
        ++c_str;

    while (C_STR_CHECK_FOR_ATOM_CHARS) {
        ++c_str;
    }

    auto size = c_str - old_str;
    if (!size)
        throw ParseError("getPossiblyBackslashedAtom: did not read anything", line, start);
    start += size;
    return QByteArray(old_str, size);
}

QPair<QByteArray,ParsedAs> getString(const QByteArray &line, int &start)
{
    if (start == line.size())
        throw NoData("getString: no data", line, start);

    if (line[start] == '"') {
        // quoted string
        ++start;
        bool escaping = false;
        QByteArray res;
        bool terminated = false;
        while (start != line.size() && !terminated) {
            if (escaping) {
                escaping = false;
                if (line[start] == '"' || line[start] == '\\')
                    res.append(line[start]);
                else
                    throw UnexpectedHere("getString: escaping invalid character", line, start);
            } else {
                switch (line[start]) {
                case '"':
                    terminated = true;
                    break;
                case '\\':
                    escaping = true;
                    break;
                case '\r': case '\n':
                    throw ParseError("getString: premature end of quoted string", line, start);
                default:
                    res.append(line[start]);
                }
            }
            ++start;
        }
        if (!terminated)
            throw NoData("getString: unterminated quoted string", line, start);
        return qMakePair(res, QUOTED);
    } else if (line[start] == '{') {
        // literal
        ++start;
        int size = getUInt(line, start);
        if (line.mid(start, 3) != "}\r\n")
            throw ParseError("getString: malformed literal specification", line, start);
        start += 3;
        if (start + size > line.size())
            throw NoData("getString: run out of data", line, start);
        int old(start);
        start += size;
        return qMakePair(line.mid(old, size), LITERAL);
    } else if (start < line.size() - 3 && line[start] == '~' && line[start + 1] == '{' ) {
        // literal8
        start += 2;
        int size = getUInt(line, start);
        if (line.mid(start, 3) != "}\r\n")
            throw ParseError("getString: malformed literal8 specification", line, start);
        start += 3;
        if (start + size > line.size())
            throw NoData("getString: literal8: run out of data", line, start);
        int old(start);
        start += size;
        return qMakePair(line.mid(old, size), LITERAL8);
    } else {
        throw UnexpectedHere("getString: did not get quoted string or literal", line, start);
    }
}

QPair<QByteArray,ParsedAs> getAString(const QByteArray &line, int &start)
{
    if (start >= line.size())
        throw NoData("getAString: no data", line, start);

    if (line[start] == '{' || line[start] == '"' || line[start] == '~')
        return getString(line, start);
    else
        return qMakePair(getAtom(line, start), ATOM);
}

QPair<QByteArray,ParsedAs> getNString(const QByteArray &line, int &start)
{
    if (startsWithNil(line, start)) {
        start += 3;
        return qMakePair<>(QByteArray(), NIL);
    } else {
        return getAString(line, start);
    }
}

QString getMailbox(const QByteArray &line, int &start)
{
    QPair<QByteArray,ParsedAs> r = getAString(line, start);
    if (r.first.toUpper() == "INBOX")
        return QLatin1String("INBOX");
    else
        return decodeImapFolderName(r.first);

}

QVariantList parseList(const char open, const char close, const QByteArray &line, int &start)
{
    if (start >= line.size())
        throw NoData("Could not parse list: no more data", line, start);

    if (line[start] == open) {
        // found the opening parenthesis
        ++start;
        if (start >= line.size())
            throw NoData("Could not parse list: just the opening bracket", line, start);

        QVariantList res;
        if (line[start] == close) {
            ++start;
            return res;
        }
        while (line[start] != close) {
            // We want to be benevolent here and eat extra whitespace
            eatSpaces(line, start);
            res.append(getAnything(line, start));
            if (start >= line.size())
                throw NoData("Could not parse list: truncated data", line, start);
            // Eat whitespace after each token, too
            eatSpaces(line, start);
            if (line[start] == close) {
                ++start;
                return res;
            }
        }
        return res;
    } else {
        throw UnexpectedHere(std::string("Could not parse list: expected a list enclosed in ")
                             + open + close + ", but got something else instead", line, start);
    }
}

QVariant getAnything(const QByteArray &line, int &start)
{
    if (start >= line.size())
        throw NoData("getAnything: no data", line, start);

    if (line[start] == '[') {
        QVariant res = parseList('[', ']', line, start);
        return res;
    } else if (line[start] == '(') {
        QVariant res = parseList('(', ')', line, start);
        return res;
    } else if (line[start] == '"' || line[start] == '{' || line[start] == '~') {
        QPair<QByteArray,ParsedAs> res = getString(line, start);
        return res.first;
    } else if (startsWithNil(line, start)) {
        start += 3;
        return QByteArray();
    } else if (line[start] == '\\') {
        // valid for "flag"
        ++start;
        if (start >= line.size())
            throw NoData("getAnything: backslash-nothing is invalid", line, start);
        if (line[start] == '*') {
            ++start;
            return QByteArray("\\*");
        }
        return QByteArray(1, '\\') + getAtom(line, start);
    } else if (line[start] >= '0' && line[start] <= '9') {
        quint64 res = getUInt64(line, start);
        if (res <= std::numeric_limits<quint32>::max())
            return static_cast<quint32>(res);
        else
            return res;
    } else {
        QByteArray atom = getAtom(line, start);
        if (atom.indexOf('[', 0) != -1) {
            // "BODY[something]" -- there's no whitespace between "[" and
            // next atom...
            int pos = line.indexOf(']', start);
            if (pos == -1)
                throw ParseError("getAnything: can't find ']' for the '['", line, start);
            ++pos;
            atom += line.mid(start, pos - start);
            start = pos;
            if (start < line.size() && line[start] == '<') {
                // Let's check if it continues with "<range>"
                pos = line.indexOf('>', start);
                if (pos == -1)
                    throw ParseError("getAnything: can't find proper <range>", line, start);
                ++pos;
                atom += line.mid(start, pos - start);
                start = pos;
            }
        }
        return atom;
    }
}

QList<uint> getSequence(const QByteArray &line, int &start)
{
    uint num = LowLevelParser::getUInt(line, start);
    if (start >= line.size() - 2) {
        // It's definitely just a number because there's no more data in here
        return QList<uint>() << num;
    } else {
        QList<uint> numbers;
        numbers << num;

        enum {COMMA, RANGE} currentType = COMMA;

        // Try to find further items in the sequence set
        while (line[start] == ':' || line[start] == ',') {
            // it's a sequence set

            if (line[start] == ':') {
                if (currentType == RANGE) {
                    // Now "x:y:z" is a funny syntax
                    throw UnexpectedHere("Sequence set: range cannot me defined by three numbers", line, start);
                }
                currentType = RANGE;
            } else {
                currentType = COMMA;
            }

            ++start;
            if (start >= line.size() - 2) throw NoData("Truncated sequence set", line, start);

            uint num = LowLevelParser::getUInt(line, start);
            if (currentType == COMMA) {
                // just adding one more to the set
                numbers << num;
            } else {
                // working with a range
                if (numbers.last() >= num)
                    throw UnexpectedHere("Sequence set contains an invalid range. "
                                         "First item of a range must always be smaller than the second item.", line, start);

                for (uint i = numbers.last() + 1; i <= num; ++i)
                    numbers << i;
            }
        }
        return numbers;
    }
}

QDateTime parseRFC2822DateTime(const QString &string)
{
    QStringList monthNames = QStringList() << QLatin1String("jan") << QLatin1String("feb") << QLatin1String("mar")
                                           << QLatin1String("apr") << QLatin1String("may") << QLatin1String("jun")
                                           << QLatin1String("jul") << QLatin1String("aug") << QLatin1String("sep")
                                           << QLatin1String("oct") << QLatin1String("nov") << QLatin1String("dec");

    QRegExp rx(QString::fromUtf8("^(?:\\s*([A-Z][a-z]+)\\s*,\\s*)?"   // date-of-week
                                 "(\\d{1,2})\\s+(%1)\\s+(\\d{2,4})" // date
                                 "\\s+(\\d{1,2})\\s*:(\\d{1,2})\\s*(?::\\s*(\\d{1,2})\\s*)?" // time
                                 "(\\s+(?:(?:([+-]?)(\\d{2})(\\d{2}))|(UT|GMT|EST|EDT|CST|CDT|MST|MDT|PST|PDT|[A-IK-Za-ik-z])))?" // timezone
                                 ).arg(monthNames.join(QLatin1String("|"))), Qt::CaseInsensitive);
    int pos = rx.indexIn(string);

    if (pos == -1)
        throw ParseError("Date format not recognized");

    QStringList list = rx.capturedTexts();

    if (list.size() != 13)
        throw ParseError("Date regular expression returned weird data (internal error?)");

    int year = list[4].toInt();
    int month = monthNames.indexOf(list[3].toLower()) + 1;
    if (month == 0)
        throw ParseError("Invalid month name");
    int day = list[2].toInt();
    int hours = list[5].toInt();
    int minutes = list[6].toInt();
    int seconds = list[7].toInt();
    int shift = list[10].toInt() * 60 + list[11].toInt();
    if (list[9] == QLatin1String("-"))
        shift *= 60;
    else
        shift *= -60;
    if (! list[12].isEmpty()) {
        const QString tz = list[12].toUpper();
        if (tz == QLatin1String("UT") || tz == QLatin1String("GMT"))
            shift = 0;
        else if (tz == QLatin1String("EST"))
            shift = 5 * 3600;
        else if (tz == QLatin1String("EDT"))
            shift = 4 * 3600;
        else if (tz == QLatin1String("CST"))
            shift = 6 * 3600;
        else if (tz == QLatin1String("CDT"))
            shift = 5 * 3600;
        else if (tz == QLatin1String("MST"))
            shift = 7 * 3600;
        else if (tz == QLatin1String("MDT"))
            shift = 6 * 3600;
        else if (tz == QLatin1String("PST"))
            shift = 8 * 3600;
        else if (tz == QLatin1String("PDT"))
            shift = 7 * 3600;
        else if (tz.size() == 1)
            shift = 0;
        else
            throw ParseError("Invalid TZ specification");
    }

    QDateTime date(QDate(year, month, day), QTime(hours, minutes, seconds), Qt::UTC);
    date = date.addSecs(shift);

    return date;
}

void eatSpaces(const QByteArray &line, int &start)
{
    while (line.size() > start && line[start] == ' ')
        ++start;
}

}
}
