/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#ifndef TROJITA_MSA_IMAP_SUBMIT_H
#define TROJITA_MSA_IMAP_SUBMIT_H

#include "AbstractMSA.h"

namespace Imap {
namespace Mailbox {
class Model;
}
}

namespace MSA
{

class ImapSubmit : public AbstractMSA
{
    Q_OBJECT
public:
    ImapSubmit(QObject *parent, Imap::Mailbox::Model *model);
    virtual bool supportsImapSending() const;
    virtual void sendImap(const QString &mailbox, const int uidValidity, const int uid,
                          const Imap::Mailbox::UidSubmitOptionsList options);

public slots:
    virtual void cancel();

private:
    Imap::Mailbox::Model *m_model;

    ImapSubmit(const ImapSubmit &); // don't implement
    ImapSubmit &operator=(const ImapSubmit &); // don't implement
};

class ImapSubmitFactory: public MSAFactory
{
public:
    explicit ImapSubmitFactory(Imap::Mailbox::Model *model);
    virtual ~ImapSubmitFactory();
    virtual AbstractMSA *create(QObject *parent) const;
private:
    Imap::Mailbox::Model *m_model;
};

}

#endif // TROJITA_MSA_IMAP_SUBMIT_H
