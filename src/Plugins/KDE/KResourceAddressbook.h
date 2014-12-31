/* Copyright (C) 2013 Pali Rohár <pali.rohar@gmail.com>

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

#ifndef KDEADDRESSBOOK_H
#define KDEADDRESSBOOK_H

#include <QStringList>

#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PluginInterface.h"

namespace KABC {
    class AddressBook;
};

class QTimer;

using namespace Plugins;

class KResourceAddressbookCompletionJob : public AddressbookCompletionJob
{
    Q_OBJECT

public:
    KResourceAddressbookCompletionJob(const QString &input, const QStringList &ignores, int max, KABC::AddressBook *ab, QObject *parent);

public slots:
    virtual void doStart();
    virtual void doStop();

private:
    QString m_input;
    QStringList m_ignores;
    int m_max;
    KABC::AddressBook *m_ab;
};

class KResourceAddressbookNamesJob : public AddressbookNamesJob
{
    Q_OBJECT

public:
    KResourceAddressbookNamesJob(const QString &email, KABC::AddressBook *ab, QObject *parent);

public slots:
    virtual void doStart();
    virtual void doStop();

private:
    QString m_email;
    KABC::AddressBook *m_ab;
};

class KResourceAddressbookProcess : public QObject
{
    Q_OBJECT

public:
    KResourceAddressbookProcess(const QString &email, const QString &displayName, KABC::AddressBook *ab, QObject *parent);
    ~KResourceAddressbookProcess();

public slots:
    void start();

private slots:
    void slotDBusListFinished();
    void slotKontactRunning();
    void slotKaddressbookRunning();

private:
    QString m_email;
    QString m_displayName;
    KABC::AddressBook *m_ab;
    QTimer *m_timer;
};

class KResourceAddressbook : public AddressbookPlugin
{
    Q_OBJECT

public:
    KResourceAddressbook(QObject *parent);
    virtual Features features() const;

public slots:
    virtual AddressbookCompletionJob *requestCompletion(const QString &input, const QStringList &ignores = QStringList(), int max = -1);
    virtual AddressbookNamesJob *requestPrettyNamesForAddress(const QString &email);
    virtual void openAddressbookWindow();
    virtual void openContactWindow(const QString &email, const QString &displayName);

private:
    KABC::AddressBook *m_ab;
};

class trojita_plugin_KResourceAddressbookPlugin : public QObject, public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(Plugins::PluginInterface)
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID "net.flaska.trojita.plugins.addressbook.kresource")
#endif

public:
    virtual QString name() const;
    virtual QString description() const;
    virtual QObject *create(QObject *parent, QSettings *settings);
};

#endif //KDEADDRESSBOOK_H

// vim: set et ts=4 sts=4 sw=4
