/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QPointer>
#include <QSettings>
#include "ui_SettingsImapPage.h"
#include "ui_SettingsCachePage.h"
#include "ui_SettingsOutgoingPage.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QTabWidget;

namespace Gui
{

class GeneralPage : public QWidget
{
    Q_OBJECT
public:
    GeneralPage(QWidget *parent, QSettings &s);
    void save(QSettings &s);
private:
    QLineEdit *realName;
    QLineEdit *address;
    QCheckBox *showHomepage;

    GeneralPage(const GeneralPage &); // don't implement
    GeneralPage &operator=(const GeneralPage &); // don't implement
};

class OutgoingPage : public QScrollArea, Ui_OutgoingPage
{
    Q_OBJECT
public:
    OutgoingPage(QWidget *parent, QSettings &s);
    void save(QSettings &s);

protected:
    virtual void resizeEvent(QResizeEvent *event);

private:
    enum { SMTP, SSMTP, SENDMAIL, IMAP_SENDMAIL };

private slots:
    void updateWidgets();

private:
    OutgoingPage(const OutgoingPage &); // don't implement
    OutgoingPage &operator=(const OutgoingPage &); // don't implement
};

class ImapPage : public QScrollArea, Ui_ImapPage
{
    Q_OBJECT
public:
    ImapPage(QWidget *parent, QSettings &s);
    void save(QSettings &s);
#ifdef XTUPLE_CONNECT
    bool hasPassword() const;
#endif

protected:
    virtual void resizeEvent(QResizeEvent *event);

private:
    enum { TCP, SSL, PROCESS };

private slots:
    void updateWidgets();
    void maybeShowPasswordWarning();

private:
    ImapPage(const ImapPage &); // don't implement
    ImapPage &operator=(const ImapPage &); // don't implement
};

class CachePage : public QScrollArea, Ui_CachePage
{
    Q_OBJECT
public:
    CachePage(QWidget *parent, QSettings &s);
    void save(QSettings &s);

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
    SettingsDialog();

    static QString warningStyleSheet;
public slots:
    void accept();
private:
    QTabWidget *stack;
    GeneralPage *general;
    ImapPage *imap;
    CachePage *cache;
    OutgoingPage *outgoing;
#ifdef XTUPLE_CONNECT
    XtConnectPage *xtConnect;
#endif

    SettingsDialog(const SettingsDialog &); // don't implement
    SettingsDialog &operator=(const SettingsDialog &); // don't implement
};

}

#endif // SETTINGSDIALOG_H
