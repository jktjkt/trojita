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
#include "Common/InvokeMethod.h"
#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginInterface.h"
#include "Plugins/PluginManager.h"


namespace Plugins {

PluginManager::PluginManager(QObject *parent, QSettings *settings, const QString &addressbookKey, const QString &passwordKey) : QObject(parent),
    m_settings(settings), m_addressbookKey(addressbookKey), m_passwordKey(passwordKey)
{
    m_addressbookName = m_settings->value(m_addressbookKey, QLatin1String("abookaddressbook")).toString();
    m_passwordName = m_settings->value(m_passwordKey, QLatin1String("cleartextpassword")).toString();
    CALL_LATER_NOARG(this, loadPlugins)
}

void PluginManager::loadPlugins()
{
    QStringList pluginDirs;
    pluginDirs << qApp->applicationDirPath();

    auto pluginDir = QStringLiteral(PLUGIN_DIR);
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
        auto filter(QStringLiteral("trojita_plugin_*"));
        Q_FOREACH(const QString &fileName, dir.entryList(QStringList() << filter, QDir::Files)) {
            const QString &absoluteFilePath = QFileInfo(dir.absoluteFilePath(fileName)).canonicalFilePath();
            if (absoluteFilePaths.contains(absoluteFilePath)) {
                continue;
            }
            absoluteFilePaths << absoluteFilePath;
            if (!QLibrary::isLibrary(absoluteFilePath)) {
                continue;
            }
#ifdef PLUGIN_DEBUG
            qDebug() << "Opening file" << absoluteFilePath;
#endif
            QPluginLoader *loader = new QPluginLoader(absoluteFilePath, this);
            if (loader->load()) {
                loadPlugin(loader->instance(), loader);
            } else {
                emit pluginError(loader->errorString());
            }
        }
    }

    Q_FOREACH(QObject *pluginInstance, QPluginLoader::staticInstances()) {
        loadPlugin(pluginInstance, nullptr);
    }

    emit pluginsChanged();
}

PluginManager::~PluginManager()
{
}

void PluginManager::loadPlugin(QObject *pluginInstance, QPluginLoader *loader)
{
    Q_ASSERT(pluginInstance);

    if (auto abookPlugin = qobject_cast<AddressbookPluginInterface *>(pluginInstance)) {
        const QString &name = abookPlugin->name();
        Q_ASSERT(!name.isEmpty());
        Q_ASSERT(!m_availableAddressbookPlugins.contains(name));
        m_availableAddressbookPlugins[name] = abookPlugin;
#ifdef PLUGIN_DEBUG
        qDebug() << "Found addressbook plugin" << name << ":" << abookPlugin->description();
#endif
        if (name == m_addressbookName) {
#ifdef PLUGIN_DEBUG
            qDebug() << "Will activate new default plugin" << name;
#endif
            setAddressbookPlugin(name);
        }
    }

    if (auto passwordPlugin = qobject_cast<PasswordPluginInterface *>(pluginInstance)) {
        const QString &name = passwordPlugin->name();
        Q_ASSERT(!name.isEmpty());
        Q_ASSERT(!m_availablePasswordPlugins.contains(name));
        m_availablePasswordPlugins[name] = passwordPlugin;
#ifdef PLUGIN_DEBUG
        qDebug() << "Found password plugin" << name << ":" << passwordPlugin->description();
#endif
        if (name == m_passwordName) {
#ifdef PLUGIN_DEBUG
            qDebug() << "Will activate new default plugin" << name;
#endif
            setPasswordPlugin(name);
        }
    }
}

QMap<QString, QString> PluginManager::availableAddressbookPlugins() const
{
    QMap<QString, QString> res;
    for (auto plugin: m_availableAddressbookPlugins) {
        res[plugin->name()] = plugin->description();
    }
    return res;
}

QMap<QString, QString> PluginManager::availablePasswordPlugins() const
{
    QMap<QString, QString> res;
    for (auto plugin: m_availablePasswordPlugins) {
        res[plugin->name()] = plugin->description();
    }
    return res;
}

QString PluginManager::addressbookPlugin() const
{
    return m_addressbookName;
}

QString PluginManager::passwordPlugin() const
{
    return m_passwordName;
}

void PluginManager::setAddressbookPlugin(const QString &name)
{
    m_addressbookName = name;
    m_settings->setValue(m_addressbookKey, name);

    if (m_addressbook) {
        delete m_addressbook;
    }

    auto plugin = m_availableAddressbookPlugins.find(name);
    if (plugin != m_availableAddressbookPlugins.end()) {
#ifdef PLUGIN_DEBUG
        qDebug() << "Setting new address book plugin:" << (*plugin)->name();
#endif
        m_addressbook = (*plugin)->create(this, m_settings);
    }

    emit pluginsChanged();
}

void PluginManager::setPasswordPlugin(const QString &name)
{
    m_passwordName = name;
    m_settings->setValue(m_passwordKey, name);

    if (m_password) {
        delete m_password;
    }

    auto plugin = m_availablePasswordPlugins.find(name);
    if (plugin != m_availablePasswordPlugins.end()) {
#ifdef PLUGIN_DEBUG
        qDebug() << "Setting new password plugin:" << (*plugin)->name();
#endif
        m_password = (*plugin)->create(this, m_settings);
    }

    emit pluginsChanged();
}

Plugins::AddressbookPlugin *PluginManager::addressbook() const
{
    return m_addressbook;
}

Plugins::PasswordPlugin *PluginManager::password() const
{
    return m_password;
}

const PluginManager::MimePartReplacers &PluginManager::mimePartReplacers() const
{
    return m_mimePartReplacers;
}

// FIXME: this API sucks, but there's a can of worms when it comes to libtrojita_plugins.so...
void PluginManager::setMimePartReplacers(const MimePartReplacers &replacers)
{
    m_mimePartReplacers = replacers;
}

} //namespace Common

// vim: set et ts=4 sts=4 sw=4
