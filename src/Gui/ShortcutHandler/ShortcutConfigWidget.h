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

#ifndef SHORTCUTCONFIGWIDGET_H
#define SHORTCUTCONFIGWIDGET_H

#ifndef QT_NO_SHORTCUT

#include <QWidget>
#include "ui_ShortcutConfigWidget.h"

class QAction;
class QKeySequence;

namespace Gui
{

class ActionDescription;

/**
 * Application programmers are not supposed to construct this class directly.
 * Use ShortcutHandler::configWidget() instead.
 * \see ShortcutHandler
 *
 * The purpose of this class is to allow program developers to add a shortcut
 * editing widget to a configuration dialog (for example as one of its tabs).
 */
class ShortcutConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ShortcutConfigWidget(QWidget *parent = 0);
    ~ShortcutConfigWidget();

    /**
     * This function should be called each time the configuration dialog
     * to which this widget is added is accepted. This function saves the
     * modified shortcuts to disk and makes sure that the modified shortcuts
     * can be used in the program.
     */
    void accept();
    /**
     * This function does not need to be used explicitly.
     * This function may be called each time the configuration dialog
     * to which this widget is added is rejected. This function discards any
     * modification which was done to the shortcuts and which is not yet saved.
     * This ensures that when the configuration dialog is shown again, the old
     * shortcuts are displayed instead of the not yet saved modifications.
     * If this function is not called when the configuration dialog is
     * canceled, reject() will be automatically called when this widget is
     * shown again.
     */
    void reject();

Q_SIGNALS:
    void shortcutsChanged(const QHash<QString, ActionDescription> &actionDescriptions);

protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void showEvent(QShowEvent *event);

    Ui::ShortcutConfigWidget ui;

private Q_SLOTS:
    void searchItems(const QString &text);
    void clearShortcut();
    void restoreDefaultShortcut();

private:
    QPushButton *clearButton(); // this function needs to be visible to ShortcutConfigDialog
    QPushButton *useDefaultButton(); // idem
    void setExclusivityGroups(const QList<QStringList> &groups);
    void addItem(const QString &actionName, const QString &text, const QString &shortcut, const QIcon &icon, const QString &parentId);
    void setActionDescriptions(const QHash<QString, ActionDescription> &actionDescriptions);
    void writeSettings();

    friend class ShortcutHandler;
    friend class ShortcutConfigDialog;

    QHash<QString, ActionDescription> m_actionDescriptions;
    QList<QStringList> m_exclusivityGroups;
    bool m_shortcutsShouldBeRestored;
};

} // namespace Gui

#endif // QT_NO_SHORTCUT

#endif // SHORTCUTCONFIGWIDGET_H
