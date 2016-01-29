/* Copyright (C) 2006 - 2011 Thomas Gahr <thomas.gahr@physik.uni-muenchen.de>
   Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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


#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QDir>
#include <QFontDatabase>
#include <QGridLayout>
#include <QIcon>
#include <QProcess>
#include <QSettings>

#include "configure.cmake.h"
#include "Util.h"
#include "Window.h"

namespace Gui {

namespace Util {

/** @short Path to the "package data directory"

This path shall contain various files (like the localization data).  In case we're running without being installed
(or on some funny^Hnon-X11 platform), this function returns an empty QString.  Please also note that the returned
value might contain data for a completely different version of Trojita.
*/
QString pkgDataDir()
{
#ifdef PKGDATADIR
    return QStringLiteral(PKGDATADIR);
#else
    return QString();
#endif
}

/** @short Ask for something and provide a facility to not ask again

Check settings whether an option is already set to ignore this question. If not, ask the user and remember whether
she wants to be asked again.
*/
int askForSomethingUnlessTold(const QString &title, const QString &message, const QString &settingsName,
                              QMessageBox::StandardButtons buttons, QWidget *parent, QSettings *settings)
{
    int saved = settings->value(settingsName, -1).toInt();
    if (saved != -1) {
        // This means that we're not supposed to ask again
        return saved;
    }

    QMessageBox box(QMessageBox::Question, title, message, QMessageBox::NoButton, parent);
    box.setStandardButtons(buttons);
    QCheckBox *checkbox = new QCheckBox(Gui::MainWindow::tr("Don't ask again"), &box);
    QGridLayout *layout = qobject_cast<QGridLayout*>(box.layout());
    Q_ASSERT(layout);
    layout->addWidget(checkbox, 1, 1);
    int res = box.exec();
    if (checkbox->isChecked())
        settings->setValue(settingsName, res);
    return res;
}

/** @short Return image data from the specified filename as a self-contained URL of the data: scheme

The image is resized and always returned in the PNG format.
*/
QString resizedImageAsDataUrl(const QString &fileName, const int extent)
{
    QByteArray bdata;
    QBuffer buf(&bdata);
    buf.open(QIODevice::WriteOnly);
    QIcon(fileName).pixmap(extent).toImage().save(&buf, "png");
    return QLatin1String("data:image/png;base64,") + QString::fromUtf8(bdata.toBase64());
}

} // namespace Util

} // namespace Gui


