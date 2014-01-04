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
#ifndef SETTINGSNAMES_H
#define SETTINGSNAMES_H

#include <QString>

namespace Common
{

struct SettingsNames {
    static const QString identitiesKey, realNameKey, addressKey, organisationKey, signatureKey, obsRealNameKey, obsAddressKey;
    static const QString msaMethodKey, methodSMTP, methodSSMTP, methodSENDMAIL, methodImapSendmail, smtpHostKey,
           smtpPortKey, smtpAuthKey, smtpStartTlsKey, smtpUserKey, smtpPassKey,
           sendmailKey, sendmailDefaultCmd;
    static const QString imapMethodKey, methodTCP, methodSSL, methodProcess, imapHostKey,
           imapPortKey, imapStartTlsKey, imapUserKey, imapPassKey, imapProcessKey,
           imapStartOffline, imapEnableId, obsImapSslPemCertificate, imapSslPemPubKey, imapBlacklistedCapabilities;
    static const QString composerSaveToImapKey, composerImapSentKey, smtpUseBurlKey;
    static const QString cacheMetadataKey, cacheMetadataMemory,
           cacheOfflineKey, cacheOfflineNone, cacheOfflineXDays, cacheOfflineAll, cacheOfflineNumberDaysKey;
    static const QString xtConnectCacheDirectory, xtSyncMailboxList, xtDbHost, xtDbPort,
           xtDbDbName, xtDbUser;
    static const QString guiMsgListShowThreading;
    static const QString guiMsgListHideRead;
    static const QString guiMailboxListShowOnlySubscribed;
    static const QString guiPreferPlaintextRendering;
    static const QString guiMainWindowLayout, guiMainWindowLayoutCompact, guiMainWindowLayoutWide, guiMainWindowLayoutOneAtTime;
    static const QString guiSizesInMainWinWhenCompact, guiSizesInMainWinWhenWide;
    static const QString guiAllowRawSearch;
    static const QString appLoadHomepage;
    static const QString guiShowSystray, guiOnSystrayClose;
    static const QString knownEmailsKey;
    static const QString addressbookPlugin, passwordPlugin;
};

}

#endif // SETTINGSNAMES_H
