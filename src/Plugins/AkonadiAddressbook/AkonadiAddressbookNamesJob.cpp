/* Copyright (C) 2017 Roland Pallai <dap78@magex.hu>
   Copyright (C) 2013 Pali Rohár <pali.rohar@gmail.com>

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

#include "AkonadiAddressbookNamesJob.h"

AkonadiAddressbookNamesJob::AkonadiAddressbookNamesJob(const QString &email, AkonadiAddressbook *parent) :
    AddressbookNamesJob(parent), m_email(email), m_parent(parent), m_job(nullptr)
{
}

void AkonadiAddressbookNamesJob::doStart()
{
    Q_ASSERT(!m_job);
    m_job = new Akonadi::ContactSearchJob(this);
    m_job->setQuery(Akonadi::ContactSearchJob::Email, m_email, Akonadi::ContactSearchJob::ExactMatch);
    connect(m_job, &KJob::result, this, &AkonadiAddressbookNamesJob::searchResult);
}

void AkonadiAddressbookNamesJob::doStop()
{
    Q_ASSERT(m_job);
    disconnect(m_job, &KJob::result, this, &AkonadiAddressbookNamesJob::searchResult);
    // Kills and deletes the job immediately
    bool ok = m_job->kill();
    Q_ASSERT(ok); Q_UNUSED(ok);
    m_job = nullptr;
    emit error(AddressbookJob::Stopped);
    finished();
}

void AkonadiAddressbookNamesJob::searchResult(KJob *job)
{
    Akonadi::ContactSearchJob *searchJob = qobject_cast<Akonadi::ContactSearchJob*>(job);
    Q_ASSERT(searchJob);
    if (job->error()) {
        qWarning() << "AkonadiAddressbookNamesJob::searchResult:" << job->errorString();
        emit error(AddressbookJob::UnknownError);
    } else {
        QStringList displayNames;
        const auto contacts = searchJob->contacts();

        Q_FOREACH(const KContacts::Addressee &contact, contacts) {
            displayNames << contact.realName();
        }
        emit prettyNamesForAddressAvailable(displayNames);
    }

    finished();
}
