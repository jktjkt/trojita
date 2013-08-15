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

#include <QTimer>

#include "PluginJob.h"

namespace Plugins
{

void PluginJob::start()
{
    if (m_running)
        return;
    m_running = true;
    QTimer::singleShot(0, this, SLOT(doStart()));
}

void PluginJob::stop()
{
    if (!m_running)
        return;
    QTimer::singleShot(0, this, SLOT(doStop()));
}

bool PluginJob::autoDelete()
{
    return m_autoDelete;
}

void PluginJob::setAutoDelete(bool b)
{
    m_autoDelete = b;
}

bool PluginJob::isRunning()
{
    return m_running;
}

void PluginJob::finished()
{
    m_running = false;
    if (m_autoDelete)
        deleteLater();
}

PluginJob::PluginJob(QObject *parent) : QObject(parent), m_running(false), m_autoDelete(false)
{
}

PluginJob::~PluginJob()
{
}

}

// vim: set et ts=4 sts=4 sw=4
