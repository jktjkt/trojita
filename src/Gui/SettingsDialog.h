/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2012        Mohammed Nafees <nafees.technocool@gmail.com>

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
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QPointer>
#include <QSettings>
#include "Plugins/PasswordPlugin.h"
#include "ui_SettingsGeneralPage.h"
#include "ui_EditIdentity.h"
#include "ui_SettingsImapPage.h"
#include "ui_SettingsCachePage.h"
#include "ui_SettingsOutgoingPage.h"

class QCheckBox;
class QComboBox;
class QDataWidgetMapper;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QTabWidget;

namespace Composer
{
class SenderIdentitiesModel;
}

namespace Plugins
{
class PluginManager;
}

namespace UiUtils {
class PasswordWatcher;
}

namespace Imap {
class ImapAccess;
}

namespace MSA {
class Account;
}

namespace Gui
{
class MainWindow;
class SettingsDialog;

/** @short Common interface for any page of the settings dialogue */
class ConfigurationWidgetInterface {
public:
    virtual void save(QSettings &s) = 0;
    virtual QWidget *asWidget() = 0;
    virtual bool checkValidity() const = 0;
    virtual bool passwordFailures(QString &message) const = 0;
};


class GeneralPage : public QScrollArea, Ui_GeneralPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    GeneralPage(SettingsDialog *parent, QSettings &s, Composer::SenderIdentitiesModel *identitiesModel);
    virtual void save(QSettings &s);
    virtual QWidget *asWidget();
    virtual bool checkValidity() const;
    virtual bool passwordFailures(QString &message) const;

private slots:
    void updateWidgets();
    void moveIdentityUp();
    void moveIdentityDown();
    void addButtonClicked();
    void editButtonClicked();
    void deleteButtonClicked();
    void passwordPluginChanged();

private:
    Composer::SenderIdentitiesModel *m_identitiesModel;
    SettingsDialog *m_parent;

    GeneralPage(const GeneralPage &); // don't implement
    GeneralPage &operator=(const GeneralPage &); // don't implement

signals:
    void saved();
    void reloadPasswords();
    void widgetsUpdated();
};

class EditIdentity : public QDialog, Ui_EditIdentity
{
    Q_OBJECT
public:
    EditIdentity(QWidget *parent, Composer::SenderIdentitiesModel *identitiesModel, const QModelIndex &currentIndex);
    void setDeleteOnReject(const bool reject = true);

public slots:
    void enableButton();
    void onReject();

private:
    Composer::SenderIdentitiesModel *m_identitiesModel;
    QDataWidgetMapper *m_mapper;
    bool m_deleteOnReject;

    EditIdentity(const EditIdentity &); // don't implement
    EditIdentity &operator=(const EditIdentity &); // don't implement

signals:
    void saved();
    void widgetsUpdated();
};


class OutgoingPage : public QScrollArea, Ui_OutgoingPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    OutgoingPage(SettingsDialog *parent, QSettings &s);
    virtual void save(QSettings &s);
    virtual QWidget *asWidget();
    virtual bool checkValidity() const;
    virtual bool passwordFailures(QString &message) const;

private slots:
    void slotSetSubmissionMethod();
    void updateWidgets();
    void slotSetPassword();
    void showPortWarning(const QString &warning);
    void setPortByText(const QString &text);

private:

    enum { NETWORK, SENDMAIL, IMAP_SENDMAIL };
    enum Encryption { SMTP, SMTP_STARTTLS, SSMTP };

    SettingsDialog *m_parent;
    UiUtils::PasswordWatcher *m_pwWatcher;

    MSA::Account *m_smtpAccountSettings;

    OutgoingPage(const OutgoingPage &); // don't implement
    OutgoingPage &operator=(const OutgoingPage &); // don't implement

signals:
    void saved();
    void widgetsUpdated();
};

class ImapPage : public QScrollArea, Ui_ImapPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    ImapPage(SettingsDialog *parent, QSettings &s);
    virtual void save(QSettings &s);
    virtual QWidget *asWidget();
    virtual bool checkValidity() const;
    virtual bool passwordFailures(QString &message) const;
#ifdef XTUPLE_CONNECT
    bool hasPassword() const;
#endif

private:
    enum { NETWORK, PROCESS };
    enum Encryption { NONE, STARTTLS, SSL };
    SettingsDialog *m_parent;
    quint16 m_imapPort;
    bool m_imapStartTls;
    UiUtils::PasswordWatcher *m_pwWatcher;

private slots:
    void updateWidgets();
    void maybeShowPortWarning();
    void changePort();
    void slotSetPassword();

private:
    ImapPage(const ImapPage &); // don't implement
    ImapPage &operator=(const ImapPage &); // don't implement

signals:
    void saved();
    void widgetsUpdated();
};

class CachePage : public QScrollArea, Ui_CachePage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    CachePage(QWidget *parent, QSettings &s);
    virtual void save(QSettings &s);
    virtual QWidget *asWidget();
    virtual bool checkValidity() const;
    virtual bool passwordFailures(QString &message) const;

private:
    QCheckBox *startOffline;

private slots:
    void updateWidgets();

private:
    CachePage(const CachePage &); // don't implement
    CachePage &operator=(const CachePage &); // don't implement

signals:
    void saved();
    void widgetsUpdated();
};

#ifdef XTUPLE_CONNECT
class SettingsDialog;
class XtConnectPage : public QWidget
{
    Q_OBJECT
public:
    XtConnectPage(QWidget *parent, QSettings &s, ImapPage *imapPage);
    void save(QSettings &s);
    virtual void showEvent(QShowEvent *event);
public slots:
    void saveXtConfig();
    void runXtConnect();
private:
    QLineEdit *cacheDir;
    QPointer<ImapPage> imap;

    QLineEdit *hostName;
    QSpinBox *port;
    QLineEdit *dbName;
    QLineEdit *username;
    QLabel *imapPasswordWarning;
    QCheckBox *debugLog;

    XtConnectPage(const XtConnectPage &); // don't implement
    XtConnectPage &operator=(const XtConnectPage &); // don't implement
signals:
    void saved();
    void widgetsUpdated();
};
#endif


class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(MainWindow *parent, Composer::SenderIdentitiesModel *identitiesModel, QSettings *settings);

    Plugins::PluginManager *pluginManager();
    Imap::ImapAccess* imapAccess();

    void setOriginalPasswordPlugin(const QString &plugin);

    static QString warningStyleSheet;

signals:
    void reloadPasswordsRequested();

public slots:
    void accept();
    void reject();
private slots:
    void adjustSizeToScrollAreas();
    void slotAccept();
private:
    MainWindow *mainWindow;
    QDialogButtonBox *buttons;
    QTabWidget *stack;
    QVector<ConfigurationWidgetInterface*> pages;
#ifdef XTUPLE_CONNECT
    XtConnectPage *xtConnect;
#endif
    Composer::SenderIdentitiesModel *m_senderIdentities;
    QSettings *m_settings;
    int m_saveSignalCount;
    QString m_originalPasswordPlugin;

    SettingsDialog(const SettingsDialog &); // don't implement
    SettingsDialog &operator=(const SettingsDialog &); // don't implement

    void addPage(ConfigurationWidgetInterface *page, const QString &title);
};

}

#endif // SETTINGSDIALOG_H
