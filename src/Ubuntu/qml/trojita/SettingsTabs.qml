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
import Ubuntu.Components 0.1
import trojita.models.ThreadingMsgListModel 0.1

Tabs {
    id: settingsTabs
    visible: false
    // We should try and keep the same settings tabs as desktop
    // so we should have profiles, SMTP, offline etc in the future

    Action {
        id: cancelAction
        onTriggered: pageStack.pop()
    }

    Action {
        id: saveAction
        onTriggered: {
            if (imapSettings.inputValid) {
                imapSettings.saveImapSettings()

                if (!imapSettings.settingsModified) {
                    pageStack.pop()
                } else {
                    imapSettings.imapSettingsChanged()
                    imapAccess.doConnect()
                    appWindow.connectModels()
                    imapAccess.threadingMsgListModel.setUserSearchingSortingPreference([], ThreadingMsgListModel.SORT_NONE, Qt.DescendingOrder)

                }
                // if the mailbox view indexValid binding doesn't
                // push the page then we do it manually
                if (pageStack.currentPage !== mailboxList) {
                    pageStack.pop()
                }
                // update state
                imapSettings.state = "HAS_ACCOUNT"
                imapSettings.settingsModified = false
            }
        }
    }

    Tab {
        id: imapSettingsTab
        title: qsTr("IMAP Settings")
        page: ImapSettings {
            id: imapSettings
            visible: true
            tools: SettingsToolbar {
                onCancel: cancelAction.trigger()
                onSave: saveAction.trigger()
                saveVisible: imapSettings.inputValid
            }
        }
    }
}

