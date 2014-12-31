/* Copyright (C) 2013 Pali Rohár <pali.rohar@gmail.com>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of Akonadi e.V. (or its successor approved
   by the membership of Akonadi e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QStringList>
#include <QProcess>

#include <Akonadi/Contact/ContactEditor>
#include <Akonadi/Contact/ContactEditorDialog>
#include <Akonadi/Contact/ContactSearchJob>
#include <KABC/Addressee>

#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PluginInterface.h"

#include "AkonadiAddressbook.h"

AkonadiAddressbookCompletionJob::AkonadiAddressbookCompletionJob(const QString &input, const QStringList &ignores, int max,
    QObject *parent) :
    AddressbookCompletionJob(parent), m_input(input), m_ignores(ignores), m_max(max), job(0)
{
}

void AkonadiAddressbookCompletionJob::doStart()
{
    if (job)
        return;
    job = new Akonadi::ContactSearchJob(this);
    if (m_max != -1)
        job->setLimit(m_max);
    job->setQuery(Akonadi::ContactSearchJob::NameOrEmail, m_input, Akonadi::ContactSearchJob::ContainsMatch);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(result()));
}

void AkonadiAddressbookCompletionJob::result()
{
    NameEmailList completion;

    const KABC::Addressee::List &list = job->contacts();
    Q_FOREACH(const KABC::Addressee &addr, list) {
        Q_FOREACH(const QString &email, addr.emails()) {
            if (!m_ignores.contains(email)) {
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

void AkonadiAddressbookCompletionJob::doStop()
{
    if (job) {
        disconnect(job, SIGNAL(result(KJob*)), this, SLOT(result()));
        job->kill();
        job = 0;
    }
    emit error(AddressbookJob::Stopped);
    finished();
}

AkonadiAddressbookNamesJob::AkonadiAddressbookNamesJob(const QString &email, QObject *parent) :
    AddressbookNamesJob(parent), m_email(email), job(0)
{
}

void AkonadiAddressbookNamesJob::doStart()
{
    if (job)
        return;
    job = new Akonadi::ContactSearchJob(this);
    job->setQuery(Akonadi::ContactSearchJob::Email, m_email, Akonadi::ContactSearchJob::ExactMatch);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(result()));
}

void AkonadiAddressbookNamesJob::result()
{
    QStringList displayNames;

    const KABC::Addressee::List &list = job->contacts();
    Q_FOREACH(const KABC::Addressee &addr, list)
        displayNames << addr.realName();

    emit prettyNamesForAddressAvailable(displayNames);
    finished();
}

void AkonadiAddressbookNamesJob::doStop()
{
    if (job) {
        disconnect(job, SIGNAL(result(KJob*)), this, SLOT(result()));
        job->kill();
        job = 0;
    }
    emit error(AddressbookJob::Stopped);
    finished();
}

AkonadiAddressbookContactWindow::AkonadiAddressbookContactWindow(const QString &email, const QString &displayName, QObject *parent) :
    QObject(parent), m_email(email), m_displayName(displayName), job(0)
{
}

void AkonadiAddressbookContactWindow::show()
{
    job = new Akonadi::ContactSearchJob(this);
    job->setLimit(1);
    job->setQuery(Akonadi::ContactSearchJob::Email, m_email, Akonadi::ContactSearchJob::ExactMatch);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(result()));
}

void AkonadiAddressbookContactWindow::result()
{
    Akonadi::ContactEditorDialog *dlg;

    if (!job->items().isEmpty()) {
        dlg = new Akonadi::ContactEditorDialog(Akonadi::ContactEditorDialog::EditMode);
        dlg->setContact(job->items().first());
    } else {
        KABC::Addressee addressee;
        addressee.insertEmail(m_email, true);
        addressee.setNameFromString(m_displayName);
        dlg = new Akonadi::ContactEditorDialog(Akonadi::ContactEditorDialog::CreateMode);
        dlg->editor()->setContactTemplate(addressee);
    }

    connect(dlg, SIGNAL(destroyed()), this, SLOT(deleteLater()));
    dlg->show();

    job = 0;
}


AkonadiAddressbook::AkonadiAddressbook(QObject *parent) : AddressbookPlugin(parent)
{
}

AddressbookPlugin::Features AkonadiAddressbook::features() const
{
    return FeatureAddressbookWindow | FeatureContactWindow | FeatureAddContact | FeatureEditContact | FeatureCompletion | FeaturePrettyNames;
}

AddressbookCompletionJob *AkonadiAddressbook::requestCompletion(const QString &input, const QStringList &ignores, int max)
{
    return new AkonadiAddressbookCompletionJob(input, ignores, max, this);
}

AddressbookNamesJob *AkonadiAddressbook::requestPrettyNamesForAddress(const QString &email)
{
    return new AkonadiAddressbookNamesJob(email, this);
}

void AkonadiAddressbook::openAddressbookWindow()
{
    QProcess::startDetached(QLatin1String("kaddressbook"));
}

void AkonadiAddressbook::openContactWindow(const QString &email, const QString &displayName)
{
    // AkonadiAddressbookContactWindow is deleted automatically
    (new AkonadiAddressbookContactWindow(email, displayName, this))->show();
}

QString trojita_plugin_AkonadiAddressbookPlugin::name() const
{
    return QLatin1String("akonadiaddressbook");
}

QString trojita_plugin_AkonadiAddressbookPlugin::description() const
{
    return tr("KDE Addressbook (akonadi)");
}

QObject *trojita_plugin_AkonadiAddressbookPlugin::create(QObject *parent, QSettings *)
{
    return new AkonadiAddressbook(parent);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(trojita_plugin_AkonadiAddressbookPlugin, trojita_plugin_AkonadiAddressbookPlugin)
#endif

// vim: set et ts=4 sts=4 sw=4
