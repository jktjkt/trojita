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

#ifndef FAKECAPABILITIESINJECTOR_H
#define FAKECAPABILITIESINJECTOR_H

#include "Imap/Model/Model.h"

/** @short Class for persuading the model that the parser supports certain capabilities */
class FakeCapabilitiesInjector
{
public:
    explicit FakeCapabilitiesInjector(Imap::Mailbox::Model *model): model(model)
    {}

    /** @short Add the specified capability to the list of capabilities "supported" by the server */
    void injectCapability(const QString& cap)
    {
        Q_ASSERT(!model->m_parsers.isEmpty());
        for (auto it = model->m_parsers.begin(); it != model->m_parsers.end(); ++it) {
            auto existingCaps = it->capabilities;
            if (!existingCaps.contains(QStringLiteral("IMAP4REV1"))) {
                existingCaps << QStringLiteral("IMAP4rev1");
            }
            if (!existingCaps.contains(cap.toUpper())) {
                existingCaps << cap;
            }
            model->updateCapabilities(it.key(), existingCaps);
        }
    }
private:
    Imap::Mailbox::Model *model;
};

#endif // FAKECAPABILITIESINJECTOR_H
