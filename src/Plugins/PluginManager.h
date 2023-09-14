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

#ifndef PLUGIN_MANAGER
#define PLUGIN_MANAGER

#include <memory>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QPluginLoader>
#include <QPointer>

#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/SpellcheckerPlugin.h"

class QSettings;

namespace Cryptography {
class PartReplacer;
}

namespace Plugins {

class AddressbookPluginInterface;
class PasswordPluginInterface;
class SpellcheckerPluginInterface;

class PLUGINMANAGER_EXPORT PluginManager : public QObject
{
    Q_OBJECT
public:
    using MimePartReplacers = std::vector<std::shared_ptr<Cryptography::PartReplacer>>;
    /** @short Create plugin manager instance and load plugins */
    PluginManager(QObject *parent, QSettings *settings, const QString &addressbookKey, const QString &passwordKey, const QString &spellcheckerKey);

    virtual ~PluginManager();

    /** @short Return list of addressbook plugin pairs (name, description) */
    QMap<QString, QString> availableAddressbookPlugins() const;

    /** @short Return list of password plugin pairs (name, description) */
    QMap<QString, QString> availablePasswordPlugins() const;

    /** @short What sleppcheckers are available? */
    QMap<QString, QString> availableSpellcheckerPlugins() const;

    /** @short Return name of addressbook plugin */
    QString addressbookPlugin() const;

    /** @short Return name of password plugin */
    QString passwordPlugin() const;

    /** @short Name of the spellchecking plugin */
    QString spellcheckerPlugin() const;

    /** @short Change the addressbook plugin and start using it */
    void setAddressbookPlugin(const QString &name);

    /** @short Change the password plugin and start using it */
    void setPasswordPlugin(const QString &name);

    /** @short Set the spellchecker plugin */
    void setSpellcheckerPlugin(const QString &name);

    /** @short Return interface of addressbook plugin or nullptr when plugin was not found */
    Plugins::AddressbookPlugin *addressbook() const;

    /** @short Return interface of password plugin or nullptr when plugin was not found */
    Plugins::PasswordPlugin *password() const;

    /** @short Interface of a sleppchecking plugin, or a nullptr when plugin was not found */
    Plugins::SpellcheckerPlugin *spellchecker() const;

    const MimePartReplacers &mimePartReplacers() const;
    void setMimePartReplacers(const MimePartReplacers &replacers);

signals:
    void pluginsChanged();
    void pluginError(const QString &errorMessage);

private slots:
    void loadPlugins();

private:
    void loadPlugin(QObject *pluginInstance);

    QSettings *m_settings;
    QString m_addressbookKey;
    QString m_passwordKey;
    QString m_spellcheckerKey;

    QMap<QString, AddressbookPluginInterface *> m_availableAddressbookPlugins;
    QMap<QString, PasswordPluginInterface *> m_availablePasswordPlugins;
    QMap<QString, SpellcheckerPluginInterface *> m_availableSpellcheckPlugins;

    QString m_addressbookName;
    QString m_passwordName;
    QString m_spellcheckerName;

    QPointer<Plugins::AddressbookPlugin> m_addressbook;
    QPointer<Plugins::PasswordPlugin> m_password;
    QPointer<Plugins::SpellcheckerPlugin> m_spellchecker;

    MimePartReplacers m_mimePartReplacers;
};

} //namespace Common

#endif //PLUGIN_MANAGER

// vim: set et ts=4 sts=4 sw=4
