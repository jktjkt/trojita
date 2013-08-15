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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QPluginLoader>
#include <QPointer>

#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PasswordPlugin.h"

class QSettings;

namespace Plugins {

class PLUGINS_EXPORT PluginManager : public QObject
{
public:
    /** @short Create plugin manager instance and load plugins */
    PluginManager(QSettings *settings, const QString &addressbookKey, const QString &passwordKey, QObject *parent);

    virtual ~PluginManager();

    /** @short Unload all plugins and load new again */
    void reloadPlugins();

    /** @short Return list of addressbook plugin pairs (name, description) */
    QMap<QString, QString> availableAddressbookPlugins();

    /** @short Return list of password plugin pairs (name, description) */
    QMap<QString, QString> availablePasswordPlugins();

    /** @short Return name of addressbook plugin */
    QString addressbookPlugin();

    /** @short Return name of password plugin */
    QString passwordPlugin();

    /** @short Change name of addressbook plugin, to load it use @see reloadPlugins */
    void setAddressbookPlugin(const QString &plugin);

    /** @short Change name of password plugin, to load it use @see reloadPlugins */
    void setPasswordPlugin(const QString &plugin);

    /** @short Return interface of addressbook plugin or NULL when plugin was not found or not loaded */
    Plugins::AddressbookPlugin *addressbook();

    /** @short Return interface of password plugin or NULL when plugin was not found or not loaded */
    Plugins::PasswordPlugin *password();

private:
    void loadPlugin(QObject *pluginInstance, QPluginLoader *loader);
    void unloadPlugin(QPluginLoader *loader);

    QSettings *m_settings;
    QString m_addressbookKey;
    QString m_passwordKey;

    QMap<QString, QString> m_availableAddressbookPlugins;
    QMap<QString, QString> m_availablePasswordPlugins;

    QString m_addressbookPlugin;
    QString m_passwordPlugin;

    QPointer<QPluginLoader> m_addressbookLoader;
    QPointer<QPluginLoader> m_passwordLoader;

    QPointer<Plugins::AddressbookPlugin> m_addressbook;
    QPointer<Plugins::PasswordPlugin> m_password;
};

} //namespace Common

#endif //PLUGIN_MANAGER

// vim: set et ts=4 sts=4 sw=4
