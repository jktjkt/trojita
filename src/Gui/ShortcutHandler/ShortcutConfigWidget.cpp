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

#include "ShortcutConfigWidget.h"

#ifndef QT_NO_SHORTCUT

#include <QKeyEvent>
#include <QMessageBox>
#include <QSettings>

#include "IconLoader.h"
#include "ShortcutHandler.h"
#include "Common/SettingsCategoryGuard.h"

namespace Gui
{

ShortcutConfigWidget::ShortcutConfigWidget(QWidget *parent)
    : QWidget(parent)
    , m_shortcutsShouldBeRestored(false)
{
    ui.setupUi(this);
    setWindowTitle(tr("Configure Shortcuts") + QLatin1String(" - ") + trUtf8("Trojitá"));

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    ui.shortcutTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui.shortcutTreeWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
#else
    ui.shortcutTreeWidget->header()->setResizeMode(0, QHeaderView::ResizeToContents);
    ui.shortcutTreeWidget->header()->setResizeMode(1, QHeaderView::Stretch);
#endif
    ui.shortcutTreeWidget->setUniformRowHeights(true); // all rows have the same height
    ui.shortcutTreeWidget->installEventFilter(this);

    ui.gridLayout->setColumnStretch(2, 1); // make sure the buttons are not too large
    ui.gridLayout->setContentsMargins(0, 0, 0, 0);

    setFocusProxy(ui.shortcutTreeWidget);

    connect(ui.searchLineEdit, SIGNAL(textChanged(QString)), this, SLOT(searchItems(QString)));
    connect(ui.clearPushButton, SIGNAL(clicked()), this, SLOT(clearShortcut()));
    connect(ui.useDefaultPushButton, SIGNAL(clicked()), this, SLOT(restoreDefaultShortcut()));
}

ShortcutConfigWidget::~ShortcutConfigWidget()
{
}

QPushButton *ShortcutConfigWidget::clearButton()
{
    return ui.clearPushButton;
}

QPushButton *ShortcutConfigWidget::useDefaultButton()
{
    return ui.useDefaultPushButton;
}

/***************************************************************************/

void ShortcutConfigWidget::setExclusivityGroups(const QList<QStringList> &groups)
{
    // this function must be called after all actions are added
    // we set a unique ID for each exclusivity group and we set the data of each toplevel item to the ID of the group to which it belongs
    // the ID must be a negative number, since eventFilter() assumes that the ID is either a negative number or the index of the corresponding action in m_actions
    m_exclusivityGroups = groups;
    const int topLevelItemCount = ui.shortcutTreeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelItemCount; ++i) {
        QTreeWidgetItem *topLevelItem = ui.shortcutTreeWidget->topLevelItem(i);
        const QString parentId = topLevelItem->text(0);
        for (int j = 0; j < m_exclusivityGroups.size(); ++j)
            if (m_exclusivityGroups.at(j).contains(parentId))
                topLevelItem->setData(1, Qt::UserRole, -j-1);
    }
}

void ShortcutConfigWidget::addItem(const QString &actionName, const QString &text, const QString &shortcut, const QIcon &icon, const QString &parentId)
{
    // search correct toplevel item
    int topLevelItemNumber = -1;
    const int topLevelItemCount = ui.shortcutTreeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelItemCount; ++i) {
        if (ui.shortcutTreeWidget->topLevelItem(i)->text(0) == parentId) {
            topLevelItemNumber = i;
            break;
        }
    }
    if (topLevelItemNumber < 0) { // toplevel item with name parentId doesn't exist yet, so create
        QTreeWidgetItem *item = new QTreeWidgetItem(ui.shortcutTreeWidget);
        item->setText(0, parentId);
        topLevelItemNumber = topLevelItemCount;
        item->setSizeHint(1, QSize(0, 1.5 * qApp->fontMetrics().height())); // since the second column is stretchable, it doesn't matter that the width of the size hint is set to 0
        item->setData(1, Qt::UserRole, QString());
    }

    // create item
    QString textWithoutAccelerator = text;
    QTreeWidgetItem *item = new QTreeWidgetItem(ui.shortcutTreeWidget->topLevelItem(topLevelItemNumber));
    item->setText(0, textWithoutAccelerator.remove(QLatin1Char('&')));
    item->setIcon(0, icon);
    item->setText(1, shortcut);
    item->setData(1, Qt::UserRole, actionName); // store objectName of the current action
}

void ShortcutConfigWidget::setActionDescriptions(const QHash<QString, ActionDescription> &actionDescriptions)
{
    m_actionDescriptions = actionDescriptions;
    ui.shortcutTreeWidget->scrollToItem(ui.shortcutTreeWidget->invisibleRootItem()->child(0));
    ui.shortcutTreeWidget->clear();
    for (QHash<QString, ActionDescription>::const_iterator it = m_actionDescriptions.constBegin(); it != m_actionDescriptions.constEnd(); ++it) {
        ActionDescription actionDescription = it.value();
        addItem(it.key(), actionDescription.text, actionDescription.shortcut, loadIcon(actionDescription.iconName), actionDescription.parentId);
    }
    ui.shortcutTreeWidget->expandAll();
}

/***************************************************************************/

void ShortcutConfigWidget::searchItems(const QString &text)
{
    const int topLevelItemCount = ui.shortcutTreeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelItemCount; ++i) {
        QTreeWidgetItem *topLevelItem = ui.shortcutTreeWidget->topLevelItem(i);
        const int childCount = topLevelItem->childCount();
        for (int j = 0; j < childCount; ++j) {
            QTreeWidgetItem *childItem = topLevelItem->child(j);
            if (childItem->text(0).contains(text, Qt::CaseInsensitive) || childItem->text(1).contains(text, Qt::CaseInsensitive))
                childItem->setHidden(false);
            else
                childItem->setHidden(true);
        }
    }
}

/***************************************************************************/

bool ShortcutConfigWidget::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    if (event->type() != QEvent::KeyPress)
        return false;

    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    QString keySequence;
    // don't allow modifiers to be handled as single keys and skip CapsLock and NumLock
    if (keyEvent->key() == Qt::Key_Control || keyEvent->key() == Qt::Key_Shift
            || keyEvent->key() == Qt::Key_Meta || keyEvent->key() == Qt::Key_Alt
            || keyEvent->key() == Qt::Key_Super_L || keyEvent->key() == Qt::Key_AltGr
            || keyEvent->key() == Qt::Key_CapsLock || keyEvent->key() == Qt::Key_NumLock)
        return false;
    // skip some particular keys (note that Qt::Key_Up and friends are used to navigate the list, in order to avoid that they interfere with the shortcut changing, we skip them)
    if (keyEvent->modifiers() == Qt::NoModifier
            && (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down
                || keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right
                || keyEvent->key() == Qt::Key_PageUp || keyEvent->key() == Qt::Key_PageDown
                || keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab
                || keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Escape))
        return false;
    if (keyEvent->key() == Qt::Key_Backtab) // the above doesn't catch "Shift+Tab"
        return false;

    // create string representation of the new shortcut
    if (keyEvent->modifiers() & Qt::ControlModifier)
        keySequence += tr("Ctrl+", "Shortcut modifier");
    if (keyEvent->modifiers() & Qt::AltModifier)
        keySequence += tr("Alt+", "Shortcut modifier");
    if (keyEvent->modifiers() & Qt::ShiftModifier)
        keySequence += tr("Shift+", "Shortcut modifier");
    keySequence += QKeySequence(keyEvent->key()).toString(QKeySequence::PortableText);

    // replace shortcut in the list (but not yet for real, this is done when the user accepts the dialog)
    QTreeWidgetItem *currentItem = ui.shortcutTreeWidget->currentItem();
    if (!currentItem) // this is the case when ui.shortcutTreeWidget is empty
        return false;
    const QString actionName = currentItem->data(1, Qt::UserRole).toString();
    if (actionName.isEmpty()) // this is the case when a toplevel item is selected
        return false;
    // test whether the new shortcut is already defined for another action, if yes then ask the user to set an empty shortcut for the old action
    const QVariant parentId = currentItem->parent()->data(1, Qt::UserRole);
    const int topLevelItemCount = ui.shortcutTreeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelItemCount; ++i) {
        QTreeWidgetItem *topLevelItem = ui.shortcutTreeWidget->topLevelItem(i);
        if (topLevelItem->data(1, Qt::UserRole) != parentId) // only deal with actions in the same exclusivity group
            continue;
        const int childCount = topLevelItem->childCount();
        for (int j = 0; j < childCount; ++j) {
            QTreeWidgetItem *childItem = topLevelItem->child(j);
            if (keySequence == childItem->text(1) && childItem->data(1, Qt::UserRole).toString() != actionName) {
                QMessageBox::StandardButton result = QMessageBox::warning(this,
                                                     tr("Shortcut Conflicts") + QLatin1String(" - ") + trUtf8("Trojitá"),
                                                     tr("<p>The \"%1\" shortcut is ambiguous with the following shortcut:</p>"
                                                        "<p>%2</p><p>Do you want to assign an empty shortcut to this action?</p>")
                                                     .arg(keySequence).arg(childItem->text(0)),
                                                     QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
                if (result == QMessageBox::Ok)
                    childItem->setText(1, QString());
                else
                    return false;
            }
        }
    }
    // finally we can set the new shortcut
    currentItem->setText(1, keySequence);
    return true;
}

void ShortcutConfigWidget::clearShortcut()
{
    QTreeWidgetItem *currentItem = ui.shortcutTreeWidget->currentItem();
    const QString actionName = currentItem->data(1, Qt::UserRole).toString();
    if (actionName.isEmpty())
        return;
    currentItem->setText(1, QString());
}

void ShortcutConfigWidget::restoreDefaultShortcut()
{
    QTreeWidgetItem *currentItem = ui.shortcutTreeWidget->currentItem();
    const QString actionName = currentItem->data(1, Qt::UserRole).toString();
    if (actionName.isEmpty())
        return;
    currentItem->setText(1, m_actionDescriptions[actionName].defaultShortcut);
}

/***************************************************************************/

void ShortcutConfigWidget::accept()
{
    // set shortcuts for real (but they are not yet saved in the settings on disk, this is done in writeSettings())
    const int topLevelItemCount = ui.shortcutTreeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelItemCount; ++i) {
        QTreeWidgetItem *topLevelItem = ui.shortcutTreeWidget->topLevelItem(i);
        const int childCount = topLevelItem->childCount();
        for (int j = 0; j < childCount; ++j) {
            QTreeWidgetItem *childItem = topLevelItem->child(j);
            const QString actionName = childItem->data(1, Qt::UserRole).toString();
            m_actionDescriptions[actionName].shortcut = childItem->text(1);
        }
    }
    writeSettings();
    Q_EMIT(shortcutsChanged(m_actionDescriptions));
}

void ShortcutConfigWidget::reject()
{
    if (!m_shortcutsShouldBeRestored)
        return;

    // restore unsaved shortcuts in the tree widget
    const int topLevelItemCount = ui.shortcutTreeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelItemCount; ++i) {
        QTreeWidgetItem *topLevelItem = ui.shortcutTreeWidget->topLevelItem(i);
        const int childCount = topLevelItem->childCount();
        for (int j = 0; j < childCount; ++j) {
            QTreeWidgetItem *childItem = topLevelItem->child(j);
            const QString actionName = childItem->data(1, Qt::UserRole).toString();
            childItem->setText(1, m_actionDescriptions[actionName].shortcut);
        }
    }
    m_shortcutsShouldBeRestored = false;
}

void ShortcutConfigWidget::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);
    reject(); // restore unsaved shortcuts in the tree widget before the configuration dialog is shown again
    m_shortcutsShouldBeRestored = true;
    ui.shortcutTreeWidget->sortByColumn(0, Qt::AscendingOrder);
}

/***************************************************************************/

/**
 * \see ShortcutHandler::readSettings()
 */
void ShortcutConfigWidget::writeSettings()
{
    QSettings *settings = ShortcutHandler::instance()->settingsObject();
    Q_ASSERT_X(settings, "ShortcutHandler", "no QSettings object found: a settings object should first be set using setSettingsObject() and then readSettings() should be called when initializing your program; note that this QSettings object should exist during the entire lifetime of the ShortcutHandler object and therefore not be deleted first");
    Common::SettingsCategoryGuard guard(settings, QLatin1String("ShortcutHandler"));
    settings->remove(QString());
    settings->beginWriteArray(QLatin1String("Shortcuts"));
    int index = 0;
    for (QHash<QString, ActionDescription>::const_iterator it = m_actionDescriptions.constBegin(); it != m_actionDescriptions.constEnd(); ++it) {
        ActionDescription actionDescription = it.value();
        const QString shortcut = actionDescription.shortcut;
        if (shortcut != actionDescription.defaultShortcut) {
            settings->setArrayIndex(index);
            settings->setValue(QLatin1String("Action"), it.key());
            settings->setValue(QLatin1String("Shortcut"), actionDescription.shortcut);
            ++index;
        }
    }
    settings->endArray();
}

} // namespace Gui

#endif // QT_NO_SHORTCUT
