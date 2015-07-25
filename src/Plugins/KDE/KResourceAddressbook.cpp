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

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QStringList>
#include <QPair>
#include <QProcess>
#include <QTimer>

#include <KABC/StdAddressBook>
#include <KABC/Addressee>

#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PluginInterface.h"

#include "KResourceAddressbook.h"

KResourceAddressbookCompletionJob::KResourceAddressbookCompletionJob(const QString &input, const QStringList &ignores, int max,
    KABC::AddressBook *ab, QObject *parent) :
    AddressbookCompletionJob(parent), m_input(input), m_ignores(ignores), m_max(max), m_ab(ab)
{
}

void KResourceAddressbookCompletionJob::doStart()
{
    NameEmailList completion;

    const KABC::Addressee::List &list = m_ab->allAddressees();
    Q_FOREACH(const KABC::Addressee &addr, list) {
        Q_FOREACH(const QString &email, addr.emails()) {
            if (!m_ignores.contains(email) && (email.contains(m_input, Qt::CaseInsensitive) ||
                    addr.realName().contains(m_input, Qt::CaseInsensitive))) {
                completion << NameEmail(addr.realName(), email);
                if (m_max != -1 && completion.size() >= m_max)
                    break;
            }
        }
        if (m_max != -1 && completion.size() >= m_max)
            break;
    }

    emit completionAvailable(completion);
    finished();
}

void KResourceAddressbookCompletionJob::doStop()
{
    emit error(AddressbookJob::Stopped);
    finished();
}

KResourceAddressbookNamesJob::KResourceAddressbookNamesJob(const QString &email, KABC::AddressBook *ab, QObject *parent) :
    AddressbookNamesJob(parent), m_email(email), m_ab(ab)
{
}

void KResourceAddressbookNamesJob::doStart()
{
    QStringList displayNames;

    const KABC::Addressee::List &list = m_ab->findByEmail(m_email);
    Q_FOREACH(const KABC::Addressee &addr, list)
        displayNames << addr.realName();

    emit prettyNamesForAddressAvailable(displayNames);
    finished();
}

void KResourceAddressbookNamesJob::doStop()
{
    emit error(AddressbookJob::Stopped);
    finished();
}

KResourceAddressbookProcess::KResourceAddressbookProcess(const QString &email, const QString &displayName, KABC::AddressBook *ab, QObject *parent) :
    QObject(parent), m_email(email), m_displayName(displayName), m_ab(ab), m_timer(0)
{
}

KResourceAddressbookProcess::~KResourceAddressbookProcess()
{
    if (m_timer) {
        disconnect(m_timer, 0, 0, 0);
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = 0;
    }
}

void KResourceAddressbookProcess::start()
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);

    connect(m_timer, SIGNAL(timeout()), this, SLOT(deleteLater()));

    // make sure that object finish work in 60s
    m_timer->start(60000);

    QDBusInterface iface(QLatin1String("org.freedesktop.DBus"), QLatin1String("/"), QLatin1String("org.freedesktop.DBus"));

    QDBusPendingCall call = iface.asyncCall(QLatin1String("ListNames"));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);

    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(slotDBusListFinished()));
}

void KResourceAddressbookProcess::slotDBusListFinished()
{
    QDBusPendingCallWatcher *watcher = static_cast<QDBusPendingCallWatcher *>(sender());
    QDBusPendingReply<QStringList> reply = *watcher;
    QStringList list = reply;

    watcher->deleteLater();

    // kontact already running
    if (list.contains(QLatin1String("org.kde.kontact"))) {
        slotKontactRunning();
        return;
    }

    // kontact not running, but kaddressbook yes
    if (list.contains(QLatin1String("org.kde.kaddressbook"))) {
        slotKaddressbookRunning();
        return;
    }

    QProcess *process = new QProcess(this);
    process->closeReadChannel(QProcess::StandardOutput);
    process->closeReadChannel(QProcess::StandardError);
    process->closeWriteChannel();

    connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(slotKaddressbookRunning()));
    connect(process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(slotKaddressbookRunning()));

    // after kaddressbook initialize dbus it forks to backgroud
    process->start(QLatin1String("kaddressbook"));
}

void KResourceAddressbookProcess::slotKontactRunning()
{
    QDBusInterface iface(QLatin1String("org.kde.kaddressbook"), QLatin1String("/kaddressbook_PimApplication"), QLatin1String("org.kde.KUniqueApplication"));

    // Try to load kontact kaddressbook application module
    QDBusPendingCall call = iface.asyncCall(QLatin1String("load"));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);

    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(slotKaddressbookRunning()));
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), watcher, SLOT(deleteLater()));
}

void KResourceAddressbookProcess::slotKaddressbookRunning()
{
    QDBusInterface iface(QLatin1String("org.kde.kaddressbook"), QLatin1String("/KAddressBook"), QLatin1String("org.kde.KAddressbook.Core"));

    if (!iface.isValid()) {
        deleteLater();
        return;
    }

    // new contact: qdbus org.kde.kaddressbook /KAddressBook org.kde.KAddressbook.Core.newContact
    if (m_email.isEmpty() && m_displayName.isEmpty()) {
        iface.call(QDBus::NoBlock, QLatin1String("newContact"));
        deleteLater();
        return;
    }

    QString uid;
    KABC::Addressee::List list;

    if (!m_email.isEmpty() && !m_displayName.isEmpty()) {
        KABC::Addressee::List listNames = m_ab->findByName(m_displayName);
        Q_FOREACH(const KABC::Addressee &addr, listNames)
            if (addr.emails().contains(m_email))
                list << addr;
    } else if (!m_email.isEmpty()) {
        list = m_ab->findByEmail(m_email);
    } else if (!m_displayName.isEmpty()) {
        list = m_ab->findByName(m_displayName);
    }

    if (list.size() > 0)
        uid = list.first().uid(); //TODO: open GUI dialog for selecting contact, now using first

    // edit contact: qdbus org.kde.kaddressbook /KAddressBook org.kde.KAddressbook.Core.showContactEditor uid
    if (!uid.isEmpty()) {
        iface.call(QDBus::NoBlock, QLatin1String("showContactEditor"), uid);
        deleteLater();
        return;
    }

    QString str;
    if (!m_email.isEmpty() && !m_displayName.isEmpty())
        str = m_displayName + QLatin1String(" <") + m_email + QLatin1Char('>');
    else if (!m_email.isEmpty())
        str = m_email;
    else if (!m_displayName.isEmpty())
        str = m_displayName + QLatin1String(" <@>"); // Without "<@>" kaddressbook parse displayName as email address

    // new contact with displayName and email: qdbus org.kde.kaddressbook /KAddressBook org.kde.KAddressbook.Core.addEmail 'displayName <email>'
    iface.call(QDBus::NoBlock, QLatin1String("addEmail"), str);
    deleteLater();
}

KResourceAddressbook::KResourceAddressbook(QObject *parent) : AddressbookPlugin(parent)
{
    m_ab = KABC::StdAddressBook::self(/*async*/ true);
}

AddressbookPlugin::Features KResourceAddressbook::features() const
{
    return FeatureAddressbookWindow | FeatureContactWindow | FeatureAddContact | FeatureEditContact | FeatureCompletion | FeaturePrettyNames;
}

AddressbookCompletionJob *KResourceAddressbook::requestCompletion(const QString &input, const QStringList &ignores, int max)
{
    return new KResourceAddressbookCompletionJob(input, ignores, max, m_ab, this);
}

AddressbookNamesJob *KResourceAddressbook::requestPrettyNamesForAddress(const QString &email)
{
    return new KResourceAddressbookNamesJob(email, m_ab, this);
}

void KResourceAddressbook::openAddressbookWindow()
{
    QProcess::startDetached(QLatin1String("kaddressbook"));
}

void KResourceAddressbook::openContactWindow(const QString &email, const QString &displayName)
{
    // KResourceAddressbookProcess is deleted automatically
    (new KResourceAddressbookProcess(email, displayName, m_ab, this))->start();
}

QString trojita_plugin_KResourceAddressbookPlugin::name() const
{
    return QLatin1String("kresourceaddressbook");
}

QString trojita_plugin_KResourceAddressbookPlugin::description() const
{
    return tr("KDE Addressbook (legacy)");
}

QObject *trojita_plugin_KResourceAddressbookPlugin::create(QObject *parent, QSettings *)
{
    return new KResourceAddressbook(parent);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(trojita_plugin_KResourceAddressbookPlugin, trojita_plugin_KResourceAddressbookPlugin)
#endif

// vim: set et ts=4 sts=4 sw=4
