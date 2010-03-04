#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

class QCheckBox;
class QComboBox;
class QLineEdit;
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

    IdentityPage(const IdentityPage&); // don't implement
    IdentityPage& operator=(const IdentityPage&); // don't implement
};

class OutgoingPage : public QWidget
{
    Q_OBJECT
public:
    OutgoingPage( QWidget* parent, QSettings& s );
    void save( QSettings& s );
private:
    enum { SMTP, SENDMAIL };
    QComboBox* method;

    QLineEdit* smtpHost;
    QLineEdit* smtpPort;
    QCheckBox* smtpAuth;
    QLineEdit* smtpUser;
    QLineEdit* smtpPass;

    QLineEdit* sendmail;
private slots:
    void updateWidgets();

private:
    OutgoingPage(const OutgoingPage&); // don't implement
    OutgoingPage& operator=(const OutgoingPage&); // don't implement
};

class ImapPage : public QWidget
{
    Q_OBJECT
public:
    ImapPage( QWidget* parent, QSettings& s );
    void save( QSettings& s );
private:
    enum { TCP, SSL, PROCESS };
    QComboBox* method;

    QLineEdit* imapHost;
    QLineEdit* imapPort;
    QCheckBox* startTls;
    QLineEdit* imapUser;
    QLineEdit* imapPass;

    QLineEdit* processPath;

    QCheckBox* startOffline;
private slots:
    void updateWidgets();

private:
    ImapPage(const ImapPage&); // don't implement
    ImapPage& operator=(const ImapPage&); // don't implement
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

    SettingsDialog(const SettingsDialog&); // don't implement
    SettingsDialog& operator=(const SettingsDialog&); // don't implement
};

}

#endif // SETTINGSDIALOG_H
