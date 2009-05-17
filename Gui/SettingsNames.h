#ifndef SETTINGSNAMES_H
#define SETTINGSNAMES_H

#include <QString>

namespace Gui {

struct SettingsNames
{
    static QString realNameKey, addressKey;
    static QString msaMethodKey, methodSMTP, methodSENDMAIL, smtpHostKey, smtpPortKey,
            smtpAuthKey, smtpUserKey, smtpPassKey, sendmailKey, sendmailDefaultCmd;
    static QString imapMethodKey, methodTCP, methodSSL, methodProcess, imapHostKey,
            imapPortKey, imapStartTlsKey, imapUserKey, imapPassKey, imapProcessKey,
            imapStartOffline;
};

}

#endif // SETTINGSNAMES_H
