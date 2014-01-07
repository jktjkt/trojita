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

namespace Gui
{

/** @short Common interface for any page of the settings dialogue */
class ConfigurationWidgetInterface {
public:
    virtual void save(QSettings &s) = 0;
    virtual QWidget *asWidget() = 0;
    virtual bool checkValidity() const = 0;
};


class GeneralPage : public QScrollArea, Ui_GeneralPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    GeneralPage(QWidget *parent, QSettings &s, Composer::SenderIdentitiesModel *identitiesModel);
    virtual void save(QSettings &s);
    virtual QWidget *asWidget();
    virtual bool checkValidity() const;

private slots:
    void updateWidgets();
    void moveIdentityUp();
    void moveIdentityDown();
    void addButtonClicked();
    void editButtonClicked();
    void deleteButtonClicked();

private:
    Composer::SenderIdentitiesModel *m_identitiesModel;

    GeneralPage(const GeneralPage &); // don't implement
    GeneralPage &operator=(const GeneralPage &); // don't implement
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
};


class OutgoingPage : public QScrollArea, Ui_OutgoingPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    OutgoingPage(QWidget *parent, QSettings &s);
    virtual void save(QSettings &s);
    virtual QWidget *asWidget();
    virtual bool checkValidity() const;

protected:
    virtual void resizeEvent(QResizeEvent *event);

private:
    enum { SMTP, SSMTP, SENDMAIL, IMAP_SENDMAIL };

private slots:
    void updateWidgets();
    void maybeShowPasswordWarning();

private:
    OutgoingPage(const OutgoingPage &); // don't implement
    OutgoingPage &operator=(const OutgoingPage &); // don't implement
};

class ImapPage : public QScrollArea, Ui_ImapPage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    ImapPage(QWidget *parent, QSettings &s);
    virtual void save(QSettings &s);
    virtual QWidget *asWidget();
    virtual bool checkValidity() const;
#ifdef XTUPLE_CONNECT
    bool hasPassword() const;
#endif

protected:
    virtual void resizeEvent(QResizeEvent *event);

private:
    enum { NETWORK, PROCESS };
    enum Encryption { NONE, STARTTLS, SSL };
    quint16 m_imapPort;
    bool m_imapStartTls;

private slots:
    void updateWidgets();
    void maybeShowPasswordWarning();
    void maybeShowPortWarning();
    void changePort();

private:
    ImapPage(const ImapPage &); // don't implement
    ImapPage &operator=(const ImapPage &); // don't implement
};

class CachePage : public QScrollArea, Ui_CachePage, public ConfigurationWidgetInterface
{
    Q_OBJECT
public:
    CachePage(QWidget *parent, QSettings &s);
    virtual void save(QSettings &s);
    virtual QWidget *asWidget();
    virtual bool checkValidity() const;

protected:
    virtual void resizeEvent(QResizeEvent *event);

private:
    QCheckBox *startOffline;

private slots:
    void updateWidgets();

private:
    CachePage(const CachePage &); // don't implement
    CachePage &operator=(const CachePage &); // don't implement
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
};
#endif


class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(QWidget *parent, Composer::SenderIdentitiesModel *identitiesModel, QSettings *settings);

    static QString warningStyleSheet;
public slots:
    void accept();
    void reject();
private:
    QTabWidget *stack;
    QVector<ConfigurationWidgetInterface*> pages;
#ifdef XTUPLE_CONNECT
    XtConnectPage *xtConnect;
#endif
    Composer::SenderIdentitiesModel *m_senderIdentities;
    QSettings *m_settings;

    SettingsDialog(const SettingsDialog &); // don't implement
    SettingsDialog &operator=(const SettingsDialog &); // don't implement

    void addPage(ConfigurationWidgetInterface *page, const QString &title);
};

}

#endif // SETTINGSDIALOG_H
