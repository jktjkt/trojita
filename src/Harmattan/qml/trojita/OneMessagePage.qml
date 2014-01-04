/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1
import net.flaska.QNAMWebView 1.0
import "Utils.js" as Utils

Page {
    property string mailbox
    property alias url: messageView.url

    function handleChangedEnvelope() {
        if (status === PageStatus.Active && !imapAccess.oneMessageModel.hasValidIndex)
            appWindow.showHome()
    }

    id: oneMessagePage

    tools: oneMailTools

    Item {
        anchors {left: parent.left; right: parent.right; bottom: parent.bottom; top: header.bottom}

        Flickable {
            id: view
            anchors.fill: parent
            contentHeight: col.height
            contentWidth: col.width

            Column {
                id: col

                AddressWidget {
                    caption: qsTr("From")
                    address: imapAccess.oneMessageModel.from
                    width: view.width
                }
                AddressWidget {
                    caption: qsTr("To")
                    address: imapAccess.oneMessageModel.to
                    width: view.width
                }
                AddressWidget {
                    caption: qsTr("Cc")
                    address: imapAccess.oneMessageModel.cc
                    width: view.width
                }
                AddressWidget {
                    caption: qsTr("Bcc")
                    address: imapAccess.oneMessageModel.bcc
                    width: view.width
                }

                Label {
                    id: dateLabel
                    width: view.width
                    text: qsTr("<b>Date:</b> ") + Utils.formatDateDetailed(imapAccess.oneMessageModel.date)
                }

                Label {
                    id: subjectLabel
                    width: view.width
                    wrapMode: Text.Wrap
                    text: qsTr("<b>Subject:</b> ") + imapAccess.oneMessageModel.subject
                }

                QNAMWebView {
                    id: messageView
                    networkAccessManager: imapAccess.msgQNAM

                    preferredWidth: view.width

                    // Without specifying the width here, plaintext e-mails would cause useless horizontal scrolling
                    width: parent.width

                    // Setting the URL from here would not be enough, we really want to force a reload whenever the message changes,
                    // even though the URL might remain the same

                    settings.userStyleSheetUrl: "data:text/css;charset=utf-8;base64," +
                                                Qt.btoa("* {color: white; background: black; font-size: " +
                                                        UiConstants.BodyTextFont.pixelSize + "px;};")
                }

                // FIXME: move this to a dedicated page...
                Component {
                    id: attachmentItemDelegate

                    Label {
                        id: lbl
                        text: "Attachment " + (model.fileName.length ? model.fileName + " " : "") + "(" + model.mimeType +
                              (model.size ?
                                   "): " + imapAccess.prettySize(model.size) :
                                   ")")
                        width: attachmentsView.width
                        height: 40
                    }
                }

                ListView {
                    id: attachmentsView
                    interactive: false
                    width: view.width
                    // FIXME: magic constants...
                    height: count * 40 + 30
                    // FIXME: filter out the main part from the view (in C++, of course)
                    model: imapAccess.oneMessageModel.attachmentsModel
                    delegate: attachmentItemDelegate
                }
            }
        }

        ScrollDecorator {
            flickableItem: view
        }
    }

    PageHeader {
        id: header
        text: imapAccess.oneMessageModel.subject
        anchors {left: parent.left; right: parent.right; top: parent.top}
    }

    ToolBarLayout {
        id: oneMailTools
        visible: true

        BackButton {}

        ToggleableToolIcon {
            id: messageReadButton
            toggled: imapAccess.oneMessageModel.isMarkedRead
            iconSource: "image://theme/icon-m-toolbar-done-white" + (toggled ? "-selected" : "")
            onClicked: imapAccess.oneMessageModel.isMarkedRead = !imapAccess.oneMessageModel.isMarkedRead
        }

        ToggleableToolIcon {
            id: messageDeleteButton
            toggled: imapAccess.oneMessageModel.isMarkedDeleted
            iconSource: "image://theme/icon-m-toolbar-delete-white" + (toggled ? "-selected" : "")
            onClicked: imapAccess.oneMessageModel.isMarkedDeleted = !imapAccess.oneMessageModel.isMarkedDeleted
        }

        NetworkPolicyButton {}
    }

    Component.onCompleted: imapAccess.oneMessageModel.envelopeChanged.connect(handleChangedEnvelope)
    Component.onDestruction: imapAccess.oneMessageModel.envelopeChanged.disconnect(handleChangedEnvelope)
}
