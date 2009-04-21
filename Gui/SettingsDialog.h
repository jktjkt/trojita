#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QTabWidget;

namespace Gui {

class IdentityPage : public QWidget
{
    Q_OBJECT
public:
    IdentityPage( QWidget* parent );
    void save();
private:
    QLineEdit* realName;
    QLineEdit* address;
    QString  realNameKey, addressKey;
};

class OutgoingPage : public QWidget
{
    Q_OBJECT
public:
    OutgoingPage( QWidget* parent );
    void save();
private:
};

class ImapPage : public QWidget
{
    Q_OBJECT
public:
    ImapPage( QWidget* parent );
    void save();
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
