/* Copyright (C) 2014 Dan Chapman <dpniel@ubuntu.com>

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
import QtQuick 2.0
import Ubuntu.Components 1.1
import Ubuntu.Components.ListItems 1.0 as ListItems
import Ubuntu.Components.Popups 1.0
import trojita.models.ThreadingMsgListModel 0.1

ToolbarItems {
    id: messageListToolbar

    // Actions
    Action {
        id: optionButtonAction
        iconSource: Qt.resolvedUrl("./navigation-menu.svg")
        text: qsTr("Options")
        onTriggered: PopupUtils.open(optionComponent, optionButton)
    }

    // Option button
    ToolbarButton {
        id: optionButton
        action: optionButtonAction
    }

    // sort button popover
    Component {
        id: optionComponent
        Popover {
            id: popover
            autoClose: true
            Component.onCompleted: messageListToolbar.locked = true
            Component.onDestruction: messageListToolbar.locked = false
            Column {
                id: containerLayout
                anchors {
                    left: parent.left
                    top: parent.top
                    right: parent.right
                }

                ListItems.ItemSelector {
                    id: sortOrderSelector
                    text: qsTr("Sort Order")
                    model: sortOrderModel
                    // It would be great to get a sort icon to go next to each sort order
                    delegate: OptionSelectorDelegate { text: description }
                    onSelectedIndexChanged: sortAction.trigger()
                    property var sortOrder: {'ASCENDING': 0, 'DESCENDING': 1 }
                    Component.onCompleted: {
                        switch (imapAccess.threadingMsgListModel.currentSortOrder()) {
                        case Qt.AscendingOrder:
                            sortOrderSelector.selectedIndex = sortOrder.ASCENDING
                            break
                        case Qt.DescendingOrder:
                            sortOrderSelector.selectedIndex = sortOrder.DESCENDING
                            break
                        }
                    }
                }
            }

            Action {
                id: sortAction
                onTriggered: {
                    if (sortOrderSelector.model.get(sortOrderSelector.selectedIndex).prop !==
                            imapAccess.threadingMsgListModel.currentSortOrder()) {
                        imapAccess.threadingMsgListModel.setUserSearchingSortingPreference(
                                    [],
                                    ThreadingMsgListModel.SORT_NONE,
                                    sortOrderSelector.model.get(sortOrderSelector.selectedIndex).prop
                                    )
                        PopupUtils.close(popover)
                    }
                }
            }
        }
    }

    ListModel {
        id: sortOrderModel
        ListElement { description: QT_TR_NOOP("Ascending"); prop: Qt.AscendingOrder }
        ListElement { description: QT_TR_NOOP("Descending"); prop: Qt.DescendingOrder }
    }
}
