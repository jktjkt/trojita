import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1
import net.flaska.QNAMWebView 1.0
import "Utils.js" as Utils

Page {
    property string mailbox
    property alias url: messageView.url
    id: oneMessagePage

    tools: commonTools

    onStatusChanged: {
        if (status == PageStatus.Inactive) {
            oneMessagePage.destroy()
            fwdOnePage = null
        } else {
            fwdOnePage = oneMessagePage
        }
    }

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
                    address: imapAccess.oneMessageModel ? imapAccess.oneMessageModel.from : undefined
                    width: view.width
                }
                AddressWidget {
                    caption: qsTr("To")
                    address: imapAccess.oneMessageModel ? imapAccess.oneMessageModel.to : undefined
                    width: view.width
                }
                AddressWidget {
                    caption: qsTr("Cc")
                    address: imapAccess.oneMessageModel ? imapAccess.oneMessageModel.cc : undefined
                    width: view.width
                }
                AddressWidget {
                    caption: qsTr("Bcc")
                    address: imapAccess.oneMessageModel ? imapAccess.oneMessageModel.bcc : undefined
                    width: view.width
                }

                Label {
                    id: dateLabel
                    width: view.width
                    text: imapAccess.oneMessageModel ? qsTr("<b>Date:</b> ") + Utils.formatDateDetailed(imapAccess.oneMessageModel.date) : ""
                }

                Label {
                    id: subjectLabel
                    width: view.width
                    wrapMode: Text.Wrap
                    text: imapAccess.oneMessageModel ? qsTr("<b>Subject:</b> ") + imapAccess.oneMessageModel.subject : ""
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
                    model: imapAccess.oneMessageModel ? imapAccess.oneMessageModel.attachmentsModel : undefined
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
        text: imapAccess.oneMessageModel ? imapAccess.oneMessageModel.subject : ""
        anchors {left: parent.left; right: parent.right; top: parent.top}
    }
}
