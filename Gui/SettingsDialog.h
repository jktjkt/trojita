#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

class QCheckBox;
class QComboBox;
class QTabWidget;

namespace Gui {

class IdentityPage : public QWidget
{
    Q_OBJECT
public:
    IdentityPage( QWidget* parent, QSettings&s );
    void save( QSettings& s );
private:
    QLineEdit* realName;
    QLineEdit* address;
    QString  realNameKey, addressKey;
};

class OutgoingPage : public QWidget
{
    Q_OBJECT
public:
    OutgoingPage( QWidget* parent, QSettings& s );
    void save( QSettings& s );
private:
    typedef enum { SMTP, SENDMAIL } Kind;
    QComboBox* method;

    QLineEdit* smtpHost;
    QLineEdit* smtpPort;
    QCheckBox* smtpAuth;
    QLineEdit* smtpUser;
    QLineEdit* smtpPass;

    QLineEdit* sendmail;

    QString methodKey, methodSMTP, methodSENDMAIL, smtpHostKey, smtpPortKey,
            smtpAuthKey, smtpUserKey, smtpPassKey, sendmailKey;
private slots:
    void updateWidgets();
};

class ImapPage : public QWidget
{
    Q_OBJECT
public:
    ImapPage( QWidget* parent, QSettings& s );
    void save( QSettings& s );
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog();
public slots:
    void accept();
private:
    QTabWidget* stack;
    IdentityPage* identity;
    ImapPage* imap;
    OutgoingPage* outgoing;
};

}

#endif // SETTINGSDIALOG_H
