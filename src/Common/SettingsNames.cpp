/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2014        Luke Dashjr <luke+trojita@dashjr.org>

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

const QString SettingsNames::identitiesKey = QStringLiteral("identities");
const QString SettingsNames::realNameKey = QStringLiteral("realName");
const QString SettingsNames::addressKey = QStringLiteral("address");
const QString SettingsNames::organisationKey = QStringLiteral("organisation");
const QString SettingsNames::signatureKey = QStringLiteral("signature");
const QString SettingsNames::obsRealNameKey = QStringLiteral("identity.realName");
const QString SettingsNames::obsAddressKey = QStringLiteral("identity.address");
const QString SettingsNames::msaMethodKey = QStringLiteral("msa.method");
const QString SettingsNames::methodSMTP = QStringLiteral("SMTP");
const QString SettingsNames::methodSSMTP = QStringLiteral("SSMTP");
const QString SettingsNames::methodSENDMAIL = QStringLiteral("sendmail");
const QString SettingsNames::methodImapSendmail = QStringLiteral("IMAP-SENDMAIL");
const QString SettingsNames::smtpHostKey = QStringLiteral("msa.smtp.host");
const QString SettingsNames::smtpPortKey = QStringLiteral("msa.smtp.port");
const QString SettingsNames::smtpAuthKey = QStringLiteral("msa.smtp.auth");
const QString SettingsNames::smtpStartTlsKey = QStringLiteral("msa.smtp.starttls");
const QString SettingsNames::smtpUserKey = QStringLiteral("msa.smtp.auth.user");
const QString SettingsNames::smtpAuthReuseImapCredsKey = QStringLiteral("msa.smtp.auth.reuseImapCredentials");
// in use by the cleartext password plugin: "msa.smtp.auth.pass"
const QString SettingsNames::sendmailKey = QStringLiteral("msa.sendmail");
const QString SettingsNames::sendmailDefaultCmd = QStringLiteral("sendmail -bm -oi");
const QString SettingsNames::smtpUseBurlKey = QStringLiteral("msa.smtp.burl");
const QString SettingsNames::imapMethodKey = QStringLiteral("imap.method");
const QString SettingsNames::methodTCP = QStringLiteral("TCP");
const QString SettingsNames::methodSSL = QStringLiteral("SSL");
const QString SettingsNames::methodProcess = QStringLiteral("process");
const QString SettingsNames::imapHostKey = QStringLiteral("imap.host");
const QString SettingsNames::imapPortKey = QStringLiteral("imap.port");
const QString SettingsNames::imapStartTlsKey = QStringLiteral("imap.starttls");
const QString SettingsNames::imapUserKey = QStringLiteral("imap.auth.user");
// in use by the cleartext password plugin: "imap.auth.pass"
const QString SettingsNames::imapProcessKey = QStringLiteral("imap.process");
const QString SettingsNames::imapStartMode = QStringLiteral("imap.startmode");
const QString SettingsNames::netOffline = QStringLiteral("OFFLINE");
const QString SettingsNames::netExpensive = QStringLiteral("EXPENSIVE");
const QString SettingsNames::netOnline = QStringLiteral("ONLINE");
const QString SettingsNames::obsImapStartOffline = QStringLiteral("imap.offline");
const QString SettingsNames::obsImapSslPemCertificate = QStringLiteral("imap.ssl.pemCertificate");
const QString SettingsNames::imapSslPemPubKey = QStringLiteral("imap.ssl.pemPubKey");
const QString SettingsNames::imapBlacklistedCapabilities = QStringLiteral("imap.capabilities.blacklist");
const QString SettingsNames::imapUseSystemProxy = QStringLiteral("imap.proxy.system");
const QString SettingsNames::imapNeedsNetwork = QStringLiteral("imap.needsNetwork");
const QString SettingsNames::imapNumberRefreshInterval = QStringLiteral("imap.numberRefreshInterval");
const QString SettingsNames::composerSaveToImapKey = QStringLiteral("composer/saveToImapEnabled");
const QString SettingsNames::composerImapSentKey = QStringLiteral("composer/imapSentName");
const QString SettingsNames::cacheMetadataKey = QStringLiteral("offline.metadataCache");
const QString SettingsNames::cacheMetadataMemory = QStringLiteral("memory");
const QString SettingsNames::cacheOfflineKey = QStringLiteral("offline.cache");
const QString SettingsNames::cacheOfflineNone = QStringLiteral("memory");
const QString SettingsNames::cacheOfflineXDays = QStringLiteral("days");
const QString SettingsNames::cacheOfflineAll = QStringLiteral("all");
const QString SettingsNames::cacheOfflineNumberDaysKey = QStringLiteral("offline.cache.numDays");
const QString SettingsNames::xtConnectCacheDirectory = QStringLiteral("xtconnect.cachedir");
const QString SettingsNames::xtSyncMailboxList = QStringLiteral("xtconnect.listOfMailboxes");
const QString SettingsNames::xtDbHost = QStringLiteral("xtconnect.db.hostname");
const QString SettingsNames::xtDbPort = QStringLiteral("xtconnect.db.port");
const QString SettingsNames::xtDbDbName = QStringLiteral("xtconnect.db.dbname");
const QString SettingsNames::xtDbUser = QStringLiteral("xtconnect.db.username");
const QString SettingsNames::guiMsgListShowThreading = QStringLiteral("gui/msgList.showThreading");
const QString SettingsNames::guiMsgListHideRead = QStringLiteral("gui/msgList.hideRead");
const QString SettingsNames::guiMailboxListShowOnlySubscribed = QStringLiteral("gui/mailboxList.showOnlySubscribed");
const QString SettingsNames::guiMainWindowLayout = QStringLiteral("gui/mainWindow.layout");
const QString SettingsNames::guiMainWindowLayoutCompact = QStringLiteral("compact");
const QString SettingsNames::guiMainWindowLayoutWide = QStringLiteral("wide");
const QString SettingsNames::guiMainWindowLayoutOneAtTime = QStringLiteral("one-at-time");
const QString SettingsNames::guiPreferPlaintextRendering = QStringLiteral("gui/preferPlaintextRendering");
const QString SettingsNames::guiShowSystray = QStringLiteral("gui/showSystray");
const QString SettingsNames::guiOnSystrayClose = QStringLiteral("gui/onSystrayClose");
const QString SettingsNames::guiStartMinimized = QStringLiteral("gui/startMinimized");
const QString SettingsNames::guiSizesInMainWinWhenCompact = QStringLiteral("gui/sizeInMainWinWhenCompact-%1");
const QString SettingsNames::guiSizesInMainWinWhenWide = QStringLiteral("gui/sizeInMainWinWhenWide-%1");
const QString SettingsNames::guiSizesInaMainWinWhenOneAtATime = QStringLiteral("gui/sizeInMainWinWhenOneAtATime-%1");
const QString SettingsNames::guiAllowRawSearch = QStringLiteral("gui/allowRawSearch");
const QString SettingsNames::guiExpandedMailboxes = QStringLiteral("gui/expandedMailboxes");
const QString SettingsNames::appLoadHomepage = QStringLiteral("app.updates.checkEnabled");
const QString SettingsNames::knownEmailsKey = QStringLiteral("addressBook/knownEmails");
const QString SettingsNames::addressbookPlugin = QStringLiteral("plugin/addressbook");
const QString SettingsNames::passwordPlugin = QStringLiteral("plugin/password");
const QString SettingsNames::imapIdleRenewal = QStringLiteral("imapIdleRenewal");
const QString SettingsNames::autoMarkReadEnabled = QStringLiteral("autoMarkRead/enabled");
const QString SettingsNames::autoMarkReadSeconds = QStringLiteral("autoMarkRead/seconds");
const QString SettingsNames::interopRevealVersions = QStringLiteral("interoperability/revealVersions");

}
