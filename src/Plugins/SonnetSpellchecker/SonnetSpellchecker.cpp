/* Copyright (C) Jan Kundrát <jkt@kde.org>

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

#include "SonnetSpellchecker.h"
#include <SonnetUi/sonnet/spellcheckdecorator.h>
#include <SonnetUi/sonnet/highlighter.h>

namespace Plugins {

SonnetSpellchecker::SonnetSpellchecker(QObject *parent) : SpellcheckerPlugin(parent)
{
}

void SonnetSpellchecker::actOnEditor(QTextEdit *editor)
{
    Sonnet::SpellCheckDecorator *decorator = new Sonnet::SpellCheckDecorator(editor);
    decorator->highlighter()->setAutomatic(false);
}

QString trojita_plugin_SonnetSpellcheckerPlugin::name() const
{
    return QStringLiteral("sonnetspellchecker");
}

QString trojita_plugin_SonnetSpellcheckerPlugin::description() const
{
    return tr("Sonnet (KF5)");
}

Plugins::SpellcheckerPlugin *trojita_plugin_SonnetSpellcheckerPlugin::create(QObject *parent, QSettings *settings)
{
    Q_UNUSED(settings);
    return new SonnetSpellchecker(parent);
}

}
