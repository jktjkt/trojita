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

#ifndef ADDRESSBOOK_INTERFACE
#define ADDRESSBOOK_INTERFACE

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

#include "PluginJob.h"

namespace Plugins
{

struct PLUGINS_EXPORT NameEmail
{
    NameEmail(const QString &n, const QString &e) : name(n), email(e) {}
    QString name;
    QString email;
};

typedef QList<NameEmail> NameEmailList;

class PLUGINS_EXPORT AddressbookJob : public PluginJob
{
    Q_OBJECT

public:
    /** @short Error emitted by error signal */
    enum Error {
        UnknownError, /**< Unknown error */
        Stopped /**< Emitted when job stopped */
    };

signals:
    /** @short Emitted when job finishes unsuccessful with error */
    void error(Plugins::AddressbookJob::Error error);

protected:
    AddressbookJob(QObject *parent);
};

class PLUGINS_EXPORT AddressbookCompletionJob : public AddressbookJob
{
    Q_OBJECT

signals:
    /** @short Emitted when completion is available */
    void completionAvailable(const Plugins::NameEmailList &completion);

protected:
    AddressbookCompletionJob(QObject *parent);
};

class PLUGINS_EXPORT AddressbookNamesJob : public AddressbookJob
{
    Q_OBJECT

signals:
    /** @short Emitted when pretty names for address is available */
    void prettyNamesForAddressAvailable(const QStringList &displayNames);

protected:
    AddressbookNamesJob(QObject *parent);
};

class PLUGINS_EXPORT AddressbookPlugin : public QObject
{
    Q_OBJECT

public:
    /** @short Features returned by method features */
    enum Feature {
        FeatureAddressbookWindow = 1 << 0, /**< Plugin has support for opening addressbook window via openAddressbookWindow */
        FeatureContactWindow = 1 << 1, /**< Plugin has support for opening contact window via openContactWindow */
        FeatureAddContact = 1 << 2, /**< Plugin has support for adding new contact via openContactWindow */
        FeatureEditContact = 1 << 3, /**< Plugin has support for editing existing contact via openContactWindow */
        FeatureCompletion = 1 << 4, /**< Plugin has support for completion via requestCompletion */
        FeaturePrettyNames = 1 << 5 /**< Plugin has support for display names via requestPrettyNamesForAddress */
    };

    Q_DECLARE_FLAGS(Features, Feature);

    /** @short Return implementation features */
    virtual Features features() const = 0;

public slots:
    /** @short Request a list of pairs (name, email) matching contacts and return AddressbookJob
     *  @p input is input string
     *  @p ignores is list of strings which are NOT included in result
     *  @p max is the demanded maximum reply length, negative value means "uncapped"
     */
    virtual AddressbookCompletionJob *requestCompletion(const QString &input, const QStringList &ignores = QStringList(), int max = -1) = 0;

    /** @short Request a list of display names matching the given e-mail address and return AddressbookJob
     *  @p email is e-mail address
     */
    virtual AddressbookNamesJob *requestPrettyNamesForAddress(const QString &email) = 0;

    /** @short Open window for addressbook manager */
    virtual void openAddressbookWindow() = 0;

    /** @short Open window for specified contact
     *  first try to match contact by email, then by name
     *  if contact not exist, open window for adding new contact and fill name and email strings
     */
    virtual void openContactWindow(const QString &email, const QString &displayName) = 0;

protected:
    AddressbookPlugin(QObject *parent);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AddressbookPlugin::Features)

}

#endif //ADDRESSBOOK_INTERFACE

// vim: set et ts=4 sts=4 sw=4
