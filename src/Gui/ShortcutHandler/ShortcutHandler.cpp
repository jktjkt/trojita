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

#include "ShortcutHandler.h"

#ifndef QT_NO_SHORTCUT

#include <QAction>
#include <QSettings>

#include "IconLoader.h"
#include "ShortcutConfigDialog.h"
#include "ShortcutConfigWidget.h"
#include "Common/SettingsCategoryGuard.h"

namespace Gui
{

ShortcutHandler *ShortcutHandler::self = 0;

ShortcutHandler::ShortcutHandler(QWidget *parent)
    : QObject(parent)
    , m_settings(0)
    , m_shortcutConfigAction(0)
    , m_shortcutConfigDialog(0)
    , m_shortcutConfigWidget(0)
{
    Q_ASSERT_X(!self, "ShortcutHandler", "there should be only one shortcut handler object");
    ShortcutHandler::self = this;
    m_exclusivityGroups.append(QStringList() << tr("Main Window"));
}

ShortcutHandler::~ShortcutHandler()
{
    if (m_shortcutConfigAction)
        delete m_shortcutConfigAction;
    if (m_shortcutConfigDialog)
        delete m_shortcutConfigDialog;
    if (m_shortcutConfigWidget)
        delete m_shortcutConfigWidget;
    m_actionDescriptions.clear();
}

void ShortcutHandler::setSettingsObject(QSettings *settings)
{
    m_settings = settings;
}

void ShortcutHandler::defineAction(const QString &actionName, const QString &iconName, const QString &text, const QString &defaultShortcut, const QString &parentId)
{
//  m_actionDescriptions[actionName] = {iconName, text, defaultShortcut, parentId}; // C++11 only, constructor in ActionDescription is then not needed
    m_actionDescriptions[actionName] = ActionDescription(iconName, text, defaultShortcut, parentId);
}

void ShortcutHandler::defineAction(const QString &actionName, const QString &iconName, const QString &text, const QKeySequence::StandardKey &key, const QString &parentId)
{
    m_actionDescriptions[actionName] = ActionDescription(iconName, text, QKeySequence(key).toString(QKeySequence::PortableText), parentId);
}

QAction *ShortcutHandler::createAction(const QString &actionName, QWidget *parent)
{
    return createAction(actionName, 0, "", parent);
}

QAction *ShortcutHandler::createAction(const QString &actionName, QObject *receiver, const char *member, QWidget *parent)
{
    if (!m_actionDescriptions.contains(actionName))
        return 0;

    ActionDescription actionDescription = m_actionDescriptions[actionName];
    QAction *action = new QAction(parent);
    action->setObjectName(actionName);
    if (!actionDescription.iconName.isEmpty()) {
        action->setIcon(loadIcon(actionDescription.iconName));
    }
    action->setText(actionDescription.text);
    action->setShortcut(actionDescription.shortcut);
    if (receiver)
        connect(action, SIGNAL(triggered()), receiver, member);
    m_actionDescriptions[actionName].action = action;
    return action;
}

void ShortcutHandler::addExclusivityGroup(const QStringList &groups)
{
    if (groups.contains(tr("Main Window"))) {
        m_exclusivityGroups.takeFirst();
        m_exclusivityGroups.prepend(groups);
    } else {
        m_exclusivityGroups.append(groups);
    }

    if (m_shortcutConfigDialog)
        m_shortcutConfigDialog->setExclusivityGroups(m_exclusivityGroups);
    if (m_shortcutConfigWidget)
        m_shortcutConfigWidget->setExclusivityGroups(m_exclusivityGroups);
}

QSettings *ShortcutHandler::settingsObject()
{
    return m_settings;
}

QAction *ShortcutHandler::action(const QString &actionName)
{
    if (!m_actionDescriptions.contains(actionName))
        return 0;
    return m_actionDescriptions[actionName].action;
}

/***************************************************************************/

void ShortcutHandler::changeShortcuts(const QHash<QString, ActionDescription> &actionDescriptions)
{
    for (QHash<QString, ActionDescription>::const_iterator it = actionDescriptions.constBegin(); it != actionDescriptions.constEnd(); ++it) {
        ActionDescription actionDescription = it.value();
        QAction *action = m_actionDescriptions[it.key()].action;
        if (action)
            action->setShortcut(QKeySequence(actionDescription.shortcut));
    }
}

QAction *ShortcutHandler::shortcutConfigAction()
{
    Q_ASSERT_X(!m_shortcutConfigWidget, "ShortcutHandler", "a shortcut configuration dialog and a shortcut configuration widget cannot exist at the same time in one application");
    if (!m_shortcutConfigAction) {
        m_shortcutConfigAction = new QAction(loadIcon("configure-shortcuts"), tr("Configure S&hortcuts..."), qobject_cast<QWidget*>(parent()));
        QObject::connect(m_shortcutConfigAction, SIGNAL(triggered()), this, SLOT(openShortcutConfigDialog()));
    }
    return m_shortcutConfigAction;
}

void ShortcutHandler::openShortcutConfigDialog()
{
    if (!m_shortcutConfigDialog) {
        m_shortcutConfigDialog = new ShortcutConfigDialog(qobject_cast<QWidget*>(parent()));
        m_shortcutConfigDialog->setActionDescriptions(m_actionDescriptions);
        m_shortcutConfigDialog->setExclusivityGroups(m_exclusivityGroups);
        connect(m_shortcutConfigDialog, SIGNAL(shortcutsChanged(QHash<QString,ActionDescription>)), this, SLOT(changeShortcuts(QHash<QString,ActionDescription>)));
    }
    m_shortcutConfigDialog->exec();
}

/***************************************************************************/

ShortcutConfigWidget *ShortcutHandler::configWidget()
{
    Q_ASSERT_X(!m_shortcutConfigAction, "ShortcutHandler", "a shortcut configuration dialog and a shortcut configuration widget cannot exist at the same time in one application");
    if (!m_shortcutConfigWidget) {
        m_shortcutConfigWidget = new ShortcutConfigWidget(qobject_cast<QWidget*>(parent()));
        m_shortcutConfigWidget->setActionDescriptions(m_actionDescriptions);
        m_shortcutConfigWidget->setExclusivityGroups(m_exclusivityGroups);
        connect(m_shortcutConfigWidget, SIGNAL(shortcutsChanged(QHash<QString,ActionDescription>)), this, SLOT(changeShortcuts(QHash<QString,ActionDescription>)));
    }
    return m_shortcutConfigWidget;
}

void ShortcutHandler::accept()
{
    m_shortcutConfigWidget->accept();
}

void ShortcutHandler::reject()
{
    m_shortcutConfigWidget->reject();
}

/***************************************************************************/

void ShortcutHandler::readSettings()
{
    Q_ASSERT_X(m_settings, "ShortcutHandler", "a settings object should be set using setSettingsObject() prior to using readSettings()");
    Common::SettingsCategoryGuard guard(m_settings, QLatin1String("ShortcutHandler"));
    const int size = m_settings->beginReadArray(QLatin1String("Shortcuts"));
    for (int i = 0; i < size; ++i) {
        m_settings->setArrayIndex(i);
        const QString actionName = m_settings->value(QLatin1String("Action")).toString();
        m_actionDescriptions[actionName].shortcut = m_settings->value(QLatin1String("Shortcut")).toString();

        QAction *action = m_actionDescriptions[actionName].action;
        if (action)
            action->setShortcut(QKeySequence(m_settings->value(QLatin1String("Shortcut")).toString()));
    }
    m_settings->endArray();
}

} // namespace Gui

#endif // QT_NO_SHORTCUT
