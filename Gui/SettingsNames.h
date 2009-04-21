#ifndef SETTINGSNAMES_H
#define SETTINGSNAMES_H

#include <QString>

namespace Gui {

struct SettingsNames
{
    static QString  realNameKey, addressKey;
    static QString methodKey, methodSMTP, methodSENDMAIL, smtpHostKey, smtpPortKey,
            smtpAuthKey, smtpUserKey, smtpPassKey, sendmailKey;
};

}

#endif // SETTINGSNAMES_H
