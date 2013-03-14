/*
   Copyright (C) 2012, 2013 by Glad Deschrijver <glad.deschrijver@gmail.com>

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

#ifndef SHORTCUTCONFIGDIALOG_H
#define SHORTCUTCONFIGDIALOG_H

#ifndef QT_NO_SHORTCUT

#include <QDialog>

class QAction;
class QKeySequence;

namespace Gui
{

class ActionDescription;
class ShortcutConfigWidget;

/**
 * Application programmers are not supposed to access this class directly.
 * \see ShortcutHandler
 */
class ShortcutConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutConfigDialog(QWidget *parent = 0);
    ~ShortcutConfigDialog();

    void setExclusivityGroups(const QList<QStringList> &groups);
    void setActionDescriptions(const QHash<QString, ActionDescription> &actionDescriptions);

Q_SIGNALS:
    void shortcutsChanged(const QHash<QString, ActionDescription> &actionDescriptions);

protected Q_SLOTS:
    void accept();
    void reject();

private:
    ShortcutConfigWidget *m_shortcutConfigWidget;
};

} // namespace Gui

#endif // QT_NO_SHORTCUT

#endif // SHORTCUTCONFIGDIALOG_H
