#include "SettingsNames.h"

namespace Gui {

QString SettingsNames::realNameKey = QLatin1String("identity.realName");
QString SettingsNames::addressKey = QLatin1String("identity.address");
QString SettingsNames::msaMethodKey = QLatin1String("msa.method");
QString SettingsNames::methodSMTP = QLatin1String("SMTP");
QString SettingsNames::methodSENDMAIL = QLatin1String("sendmail");
QString SettingsNames::smtpHostKey = QLatin1String("msa.smtp.host");
QString SettingsNames::smtpPortKey = QLatin1String("msa.smtp.port");
QString SettingsNames::smtpAuthKey = QLatin1String("msa.smtp.auth");
QString SettingsNames::smtpUserKey = QLatin1String("msa.smtp.auth.user");
QString SettingsNames::smtpPassKey = QLatin1String("msa.smtp.auth.pass");
QString SettingsNames::sendmailKey = QLatin1String("msa.sendmail");
QString SettingsNames::sendmailDefaultCmd = QLatin1String("sendmail -bm -oi");
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

}
