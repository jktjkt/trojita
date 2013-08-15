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

#ifndef TROJITA_PLUGIN_JOB
#define TROJITA_PLUGIN_JOB

#include <QObject>

#ifdef QT_STATICPLUGIN
#define PLUGINS_EXPORT
#else
#ifdef BUILD_PLUGIN
#define PLUGINS_EXPORT Q_DECL_IMPORT
#else
#define PLUGINS_EXPORT Q_DECL_EXPORT
#endif
#endif

namespace Plugins
{

class PLUGINS_EXPORT PluginJob : public QObject
{
    Q_OBJECT

public:
    ~PluginJob();

    /** @short Return true if auto delete is set */
    bool autoDelete();

    /** @short Set auto delete, when b is true, object is deleted after finish via deleteLater */
    void setAutoDelete(bool b);

    /** @short Return true if job running */
    bool isRunning();

public slots:
    /** @short Start job - it will call doStart() via event loop */
    void start();

    /** @short Stop job */
    void stop();

private:
    bool m_running;
    bool m_autoDelete;

protected:
    PluginJob(QObject *parent);

protected slots:
    /** @short Reimplement starting job */
    virtual void doStart() = 0;

    /** @short Reimplement stopping job */
    virtual void doStop() = 0;

    /** @short Call when job finish (successful or unsuccessful) */
    void finished();
};

}

#endif //TROJITA_PLUGIN_JOB

// vim: set et ts=4 sts=4 sw=4
