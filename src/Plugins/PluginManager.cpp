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

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QMap>
#include <QPluginLoader>
#include <QSettings>
#include <QString>
#include <QStringList>

#include "configure.cmake.h"

#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginInterface.h"

#include "PluginManager.h"

namespace Plugins {

PluginManager::PluginManager(QSettings *settings, const QString &addressbookKey, const QString &passwordKey, QObject *parent) : QObject(parent),
    m_settings(settings), m_addressbookKey(addressbookKey), m_passwordKey(passwordKey)
{
    m_addressbookPlugin = m_settings->value(m_addressbookKey, QLatin1String("abookaddressbook")).toString();
    m_passwordPlugin = m_settings->value(m_passwordKey, QLatin1String("cleartext")).toString();

    reloadPlugins();
}

PluginManager::~PluginManager()
{
}

void PluginManager::unloadPlugin(QPluginLoader *loader)
{
    if (loader) {
        loader->unload();
        delete loader;
    }
}

void PluginManager::loadPlugin(QObject *pluginInstance, QPluginLoader *loader)
{
    if (!pluginInstance) {
#ifdef PLUGIN_DEBUG
        qDebug() << "It is not qt library";
        if (loader)
            qDebug() << loader->errorString();
#endif
        // close library and cleanup resources
        unloadPlugin(loader);
        return;
    }

    PluginInterface *trojitaPlugin = qobject_cast<PluginInterface *>(pluginInstance);
    if (!trojitaPlugin) {
#ifdef PLUGIN_DEBUG
        qDebug() << "It is not trojita library";
        if (loader)
            qDebug() << loader->errorString();
#endif
        // close library and cleanup resources
        unloadPlugin(loader);
        return;
    }

    QObject *plugin = trojitaPlugin->create(this);
    if (!plugin) {
#ifdef PLUGIN_DEBUG
        qDebug() << "Failed to create QObject";
        if (loader)
            qDebug() << loader->errorString();
#endif
        // close library and cleanup resources
        unloadPlugin(loader);
        return;
    }

    const QString &name = trojitaPlugin->name();
    const QString &description = trojitaPlugin->description();

    if (name.isEmpty()) {
#ifdef PLUGIN_DEBUG
        qDebug() << "Plugin name is empty string";
#endif
        // close library and cleanup resources
        unloadPlugin(loader);
        return;
    }

    Plugins::AddressbookPlugin *newAddressbook = qobject_cast<Plugins::AddressbookPlugin *>(plugin);
    Plugins::PasswordPlugin *newPassword = qobject_cast<Plugins::PasswordPlugin *>(plugin);

#ifdef PLUGIN_DEBUG
    qDebug() << "Found Trojita plugin" << (loader ? loader->fileName() : QLatin1String("(static)")) <<
        ":" << name << ":" << description;
#endif

    bool unload = true;

    if (newAddressbook) {
        if (!m_availableAddressbookPlugins.contains(name))
            m_availableAddressbookPlugins.insert(name, description);
        if (!m_addressbook && name == m_addressbookPlugin) {
            m_addressbook = newAddressbook;
            m_addressbookLoader = loader;
            unload = false;
#ifdef PLUGIN_DEBUG
            qDebug() << "Using addressbook plugin" << name << ":" << description;
#endif
        }
    }

    if (newPassword) {
        if (!m_availablePasswordPlugins.contains(name))
            m_availablePasswordPlugins.insert(name, description);
        if (!m_password && name == m_passwordPlugin) {
            m_password = newPassword;
            m_passwordLoader = loader;
            unload = false;
#ifdef PLUGIN_DEBUG
            qDebug() << "Using password plugin" << name << ":" << description;
#endif
        }
    }

    if (unload) {
        // plugin is not used, so close library and cleanup resources
        delete plugin;
        unloadPlugin(loader);
    }
}

void PluginManager::reloadPlugins()
{
    delete m_addressbook;
    delete m_password;

    unloadPlugin(m_addressbookLoader);
    unloadPlugin(m_passwordLoader);

    m_availableAddressbookPlugins.clear();
    m_availablePasswordPlugins.clear();

    QString pluginDir;
    QStringList pluginDirs;

    pluginDir = qApp->applicationDirPath();
    if (!pluginDirs.contains(pluginDir))
        pluginDirs << pluginDir;

    pluginDir = QLatin1String(PLUGIN_DIR);
    if (!pluginDirs.contains(pluginDir))
        pluginDirs << pluginDir;

#ifdef PLUGIN_DEBUG
    qDebug() << "Number of static linked plugins:" << QPluginLoader::staticInstances().size();
#endif

#ifdef PLUGIN_DEBUG
    qDebug() << "Searching for plugins in:" << pluginDirs;
#endif

    QStringList absoluteFilePaths;

    Q_FOREACH(const QString &dirName, pluginDirs) {
        QDir dir(dirName);
        Q_FOREACH(const QString &fileName, dir.entryList(QStringList() << QLatin1String("trojita_plugin_*"), QDir::Files)) {
            const QString &absoluteFilePath = QFileInfo(dir.absoluteFilePath(fileName)).canonicalFilePath();
            if (absoluteFilePaths.contains(absoluteFilePath))
                continue;
            absoluteFilePaths << absoluteFilePath;
#ifdef PLUGIN_DEBUG
            qDebug() << "Opening file" << absoluteFilePath;
#endif
            QPluginLoader *loader = new QPluginLoader(absoluteFilePath, this);
            loadPlugin(loader->instance(), loader);
        }
    }

    Q_FOREACH(QObject *pluginInstance, QPluginLoader::staticInstances())
        loadPlugin(pluginInstance, NULL);

}

QMap<QString, QString> PluginManager::availableAddressbookPlugins()
{
    return m_availableAddressbookPlugins;
}

QMap<QString, QString> PluginManager::availablePasswordPlugins()
{
    return m_availablePasswordPlugins;
}

QString PluginManager::addressbookPlugin()
{
    return m_addressbookPlugin;
}

QString PluginManager::passwordPlugin()
{
    return m_passwordPlugin;
}

void PluginManager::setAddressbookPlugin(const QString &plugin)
{
    m_addressbookPlugin = plugin;
    m_settings->setValue(m_addressbookKey, plugin);
}

void PluginManager::setPasswordPlugin(const QString &plugin)
{
    m_passwordPlugin = plugin;
    m_settings->setValue(m_passwordKey, plugin);
}

Plugins::AddressbookPlugin *PluginManager::addressbook()
{
    return m_addressbook;
}

Plugins::PasswordPlugin *PluginManager::password()
{
    return m_password;
}

} //namespace Common

// vim: set et ts=4 sts=4 sw=4
