/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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
