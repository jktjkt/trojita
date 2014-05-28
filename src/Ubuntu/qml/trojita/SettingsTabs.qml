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
import Ubuntu.Components.Popups 1.0
import trojita.MSA.Account 0.1

// We should try and keep the same settings tabs as desktop
// so we should have profiles, SMTP, offline etc in the future

Tabs {
    id: settingsTabs
    visible: false
    // during the password saving there is a short period where password is an empty string and trying to
    // either using the binding to determine if password exists or reloadPassword cause undesired events.
    // This helps to determine if we need to show error dialog on saving failed signals
    property bool passwordGiven

    Component.onCompleted: {
        // use the passwordWatcher savingDone signal to trigger saving the rest and ensure we
        // have any given password available when connecting.
        imapAccess.passwordWatcher.savingDone.connect(continueSavingAction.trigger)
        imapAccess.passwordWatcher.savingFailed.connect(showSavingPasswordFailed)
    }

    function showSavingPasswordFailed(message) {
        // if no passwordGiven then we can safely just continue and ignore
        // the error message
        if (!passwordGiven) {
            continueSavingAction.trigger()
        } else {
            PopupUtils.open(Qt.resolvedUrl("InfoDialog.qml"), appWindow, {
                                title: qsTr("Failed saving password"),
                                text: message,
                                buttonText: qsTr("Continue"),
                                buttonAction: continueSavingAction
                            })
        }
    }

    Action {
        id: continueSavingAction
        onTriggered: {
            imapSettings.saveImapSettings()
            if (!imapSettings.settingsModified) {
                pageStack.pop()
            } else {
                imapSettings.imapSettingsChanged()
                imapAccess.doConnect()
                appWindow.connectModels()
            }
            // update state
            imapSettings.state = "HAS_ACCOUNT"
            imapSettings.settingsModified = false

        }
    }

    Action {
        id: cancelAction
        onTriggered: {
            smtpAccountSettings.restoreSettings()
            pageStack.pop()
        }
    }

    Action {
        id: saveAction
        onTriggered: {
            if (!imapSettings.settingsValid) {
                settingsTabs.selectedTabIndex = 0
            } else if (!smtpSettings.settingsValid) {
                settingsTabs.selectedTabIndex = 1
            } else {
                smtpAccountSettings.saveSettings()
                passwordGiven = imapSettings.imapPassword
                imapAccess.passwordWatcher.setPassword(imapSettings.imapPassword)
            }
        }
    }

    SettingsToolbar {
        id: settingsToolbar
        onCancel: cancelAction.trigger()
        onSave: saveAction.trigger()
    }

    // XXX: Should we be using Loaders here? since we are going to be pushing
    // multiple created tabs at the same time. Does this cause any performance
    // hit, as we are already ontop of the created mailbox view? Also we 'should' lazily load this component.

    // ATM with just these two tabs the performance is still responsive with no visible lag, so maybe
    // re-evaluate after more tabs have been added. SmtpAccountSettings.cpp has already been setup to support
    // using loaders, as the pages will get loaded and unloaded during tab switches
    // we have to retain the settings state for that page without saving it to QSettings.
    // So an explicit 'Save' function needs to be called to write it to file. Should we do this for imapaccess??

    Tab {
        id: imapSettingsTab
        title: qsTr("IMAP Settings")
        page: ImapSettings {
            id: imapSettings
            visible: true
            tools: settingsToolbar
        }
    }

    Tab {
        id: smtpSettingsTab
        title: qsTr("SMTP Settings")
        page: SmtpSettings {
            id: smtpSettings
            visible: true
            // For some reason we need to bind these here to avoid a binding loop. WHY???
            // this may become an issue if we do use Loaders
            smtpEncryptionModelIndex: {
                switch (smtpAccountSettings.submissionMethod){
                case MSAAccount.SMTP_STARTTLS:
                    1
                    break
                case MSAAccount.SSMTP:
                    2
                    break
                default:
                    0
                }
            }
            smtpPathToSendmail: smtpAccountSettings.pathToSendmail
            smtpAuthenticate: smtpAccountSettings.authenticateEnabled
            smtpBurl: smtpAccountSettings.useBurl
            smtpUserName: smtpAccountSettings.username ? smtpAccountSettings.username : ""
            smtpServer: smtpAccountSettings.server ? smtpAccountSettings.server : ""
            smtpPort: smtpAccountSettings.port
            smtpSentLocation: smtpAccountSettings.sentMailboxName ? smtpAccountSettings.sentMailboxName : ""
            smtpSaveToImap: smtpAccountSettings.saveToImap
            tools: settingsToolbar
        }
    }
}

