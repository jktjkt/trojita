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

#pragma once

#include "Plugins/SpellcheckerPlugin.h"
#include "Plugins/PluginInterface.h"

namespace Plugins {

class SonnetSpellchecker : public SpellcheckerPlugin
{
    Q_OBJECT

public:
    explicit SonnetSpellchecker(QObject *parent);

public slots:
    void actOnEditor(QTextEdit* editor) override;

};

class trojita_plugin_SonnetSpellcheckerPlugin : public QObject, public SpellcheckerPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(Plugins::SpellcheckerPluginInterface)
    Q_PLUGIN_METADATA(IID "net.flaska.trojita.plugins.spellchecker.sonnet")

public:
    QString name() const override;
    QString description() const override;
    SpellcheckerPlugin *create(QObject *parent, QSettings *settings) override;
};

}
