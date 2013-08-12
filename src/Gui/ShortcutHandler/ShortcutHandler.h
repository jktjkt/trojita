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

#ifndef SHORTCUTHANDLER_H
#define SHORTCUTHANDLER_H

#ifndef QT_NO_SHORTCUT

#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QStringList>

class QAction;
class QSettings;
class QWidget;

namespace Gui
{

class ShortcutConfigDialog;
class ShortcutConfigWidget;

class ActionDescription
{
public:
    explicit ActionDescription(const QString &iconName = 0, const QString &text = 0, const QString &defaultShortcut = 0, const QString &parentId = QObject::tr("Main Window")) : iconName(iconName), text(text), defaultShortcut(defaultShortcut), shortcut(defaultShortcut), parentId(parentId) {}
    QString iconName;
    QString text;
    QString defaultShortcut;
    QString shortcut;
    QString parentId;
    QPointer<QAction> action;
};

/**
 * @short Class for handling the configuration of keyboard shortcuts.
 *
 * This class is used for handling the configuration of keyboard shortcuts
 * for actions defined with QAction.
 *
 * \b Initialization \n
 *
 * Only one ShortcutHandler instance can be created during the lifetime of
 * your application. Typically this is done in the constructor of your main
 * window as follows:
 *
 * \code
 * ShortcutHandler *shortcutHandler = new ShortcutHandler(parentWidget);
 * QSettings *settingsObject = new QSettings(parentWidget);
 * shortcutHandler->setSettingsObject(settingsObject);
 * \endcode
 *
 * where parentWidget is the widget which will serve as the parent widget
 * for the shortcut configuration dialog (typically this is the main window
 * of your application). The shortcut configuration dialog will be modal
 * w.r.t. parentWidget. Specifying the parent in this way also ensures that
 * the ShortcutHandler object itself will be deleted when parentWidget is
 * deleted.
 *
 * In the example above, a QSettings object is defined and passed to the
 * ShortcutHandler instance using setSettingsObject(QSettings*). This is
 * required. This allows you to save the shortcut configuration in the
 * same place as the other configuration settings of your program. Note that
 * ShortcutHandler doesn't take over responsibility of the QSettings object,
 * which should therefore be deleted by the caller. The QSettings object
 * passed to ShortcutHandler should exist during the entire lifetime of the
 * ShortcutHandler object, and should therefore not be deleted before the
 * ShortcutHandler object is deleted.
 *
 * \b Making the shortcuts of your actions configurable \n
 *
 * In order to make a shortcut configurable, the QAction which has this
 * shortcut must be added to the ShortcutHandler instance using
 * addAction(QAction*) or addAction(QAction*, const QString&). For example:
 *
 * \code
 * QAction *quitAction = new QAction(tr("&Quit"), this);
 * quitAction->setShortcut(tr("Ctrl+Q"));
 * quitAction->setObjectName("action_application_exit");
 * ShortcutHandler::instance()->addAction(quitAction);
 * \endcode
 *
 * This code can be used in any class in which you have included the
 * shortcuthandler.h header file. It doesn't matter in which way the
 * shortcut of the action is defined (e.g. the initial shortcut could
 * also have been set to QKeySequence::Open) and the shortcut handling
 * even works if there is no initial shortcut set. Note that if there
 * is a shortcut set before the action is added to the ShortcutHandler
 * object, then it will serve as the default shortcut in the configuration
 * dialog. Note that it is compulsary to set the object name of the action
 * with setObjectName(const QString&) before the action is added to the
 * ShortcutHandler object, because the object name of the action (which
 * should be unique throughout the whole application) is used to store the
 * shortcut in the settings on disk. The action can be safely removed
 * during the lifetime of the ShortcutHandler instance. Actions can be
 * added to the ShortcutHandler instance at any moment during its lifetime,
 * even after the configuration widget has already been shown (the new
 * actions will be visible in the configuration widget when it is shown
 * again).
 *
 * The second argument to addAction(QAction*, const QString&) sets the
 * label under which the action is classified in the shortcut configuration
 * dialog. If this label is not set, then it defaults to
 * QObject::tr("Main Window").
 *
 * After all QActions are initialized and the appropriate ones are added
 * to ShortcutHandler, the following code must be run in order to read
 * previous modifications to the shortcuts from disk:
 *
 * \code
 * ShortcutHandler::instance()->readSettings();
 * \endcode
 *
 * \b Making use of predefined shortcuts \n
 *
 * Instead of first creating actions and then adding them to ShortcutHandler,
 * you can predefine shortcuts using
 * defineAction(const QString&, const QString&, const QString&, const QString&, const QString&) or
 * defineAction(const QString&, const QString&, const QString&, const QKeySequence::StandardKey&, const QString&),
 * after which you create the actions for the predefined shortcuts with
 * createAction(const QString&, QWidget*) or
 * createAction(const QString&, QObject*, const char*, QWidget*).
 * The first argument of both createAction() functions is the name of an action as known
 * by ShortcutHandler. The last argument of both functions is the widget
 * that will be the parent of the action. The second function has two
 * additional arguments which define the receiver of the actions's
 * triggered() signal and the slot which is executed when this signal is
 * triggered. For example, in the constructor of the main window you can use:
 *
 * \code
 * ShortcutHandler::instance()->defineAction("action_application_exit", "application-exit", tr("E&xit"), QKeySequence::Quit, tr("Main Window"));
 * ShortcutHandler::instance()->defineAction("action_toggle_document_editable", "document-edit", tr("&Set/Unset Document Editable"), tr("Ctrl+E"), tr("Main Window"));
 * \endcode
 *
 * and anywhere in the program you can define:
 *
 * \code
 * QAction *quitAction = ShortcutHandler::instance()->createAction("action_application_exit", this, SLOT(slotQuitApplication()), this);
 * \endcode
 *
 * This code creates an action with the shortcut, text and icon defined for
 * "action_application_exit" in ShortcutHandler. If no text or icon is defined
 * for "action_application_exit", quitAction will be equal to 0. This action
 * will call the slotQuitApplication() slot defined in "this" (the second
 * argument) and has "this" (the last argument) as parent. If the action
 * should be checkable, then the following code can be used:
 *
 * \code
 * QAction *toggleEditableAction = ShortcutHandler::instance()->createAction("action_toggle_document_editable", this);
 * toggleEditableAction->setCheckable();
 * connect(toggleEditableAction, SIGNAL(triggered(bool)), this, SLOT(slotToggleEditable(bool)));
 * \endcode
 *
 * In both cases, if an action is created with createAction(), it can be
 * accessed later with action(const QString&). For example:
 *
 * \code
 * toolBar->addAction(ShortcutHandler::instance()->action("action_toggle_document_editable"));
 * \endcode
 *
 * After all actions are defined using defineAction(), the following code
 * must be run in order to read previous modifications to the shortcuts
 * from disk:
 *
 * \code
 * ShortcutHandler::instance()->readSettings();
 * \endcode
 *
 * \b Accessing the shortcut configuration dialog \n
 *
 * In order to make the shortcut configuration dialog accessible to the
 * users of your program, an action can be added to a menu or toolbar.
 * This action can be obtained with
 * ShortcutHandler::instance()->shortcutConfigAction()
 * anywhere in your program. For example:
 *
 * \code
 * QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
 * settingsMenu->addAction(ShortcutHandler::instance()->shortcutConfigAction());
 * \endcode
 *
 * This code adds an item labeled "Configure Shortcuts" (or a localized
 * version of this) to settingsMenu, which when triggered shows the shortcut
 * configuration dialog. The shortcut configuration dialog is deleted when
 * the parent widget of the ShortcutHandler instance is deleted.
 *
 * Warning: this approach cannot be combined with the following approach.
 *
 * \b Adding the shortcut configuration widget to an existing dialog \n
 *
 * Alternatively to adding an action which opens a shortcut configuration
 * dialog as described above, it is possible to add a shortcut configuration
 * widget to an existing configuration dialog. For example, in the constructor
 * of a configuration dialog one can have:
 *
 * \code
 * QVBoxLayout *mainLayout = new QVBoxLayout;
 * mainLayout->addWidget(ShortcutHandler::instance()->configWidget());
 * setLayout(mainLayout);
 * \endcode
 *
 * You can use ShortcutHandler::instance()->configWidget() multiple times,
 * but this will always refer to the same widget. The shortcut configuration
 * widget is deleted when the parent widget of the ShortcutHandler is deleted
 * (or when its new parent is deleted if the widget is reparented).
 *
 * In order to save the modified shortcuts to disk, it is necessary to call
 * ShortcutHandler::instance()->accept() each time the configuration dialog
 * to which this widget is added is accepted. This can happen for example
 * in the configuration dialog's own accept() function.
 *
 * Warning: this approach cannot be combined with the previous approach:
 * either you add an action with shortcutConfigAction() (which shows a
 * standalone shortcut configuration dialog) or you add a widget with
 * configWidget(). Using both is not possible.
 *
 * \b Exclusivity groups \n
 *
 * By default all shortcuts managed by ShortcutHandler should be different.
 * ShortcutHandler does however not test initially if this condition is
 * satisfied. When the user attempts to set a shortcut which is already in
 * use, a warning is displayed and the possibility is given to unset the
 * shortcut for the action that had the shortcut previously. Sometimes it
 * is however useful to be able to use the same shortcut in different
 * situations, for example when the application has two dialogs which can
 * never be shown together, or when the application has a full screen mode
 * effectively hiding the default window of the application. In order to
 * be able to use the same shortcuts in such situations, exclusivity groups
 * can be defined. For example, the code
 *
 * \code
 * ShortcutHandler::instance()->addExclusivityGroup(QStringList() << tr("Presentation"));
 * ShortcutHandler::instance()->addExclusivityGroup(QStringList() << tr("LaTeX commands") << tr("Math commands"));
 * \endcode
 *
 * defines two exclusivity groups. The first one contains one element
 * ("Presentation"), the second one contains two elements ("LaTeX commands"
 * and "Math commands"). The elements must be labels which are passed
 * previously to addAction(QAction*, const QString&) as the second argument.
 * Actions having the label "Presentation" will then be required to
 * have unique shortcuts, no two actions which have either one of the
 * labels "LaTeX commands" and "Math commands" can have the same shortcuts.
 * On the other hand, actions with label "Presentation" can have the
 * same shortcuts as actions with label "LaTeX commands" or "Math commands".
 *
 * By default, there exists a label "Main Window" which belongs to its own
 * exclusivity group. This label only exists when actions are added with
 * addAction(QAction*, const QString&) without the second argument being
 * specified (the corresponding exclusivity group always exists, but is
 * ignored in this case). It is possible to add other labels to this
 * exclusivity group by specifying for example:
 *
 * \code
 * ShortcutHandler::instance()->addExclusivityGroup(QStringList() << tr("Main Window") << tr("Presentation"));
 * \endcode
 *
 * This code adds "Presentation" to the exclusivity group containing
 * "Main Window".
 *
 * If no exclusivity groups are defined, all labels are added to the same
 * exclusivity group (which is the group containing "Main Window").
 *
 * Note that adding an exclusivity group can only be done \b after \n all
 * necessary labels are added to the shortcut handler using
 * addAction(QAction*, const QString&) where the second argument is the
 * label which is added to the exclusivity group. If a label is added to
 * two or more exclusivity groups in subsequent calls of
 * addExclusivityGroup(), then the label will only belong to the last
 * defined exclusivity group.
 */
class ShortcutHandler : public QObject
{
    Q_OBJECT

public:
    explicit ShortcutHandler(QWidget *parent);
    ~ShortcutHandler();
    static ShortcutHandler *instance() { return self; }

    void setSettingsObject(QSettings *settings);
    /**
     * Defines an action with a shortcut. All actions defined using this
     * function will be visible in the shortcut configuration widget.
     * \param actionName the name under which the action is known to the shortcut configuration system
     * \param iconName the name of the icon displayed when the action is added to a menu or toolbar. If iconName is an empty string, then no icon is shown.
     * \param text the text visible on the item in the menu or toolbar corresponding to this action
     * \param defaultShortcut the (localized) string describing the default shortcut which triggers this action, e.g. tr("Ctrl+E")
     * \param parentId the title under which this action is shown in the shortcut configuration widget. In the shortcut configuration widget, the actions are grouped according to parentId.
     */
    void defineAction(const QString &actionName, const QString &iconName, const QString &text, const QString &defaultShortcut = 0, const QString &parentId = QObject::tr("Main Window"));
    /**
     * Defines an action with a shortcut. All actions defined using this
     * function will be visible in the shortcut configuration widget.
     * \param actionName the name under which the action is known to the shortcut configuration system
     * \param iconName the name of the icon displayed when the action is added to a menu or toolbar. If iconName is an empty string, then no icon is shown.
     * \param text the text visible on the item in the menu or toolbar corresponding to this action
     * \param key the QKeySequence::StandardKey value describing the default shortcut which triggers this action, e.g. QKeySequence::Quit
     * \param parentId the title under which this action is shown in the shortcut configuration widget. In the shortcut configuration widget, the actions are grouped according to parentId.
     */
    void defineAction(const QString &actionName, const QString &iconName, const QString &text, const QKeySequence::StandardKey &key, const QString &parentId = QObject::tr("Main Window"));
    QAction *createAction(const QString &actionName, QWidget *parent = 0);
    QAction *createAction(const QString &actionName, QObject *receiver, const char *member, QWidget *parent = 0);
    void addExclusivityGroup(const QStringList &group);
    QSettings *settingsObject();
    QAction *action(const QString &actionName);
    QAction *shortcutConfigAction();
    ShortcutConfigWidget *configWidget();
    void accept();
    void reject();
    void readSettings();

private Q_SLOTS:
    void changeShortcuts(const QHash<QString, ActionDescription> &actionDescriptions);
    void openShortcutConfigDialog();

private:
    static ShortcutHandler *self;

    QSettings *m_settings;
    QPointer<QAction> m_shortcutConfigAction;
    QPointer<ShortcutConfigDialog> m_shortcutConfigDialog;
    QPointer<ShortcutConfigWidget> m_shortcutConfigWidget;

    QHash<QString, ActionDescription> m_actionDescriptions;
    QList<QStringList> m_exclusivityGroups;
};

} // namespace Gui

#endif // QT_NO_SHORTCUT

#endif // SHORTCUTHANDLER_H
