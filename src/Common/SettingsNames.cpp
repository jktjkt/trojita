/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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
#include "SettingsNames.h"

namespace Common
{

QString SettingsNames::identitiesKey = QLatin1String("identities");
QString SettingsNames::realNameKey = QLatin1String("realName");
QString SettingsNames::addressKey = QLatin1String("address");
QString SettingsNames::organisationKey = QLatin1String("organisation");
QString SettingsNames::signatureKey = QLatin1String("signature");
QString SettingsNames::obsRealNameKey = QLatin1String("identity.realName");
QString SettingsNames::obsAddressKey = QLatin1String("identity.address");
QString SettingsNames::msaMethodKey = QLatin1String("msa.method");
QString SettingsNames::methodSMTP = QLatin1String("SMTP");
QString SettingsNames::methodSSMTP = QLatin1String("SSMTP");
QString SettingsNames::methodSENDMAIL = QLatin1String("sendmail");
QString SettingsNames::methodImapSendmail = QLatin1String("IMAP-SENDMAIL");
QString SettingsNames::smtpHostKey = QLatin1String("msa.smtp.host");
QString SettingsNames::smtpPortKey = QLatin1String("msa.smtp.port");
QString SettingsNames::smtpAuthKey = QLatin1String("msa.smtp.auth");
QString SettingsNames::smtpStartTlsKey = QLatin1String("msa.smtp.starttls");
QString SettingsNames::smtpUserKey = QLatin1String("msa.smtp.auth.user");
QString SettingsNames::smtpPassKey = QLatin1String("msa.smtp.auth.pass");
QString SettingsNames::sendmailKey = QLatin1String("msa.sendmail");
QString SettingsNames::sendmailDefaultCmd = QLatin1String("sendmail -bm -oi");
QString SettingsNames::smtpUseBurlKey = QLatin1String("msa.smtp.burl");
QString SettingsNames::imapMethodKey = QLatin1String("imap.method");
QString SettingsNames::methodTCP = QLatin1String("TCP");
QString SettingsNames::methodSSL = QLatin1String("SSL");
QString SettingsNames::methodProcess = QLatin1String("process");
QString SettingsNames::imapHostKey = QLatin1String("imap.host");
QString SettingsNames::imapPortKey = QLatin1String("imap.port");
QString SettingsNames::imapStartTlsKey = QLatin1String("imap.starttls");
QString SettingsNames::imapUserKey = QLatin1String("imap.auth.user");
QString SettingsNames::imapPassKey = QLatin1String("imap.auth.pass");
QString SettingsNames::imapProcessKey = QLatin1String("imap.process");
QString SettingsNames::imapStartOffline = QLatin1String("imap.offline");
QString SettingsNames::imapEnableId = QLatin1String("imap.enableId");
QString SettingsNames::imapSslPemCertificate = QLatin1String("imap.ssl.pemCertificate");
QString SettingsNames::imapBlacklistedCapabilities = QLatin1String("imap.capabilities.blacklist");
QString SettingsNames::composerSaveToImapKey = QLatin1String("composer/saveToImapEnabled");
QString SettingsNames::composerImapSentKey = QLatin1String("composer/imapSentName");
QString SettingsNames::cacheMetadataKey = QLatin1String("offline.metadataCache");
QString SettingsNames::cacheMetadataMemory = QLatin1String("memory");
QString SettingsNames::cacheOfflineKey = QLatin1String("offline.cache");
QString SettingsNames::cacheOfflineNone = QLatin1String("memory");
QString SettingsNames::cacheOfflineXDays = QLatin1String("days");
QString SettingsNames::cacheOfflineAll = QLatin1String("all");
QString SettingsNames::cacheOfflineNumberDaysKey = QLatin1String("offline.cache.numDays");
QString SettingsNames::xtConnectCacheDirectory = QLatin1String("xtconnect.cachedir");
QString SettingsNames::xtSyncMailboxList = QLatin1String("xtconnect.listOfMailboxes");
QString SettingsNames::xtDbHost = QLatin1String("xtconnect.db.hostname");
QString SettingsNames::xtDbPort = QLatin1String("xtconnect.db.port");
QString SettingsNames::xtDbDbName = QLatin1String("xtconnect.db.dbname");
QString SettingsNames::xtDbUser = QLatin1String("xtconnect.db.username");
QString SettingsNames::guiMsgListShowThreading = QLatin1String("gui/msgList.showThreading");
QString SettingsNames::guiMsgListHideRead = QLatin1String("gui/msgList.hideRead");
QString SettingsNames::guiMailboxListShowOnlySubscribed = QLatin1String("gui/mailboxList.showOnlySubscribed");
QString SettingsNames::guiMainWindowLayout = QLatin1String("gui/mainWindow.layout");
QString SettingsNames::guiMainWindowLayoutCompact = QLatin1String("compact");
QString SettingsNames::guiMainWindowLayoutWide = QLatin1String("wide");
QString SettingsNames::guiMainWindowLayoutOneAtTime = QLatin1String("one-at-time");
QString SettingsNames::guiPreferPlaintextRendering = QLatin1String("gui/preferPlaintextRendering");
QString SettingsNames::guiShowSystray = QLatin1String("gui/showSystray");
QString SettingsNames::guiOnSystrayClose = QLatin1String("gui/onSystrayClose");
QString SettingsNames::guiSizesInMainWinWhenCompact = QLatin1String("gui/sizeInMainWinWhenCompact-%1");
QString SettingsNames::guiSizesInMainWinWhenWide = QLatin1String("gui/sizeInMainWinWhenWide-%1");
QString SettingsNames::appLoadHomepage = QLatin1String("app.updates.checkEnabled");
QString SettingsNames::knownEmailsKey = QLatin1String("addressBook/knownEmails");

}
