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
#include "SettingsNames.h"

namespace Common
{

const QString SettingsNames::identitiesKey = QLatin1String("identities");
const QString SettingsNames::realNameKey = QLatin1String("realName");
const QString SettingsNames::addressKey = QLatin1String("address");
const QString SettingsNames::organisationKey = QLatin1String("organisation");
const QString SettingsNames::signatureKey = QLatin1String("signature");
const QString SettingsNames::obsRealNameKey = QLatin1String("identity.realName");
const QString SettingsNames::obsAddressKey = QLatin1String("identity.address");
const QString SettingsNames::msaMethodKey = QLatin1String("msa.method");
const QString SettingsNames::methodSMTP = QLatin1String("SMTP");
const QString SettingsNames::methodSSMTP = QLatin1String("SSMTP");
const QString SettingsNames::methodSENDMAIL = QLatin1String("sendmail");
const QString SettingsNames::methodImapSendmail = QLatin1String("IMAP-SENDMAIL");
const QString SettingsNames::smtpHostKey = QLatin1String("msa.smtp.host");
const QString SettingsNames::smtpPortKey = QLatin1String("msa.smtp.port");
const QString SettingsNames::smtpAuthKey = QLatin1String("msa.smtp.auth");
const QString SettingsNames::smtpStartTlsKey = QLatin1String("msa.smtp.starttls");
const QString SettingsNames::smtpUserKey = QLatin1String("msa.smtp.auth.user");
const QString SettingsNames::smtpPassKey = QLatin1String("msa.smtp.auth.pass");
const QString SettingsNames::sendmailKey = QLatin1String("msa.sendmail");
const QString SettingsNames::sendmailDefaultCmd = QLatin1String("sendmail -bm -oi");
const QString SettingsNames::smtpUseBurlKey = QLatin1String("msa.smtp.burl");
const QString SettingsNames::imapMethodKey = QLatin1String("imap.method");
const QString SettingsNames::methodTCP = QLatin1String("TCP");
const QString SettingsNames::methodSSL = QLatin1String("SSL");
const QString SettingsNames::methodProcess = QLatin1String("process");
const QString SettingsNames::imapHostKey = QLatin1String("imap.host");
const QString SettingsNames::imapPortKey = QLatin1String("imap.port");
const QString SettingsNames::imapStartTlsKey = QLatin1String("imap.starttls");
const QString SettingsNames::imapUserKey = QLatin1String("imap.auth.user");
const QString SettingsNames::imapPassKey = QLatin1String("imap.auth.pass");
const QString SettingsNames::imapProcessKey = QLatin1String("imap.process");
const QString SettingsNames::imapStartOffline = QLatin1String("imap.offline");
const QString SettingsNames::imapEnableId = QLatin1String("imap.enableId");
const QString SettingsNames::obsImapSslPemCertificate = QLatin1String("imap.ssl.pemCertificate");
const QString SettingsNames::imapSslPemPubKey = QLatin1String("imap.ssl.pemPubKey");
const QString SettingsNames::imapBlacklistedCapabilities = QLatin1String("imap.capabilities.blacklist");
const QString SettingsNames::composerSaveToImapKey = QLatin1String("composer/saveToImapEnabled");
const QString SettingsNames::composerImapSentKey = QLatin1String("composer/imapSentName");
const QString SettingsNames::cacheMetadataKey = QLatin1String("offline.metadataCache");
const QString SettingsNames::cacheMetadataMemory = QLatin1String("memory");
const QString SettingsNames::cacheOfflineKey = QLatin1String("offline.cache");
const QString SettingsNames::cacheOfflineNone = QLatin1String("memory");
const QString SettingsNames::cacheOfflineXDays = QLatin1String("days");
const QString SettingsNames::cacheOfflineAll = QLatin1String("all");
const QString SettingsNames::cacheOfflineNumberDaysKey = QLatin1String("offline.cache.numDays");
const QString SettingsNames::xtConnectCacheDirectory = QLatin1String("xtconnect.cachedir");
const QString SettingsNames::xtSyncMailboxList = QLatin1String("xtconnect.listOfMailboxes");
const QString SettingsNames::xtDbHost = QLatin1String("xtconnect.db.hostname");
const QString SettingsNames::xtDbPort = QLatin1String("xtconnect.db.port");
const QString SettingsNames::xtDbDbName = QLatin1String("xtconnect.db.dbname");
const QString SettingsNames::xtDbUser = QLatin1String("xtconnect.db.username");
const QString SettingsNames::guiMsgListShowThreading = QLatin1String("gui/msgList.showThreading");
const QString SettingsNames::guiMsgListHideRead = QLatin1String("gui/msgList.hideRead");
const QString SettingsNames::guiMailboxListShowOnlySubscribed = QLatin1String("gui/mailboxList.showOnlySubscribed");
const QString SettingsNames::guiMainWindowLayout = QLatin1String("gui/mainWindow.layout");
const QString SettingsNames::guiMainWindowLayoutCompact = QLatin1String("compact");
const QString SettingsNames::guiMainWindowLayoutWide = QLatin1String("wide");
const QString SettingsNames::guiMainWindowLayoutOneAtTime = QLatin1String("one-at-time");
const QString SettingsNames::guiPreferPlaintextRendering = QLatin1String("gui/preferPlaintextRendering");
const QString SettingsNames::guiShowSystray = QLatin1String("gui/showSystray");
const QString SettingsNames::guiOnSystrayClose = QLatin1String("gui/onSystrayClose");
const QString SettingsNames::guiSizesInMainWinWhenCompact = QLatin1String("gui/sizeInMainWinWhenCompact-%1");
const QString SettingsNames::guiSizesInMainWinWhenWide = QLatin1String("gui/sizeInMainWinWhenWide-%1");
const QString SettingsNames::guiAllowRawSearch = QLatin1String("gui/allowRawSearch");
const QString SettingsNames::appLoadHomepage = QLatin1String("app.updates.checkEnabled");
const QString SettingsNames::knownEmailsKey = QLatin1String("addressBook/knownEmails");
const QString SettingsNames::addressbookPlugin = QLatin1String("plugin/addressbook");
const QString SettingsNames::passwordPlugin = QLatin1String("plugin/password");

}
