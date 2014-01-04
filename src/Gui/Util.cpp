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
#include <QCursor> // for Util::centerWidgetOnScreen
#include <QDesktopWidget> // for Util::centerWidgetOnScreen
#include <QDir>
#include <QGridLayout>
#include <QIcon>
#include <QProcess>
#include <QSettings>

#include "configure.cmake.h"
#include "Util.h"
#include "Window.h"

namespace {

bool isRunningKde4()
{
    return qgetenv("KDE_SESSION_VERSION") == "4";
}

bool isRunningGnome()
{
    return qgetenv("DESKTOP_SESSION") == "gnome";
}

bool isRunningXfce()
{
    return qgetenv("DESKTOP_SESSION") == "xfce";
}

/** @short Return full path to the $KDEHOME

Shamelessly stolen from Qt4's src/gui/kernel/qkde.cpp (it's a private class) and adopted to check $KDE_SESSION_VERSION
instead of yet another private class.
*/
QString kdeHome()
{
    static QString kdeHomePath;
    if (kdeHomePath.isEmpty()) {
        kdeHomePath = QString::fromLocal8Bit(qgetenv("KDEHOME"));
        if (kdeHomePath.isEmpty()) {
            QDir homeDir(QDir::homePath());
            QString kdeConfDir(QLatin1String("/.kde"));
            if (qgetenv("KDE_SESSION_VERSION") == "4" && homeDir.exists(QLatin1String(".kde4"))) {
                kdeConfDir = QLatin1String("/.kde4");
            }
            kdeHomePath = QDir::homePath() + kdeConfDir;
        }
    }
    return kdeHomePath;
}

}

namespace Gui
{

namespace Util
{

void centerWidgetOnScreen(QWidget *widget, bool centerOnCursorScreen)
{
    widget->adjustSize();
    // Regarding the option to center widget on screen containing mousepointer (only relevant for dual-screen-setups):
    // If some day we'll have (configurable) key shortcuts, there might be situations when the mousepointer (and
    // therefore most likely the user's attention) is not on the screen containing widget's parentWidget.   So by
    // centerning the widget on the screen containing the mousepointer we assure to be as close to the user's focus as
    // possible.  For single screen setups this doesn't make any difference at all.  If the widget to be centered is
    // shown as a result of a mouseClick this makes no difference, too, since the mouseClick most probably happened on
    // the widget's parentWidget so the widget will be centered on the screen containing it.  Still for the sake of
    // completeness the option to pass false is kept open for any case where it might be needed.
    if (centerOnCursorScreen) {
        widget->move(QApplication::desktop()->screenGeometry(QCursor::pos()).center()
                     - widget->rect().center());
    } else {
        widget->move(QApplication::desktop()->screenGeometry(widget->parentWidget()).center()
                     - widget->rect().center());
    }
}

/** @short Path to the "package data directory"

This path shall contain various files (like the localization data).  In case we're running without being installed
(or on some funny^Hnon-X11 platform), this function returns an empty QString.  Please also note that the returned
value might contain data for a completely different version of Trojita.
*/
QString pkgDataDir()
{
#ifdef PKGDATADIR
    return QLatin1String(PKGDATADIR);
#else
    return QString();
#endif
}

/** @short Return the source color tinted with the tintColor

This is shamelessly stolen from Qt5's qtquick1 module.
*/
QColor tintColor(const QColor &color, const QColor &tintColor)
{
    QColor finalColor;
    int a = tintColor.alpha();
    if (a == 0xff) {
        finalColor = tintColor;
    } else if (a == 0x00) {
        finalColor = color;
    } else {
        qreal a = tintColor.alphaF();
        qreal inv_a = 1.0 - a;

        finalColor.setRgbF(tintColor.redF() * a + color.redF() * inv_a,
                           tintColor.greenF() * a + color.greenF() * inv_a,
                           tintColor.blueF() * a + color.blueF() * inv_a,
                           a + inv_a * color.alphaF());
    }
    return finalColor;
}


/** @short Return the monospace font according to the systemwide settings */
QFont systemMonospaceFont()
{
    static bool initialized = false;
    static QFont font;

    if (!initialized) {
        if (isRunningKde4()) {
            // This part was shamelessly inspired by Qt4's src/gui/kernel/qapplication_x11.cpp
            QSettings kdeSettings(::kdeHome() + QLatin1String("/share/config/kdeglobals"), QSettings::IniFormat);
            QLatin1String confKey("fixed");
            QString fontDescription = kdeSettings.value(confKey).toStringList().join(QLatin1String(","));
            if (fontDescription.isEmpty())
                fontDescription = kdeSettings.value(confKey).toString();
            initialized = !fontDescription.isEmpty() && font.fromString(fontDescription);
        } else if (isRunningGnome() || isRunningXfce()) {
            // Under Gnome and Xfce, we can read the preferred font in yet another format via gconftool-2
            QByteArray fontDescription;
            do {
                QProcess gconf;
                gconf.start(QLatin1String("gconftool-2"),
                            QStringList() << QLatin1String("--get") << QLatin1String("/desktop/gnome/interface/monospace_font_name"));
                if (!gconf.waitForStarted())
                    break;
                gconf.closeWriteChannel();
                if (!gconf.waitForFinished())
                    break;
                fontDescription = gconf.readAllStandardOutput();
                fontDescription = fontDescription.trimmed();
            } while (0);

            // This value is apparently supposed to be parsed via the pango_font_description_from_string function. We, of course,
            // do not link with Pango, so we attempt to do a very crude parsing experiment by hand.
            // This code does *not* handle many fancy options like specifying the font size in pixels, parsing the bold/italics
            // options etc. It will also very likely break when the user has specified preefrences for more than one font family.
            // However, I hope it's better to try to do something than ignoring the problem altogether.
            int lastSpace = fontDescription.lastIndexOf(' ');
            bool ok;
            double size = fontDescription.mid(lastSpace).toDouble(&ok);
            if (lastSpace > 0 && ok) {
                font = QFont(fontDescription.left(lastSpace), size);
                initialized = true;
            }
        }
    }

    if (!initialized) {
        // Ok, that failed, let's create some fallback font.
        // The problem is that these names wary acros platforms,
        // but the following works well -- at first, we come up with a made-up name, and then
        // let the Qt font substitution algorithm do its magic.
        font = QFont(QLatin1String("x-trojita-terminus-like-fixed-width"));
        // Using QFont::Monospace results in a proportional font on jkt's system
        font.setStyleHint(QFont::TypeWriter);

        // Gnome, Mac and perhaps everyone else uses 10pt as the default font size, so let's use that as well
        int defaultPointSize = 10;
        if (isRunningKde4()) {
            // kdeui/kernel/kglobalsettings.cpp from KDELIBS sets default fixed font size to 9 on X11. Let's hope nobody runs
            // this desktop-GUI-specific code *with KDE* under Harmattan or Mac. Seriously.
            defaultPointSize = 9;

        }
        font.setPointSize(defaultPointSize);
    }

    return font;
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


