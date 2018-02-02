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

#include "AkonadiAddressbookCompletionJob.h"

#include <QDebug>

AkonadiAddressbookCompletionJob::AkonadiAddressbookCompletionJob(const QString &input, const QStringList &ignores, int max, AkonadiAddressbook *parent) :
    AddressbookCompletionJob(parent), m_input(input), m_ignores(ignores), m_max(max), m_parent(parent), m_job(nullptr)
{
}

void AkonadiAddressbookCompletionJob::doStart()
{
    Q_ASSERT(!m_job);
    m_job = new Akonadi::ContactSearchJob(this);
    // Contacts without email are skipped below so fetch some more
    if (m_max != -1)
        m_job->setLimit(m_max * 2);
    m_job->setQuery(Akonadi::ContactSearchJob::NameOrEmail, m_input, Akonadi::ContactSearchJob::StartsWithMatch);
    connect(m_job, &KJob::result, this, &AkonadiAddressbookCompletionJob::searchResult);
}

void AkonadiAddressbookCompletionJob::doStop()
{
    Q_ASSERT(m_job);
    disconnect(m_job, &KJob::result, this, &AkonadiAddressbookCompletionJob::searchResult);
    // Kills and deletes the job immediately
    bool ok = m_job->kill();
    Q_ASSERT(ok); Q_UNUSED(ok);
    m_job = nullptr;
    emit error(AddressbookJob::Stopped);
    finished();
}

void AkonadiAddressbookCompletionJob::searchResult(KJob *job)
{
    Akonadi::ContactSearchJob *searchJob = qobject_cast<Akonadi::ContactSearchJob*>(job);
    Q_ASSERT(searchJob);
    if (job->error()) {
        qWarning() << "AkonadiAddressbookCompletionJob::searchResult:" << job->errorString();
        emit error(AddressbookJob::UnknownError);
    } else {
        NameEmailList list;
        const auto contacts = searchJob->contacts();

        for (int i = 0; i < contacts.size() && (m_max == -1 || list.size() < m_max); ++i) {
            KContacts::Addressee contact = contacts[i];

            // put the matching ones first and then the rest
            QStringList emails1 = contact.emails().filter(m_input, Qt::CaseInsensitive);
            QStringList emails2 = contact.emails().toSet().subtract(emails1.toSet()).toList();

            Q_FOREACH(const QString &email, emails1 + emails2) {
                if (!m_ignores.contains(email)) {
                    list << NameEmail(contact.realName(), email);
                    if (m_max != -1 && list.size() >= m_max)
                        break;
                }
            }
        }
        emit completionAvailable(list);
    }

    finished();
}
