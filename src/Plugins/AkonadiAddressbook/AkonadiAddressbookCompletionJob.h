/* Copyright (C) Roland Pallai <dap78@magex.hu>

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

#ifndef AKONADIADDRESSBOOKCOMPLETIONJOB_H
#define AKONADIADDRESSBOOKCOMPLETIONJOB_H

#include "AkonadiAddressbook.h"

#include <akonadi-contact_version.h>
#if AKONADICONTACT_VERSION > QT_VERSION_CHECK(5, 19, 41)
#include <Akonadi/ContactSearchJob>
#else
#include <Akonadi/Contact/ContactSearchJob>
#endif

class AkonadiAddressbookCompletionJob : public AddressbookCompletionJob {
    Q_OBJECT

public:
    AkonadiAddressbookCompletionJob(const QString &input, const QStringList &ignores, int max, AkonadiAddressbook *parent);

public slots:
    virtual void doStart();
    virtual void doStop();

private slots:
    void searchResult(KJob *job);

private:
    QString m_input;
    QStringList m_ignores;
    int m_max;
    AkonadiAddressbook *m_parent;
    Akonadi::ContactSearchJob *m_job;
};

#endif
