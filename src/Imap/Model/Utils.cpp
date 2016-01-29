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
#include "Utils.h"
#include <QAbstractProxyModel>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QProcess>
#include <QSettings>
#include <QSslCertificate>
#include <QSslKey>
#include <QSysInfo>

#include "Common/Paths.h"
#include "Common/SettingsNames.h"
#include "Imap/Model/NetworkPolicy.h"

#ifdef TROJITA_MOBILITY_SYSTEMINFO
#include <QSystemDeviceInfo>
#endif

namespace Imap {
namespace Mailbox {

QString persistentLogFileName()
{
    QString logFileName = Common::writablePath(Common::LOCATION_CACHE);
    if (logFileName.isEmpty()) {
        logFileName = QDir::homePath() + QLatin1String("/.trojita-connection-log");
    } else {
        QDir().mkpath(logFileName);
        logFileName += QLatin1String("/trojita-connection-log");
    }
    return logFileName;
}

QString systemPlatformVersion()
{
    QString os = QStringLiteral(""
#ifdef Q_OS_AIX
                                    "AIX"
#endif
#ifdef Q_OS_BSD4
#ifndef Q_OS_MAC
                                    "AnyBSD4.4"
#endif
#endif
#ifdef Q_OS_BSDI
                                    "BSD/OS"
#endif
#ifdef Q_OS_CYGWIN
                                    "Cygwin"
#endif
#ifdef Q_OS_DGUX
                                    "DG/UX"
#endif
#ifdef Q_OS_DYNIX
                                    "DYNIX/ptx"
#endif
#ifdef Q_OS_FREEBSD
                                    "FreeBSD"
#endif
#ifdef Q_OS_HPUX
                                    "HP-UX"
#endif
#ifdef Q_OS_HURD
                                    "Hurd"
#endif
#ifdef Q_OS_IRIX
                                    "Irix"
#endif
#ifdef Q_OS_LINUX
                                    "Linux"
#endif
#ifdef Q_OS_LYNX
                                    "LynxOS"
#endif
#ifdef Q_OS_MAC
                                    "MacOS"
#endif
#ifdef Q_OS_MSDOS
                                    "MSDOS"
#endif
#ifdef Q_OS_NETBSD
                                    "NetBSD"
#endif
#ifdef Q_OS_OS2
                                    "OS/2"
#endif
#ifdef Q_OS_OPENBSD
                                    "OpenBSD"
#endif
#ifdef Q_OS_OS2EMX
                                    "OS2EMX"
#endif
#ifdef Q_OS_OSF
                                    "HPTru64UNIX"
#endif
#ifdef Q_OS_QNX
                                    "QNXNeutrino"
#endif
#ifdef Q_OS_RELIANT
                                    "ReliantUNIX"
#endif
#ifdef Q_OS_SCO
                                    "SCOOpenServer5"
#endif
#ifdef Q_OS_SOLARIS
                                    "Solaris"
#endif
#ifdef Q_OS_SYMBIAN
                                    "Symbian"
#endif
#ifdef Q_OS_ULTRIX
                                    "Ultrix"
#endif
#ifdef Q_OS_UNIXWARE
                                    "Unixware"
#endif
#ifdef Q_OS_WIN32
                                    "Windows"
#endif
#ifdef Q_OS_WINCE
                                    "WinCE"
#endif
                                   );
#ifdef Q_OS_UNIX
    if (os.isEmpty()) {
        os = QStringLiteral("Unix");
    }
#endif

    static QString platformVersion;
#ifdef TROJITA_MOBILITY_SYSTEMINFO
    if (platformVersion.isEmpty()) {
        QtMobility::QSystemDeviceInfo device;
        if (device.productName().isEmpty()) {
            platformVersion = device.manufacturer() + QLatin1String(" ") + device.model();
        } else {
            platformVersion = QString::fromAscii("%1 %2 (%3)").arg(device.manufacturer(), device.productName(), device.model());
        }
    }
#endif
    if (platformVersion.isEmpty()) {
#if defined(Q_OS_WIN32) || defined(Q_OS_WINCE)
        switch (QSysInfo::WindowsVersion) {
        case QSysInfo::WV_32s:
            platformVersion = QLatin1String("3.1");
            break;
        case QSysInfo::WV_95:
            platformVersion = QLatin1String("95");
            break;
        case QSysInfo::WV_98:
            platformVersion = QLatin1String("98");
            break;
        case QSysInfo::WV_Me:
            platformVersion = QLatin1String("Me");
            break;
        case QSysInfo::WV_NT:
            platformVersion = QLatin1String("NT");
            break;
        case QSysInfo::WV_2000:
            platformVersion = QLatin1String("2000");
            break;
        case QSysInfo::WV_XP:
            platformVersion = QLatin1String("XP");
            break;
        case QSysInfo::WV_2003:
            platformVersion = QLatin1String("2003");
            break;
        case QSysInfo::WV_VISTA:
            platformVersion = QLatin1String("Vista");
            break;
        case QSysInfo::WV_WINDOWS7:
            platformVersion = QLatin1String("7");
            break;
        case QSysInfo::WV_WINDOWS8:
            platformVersion = QLatin1String("8");
            break;
        case QSysInfo::WV_WINDOWS8_1:
            platformVersion = QLatin1String("8.1");
            break;
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
        case QSysInfo::WV_WINDOWS10:
            platformVersion = QLatin1String("10");
            break;
#endif
        case QSysInfo::WV_CE:
            platformVersion = QLatin1String("CE");
            break;
        case QSysInfo::WV_CENET:
            platformVersion = QLatin1String("CE.NET");
            break;
        case QSysInfo::WV_CE_5:
            platformVersion = QLatin1String("CE5.x");
            break;
        case QSysInfo::WV_CE_6:
            platformVersion = QLatin1String("CE6.x");
            break;
        case QSysInfo::WV_DOS_based:
            platformVersion = QLatin1String("DOS-based");
            break;
        case QSysInfo::WV_NT_based:
            platformVersion = QLatin1String("NT-based");
            break;
        case QSysInfo::WV_CE_based:
            platformVersion = QLatin1String("CE-based");
            break;
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
        case QSysInfo::WV_None:
            platformVersion = QLatin1String("non-Windows");
            break;
#endif
        }
#endif
#ifdef Q_OS_MAC
        switch (QSysInfo::MacintoshVersion) {
        case QSysInfo::MV_9:
            platformVersion = QLatin1String("9.0");
            break;
        case QSysInfo::MV_10_0:
            platformVersion = QLatin1String("X 10.0");
            break;
        case QSysInfo::MV_10_1:
            platformVersion = QLatin1String("X 10.1");
            break;
        case QSysInfo::MV_10_2:
            platformVersion = QLatin1String("X 10.2");
            break;
        case QSysInfo::MV_10_3:
            platformVersion = QLatin1String("X 10.3");
            break;
        case QSysInfo::MV_10_4:
            platformVersion = QLatin1String("X 10.4");
            break;
        case QSysInfo::MV_10_5:
            platformVersion = QLatin1String("X 10.5");
            break;
        case QSysInfo::MV_10_6:
            platformVersion = QLatin1String("X 10.6");
            break;
        case QSysInfo::MV_10_7:
            platformVersion = QLatin1String("X 10.7");
            break;
        case QSysInfo::MV_10_8:
            platformVersion = QLatin1String("X 10.8");
            break;
        case QSysInfo::MV_10_9:
            platformVersion = QLatin1String("X 10.9");
            break;
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
        case QSysInfo::MV_10_10:
            platformVersion = QLatin1String("X 10.10");
            break;
#endif
        case QSysInfo::MV_IOS:
            platformVersion = QLatin1String("iOS");
            break;
        case QSysInfo::MV_IOS_4_3:
            platformVersion = QLatin1String("iOS 4.3");
            break;
        case QSysInfo::MV_IOS_5_0:
            platformVersion = QLatin1String("iOS 5.0");
            break;
        case QSysInfo::MV_IOS_5_1:
            platformVersion = QLatin1String("iOS 5.1");
            break;
        case QSysInfo::MV_IOS_6_0:
            platformVersion = QLatin1String("iOS 6.0");
            break;
        case QSysInfo::MV_IOS_6_1:
            platformVersion = QLatin1String("iOS 6.1");
            break;
        case QSysInfo::MV_IOS_7_0:
            platformVersion = QLatin1String("iOS 7.0");
            break;
        case QSysInfo::MV_IOS_7_1:
            platformVersion = QLatin1String("iOS 7.1");
            break;
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
        case QSysInfo::MV_IOS_8_0:
            platformVersion = QLatin1String("iOS 8.0");
            break;
#endif
        case QSysInfo::MV_Unknown:
            platformVersion = QLatin1String("iOS (unknown)");
            break;
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
        case QSysInfo::MV_None:
            platformVersion = QLatin1String("non-Mac");
            break;
#endif
        }
#endif
        if (platformVersion.isEmpty()) {
            // try to call the lsb_release
            QProcess *proc = new QProcess(0);
            proc->start(QStringLiteral("lsb_release"), QStringList() << QStringLiteral("-s") << QStringLiteral("-d"));
            proc->waitForFinished();
            platformVersion = QString::fromLocal8Bit(proc->readAll()).trimmed().replace(QLatin1Char('"'), QString()).replace(QLatin1Char(';'), QLatin1Char(','));
            proc->deleteLater();
        }
    }
    return QStringLiteral("Qt/%1; %2; %3; %4").arg(QString::fromUtf8(qVersion()), QGuiApplication::platformName(), os, platformVersion);
}

}

/** @short Return current date in the RFC2822 format

This function will return RFC2822-formatted current date with proper timezone information and all the glory details.
It's rather surprising how hard is to do that in Qt -- why is there no accessor for the timezone info which is *already stored*
in QDateTime?
*/
QString formatDateTimeWithTimeZoneAtEnd(const QDateTime &now, const QString &format)
{
    // At first, try to find out the offset of the current timezone. That's the part which sucks.
    // If there's a more Qt-ish way of doing that, please let me know.
    // that's right, both of these command are actually needed
    QDateTime nowUtc = now.toUTC();
    nowUtc.setTimeSpec(Qt::LocalTime);

    // Got to cast to a signed type to prevent unsigned underflow here. Also go to 64bits because otherwise there'd
    // a problem when the value is out-of-range for an int32.
    int minutesDifference = (static_cast<qint64>(now.toTime_t()) - static_cast<qint64>(nowUtc.toTime_t())) / 60;
    int tzOffsetHours = qAbs(minutesDifference) / 60;
    int tzOffsetMinutes = qAbs(minutesDifference) % 60;
    // The rest is just a piece of cake now

    return QLocale(QStringLiteral("C")).toString(now, format) +
            QLatin1String(minutesDifference >= 0 ? "+" : "-") +
            QString::number(tzOffsetHours).rightJustified(2, QLatin1Char('0')) +
            QString::number(tzOffsetMinutes).rightJustified(2, QLatin1Char('0'));
}

/** @short Return current date in the RFC2822 format */
QString dateTimeToRfc2822(const QDateTime &now)
{
    return formatDateTimeWithTimeZoneAtEnd(now, QStringLiteral("ddd, dd MMM yyyy hh:mm:ss "));
}

/** @short Return current date in the RFC3501's INTERNALDATE format */
QString dateTimeToInternalDate(const QDateTime &now)
{
    return formatDateTimeWithTimeZoneAtEnd(now, QStringLiteral("dd-MMM-yyyy hh:mm:ss "));
}

/** @short Migrate old application settings to the new format */
void migrateSettings(QSettings *settings)
{
    using Common::SettingsNames;

    // Process the obsolete settings about the "cache backend". This has been changed to "offline stuff" after v0.3.
    if (settings->value(SettingsNames::cacheMetadataKey).toString() == SettingsNames::cacheMetadataMemory) {
        settings->setValue(SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineNone);
        settings->remove(SettingsNames::cacheMetadataKey);

        // Also remove the older values used for cache lifetime management which were not used, but set to zero by default
        settings->remove(QStringLiteral("offline.sync"));
        settings->remove(QStringLiteral("offline.sync.days"));
        settings->remove(QStringLiteral("offline.sync.messages"));
    }

    // Migrate the "last known certificate" from the full PEM format to just the pubkey
    QByteArray lastKnownCertPem = settings->value(SettingsNames::obsImapSslPemCertificate).toByteArray();
    if (!lastKnownCertPem.isEmpty()) {
        QList<QSslCertificate> oldChain = QSslCertificate::fromData(lastKnownCertPem, QSsl::Pem);
        if (!oldChain.isEmpty()) {
            settings->setValue(SettingsNames::imapSslPemPubKey, oldChain[0].publicKey().toPem());
        }
    }
    settings->remove(SettingsNames::obsImapSslPemCertificate);

    // Migration of the sender identities
    bool needsIdentityMigration = settings->beginReadArray(SettingsNames::identitiesKey) == 0;
    settings->endArray();
    if (needsIdentityMigration) {
        QString realName = settings->value(SettingsNames::obsRealNameKey).toString();
        QString email = settings->value(SettingsNames::obsAddressKey).toString();
        if (!realName.isEmpty() || !email.isEmpty()) {
            settings->beginWriteArray(SettingsNames::identitiesKey);
            settings->setArrayIndex(0);
            settings->setValue(SettingsNames::realNameKey, realName);
            settings->setValue(SettingsNames::addressKey, email);
            settings->endArray();
            settings->remove(Common::SettingsNames::obsRealNameKey);
            settings->remove(Common::SettingsNames::obsAddressKey);
        }
    }

    QVariant offlineSetting = settings->value(SettingsNames::obsImapStartOffline);
    if (offlineSetting.isValid()) {
        settings->setValue(SettingsNames::imapStartMode, offlineSetting.toBool() ? Common::SettingsNames::netOffline : Common::SettingsNames::netOnline);
        settings->remove(SettingsNames::obsImapStartOffline);
    }

    const QString obsImapEnableId = QStringLiteral("imap.enableId");
    auto enableId = settings->value(obsImapEnableId);
    if (enableId.isValid() && enableId.toBool() == false && !settings->contains(Common::SettingsNames::interopRevealVersions)) {
        settings->setValue(Common::SettingsNames::interopRevealVersions, QVariant(false));
        settings->remove(obsImapEnableId);
    }
}

/** @short Return the matching QModelIndex after stripping all proxy layers */
QModelIndex deproxifiedIndex(const QModelIndex& index)
{
    QModelIndex res = index;
    while (const QAbstractProxyModel *proxy = qobject_cast<const QAbstractProxyModel *>(res.model())) {
        res = proxy->mapToSource(res);
    }
    return res;
}

/** @short Recursively removes a directory and all its contents

This by some crazy voodoo unintentional 'magic', is almost identical
to a solution by John Schember, which was pointed out by jkt.

http://john.nachtimwald.com/2010/06/08/qt-remove-directory-and-its-contents/
*/
bool removeRecursively(const QString &dirName)
{
    bool result = true;
    QDir dir = dirName;

    if (dir.exists(dirName)) {
        Q_FOREACH(const QFileInfo &fileInfo,
                  dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files,
                                    QDir::DirsFirst)) {
            if (fileInfo.isDir()) {
                result = removeRecursively(fileInfo.absoluteFilePath());
            } else {
                result = QFile::remove(fileInfo.absoluteFilePath());
            }
            if (!result) {
                return result;
            }
        }
        result = dir.rmdir(dirName);
    }
    return result;
}

}
