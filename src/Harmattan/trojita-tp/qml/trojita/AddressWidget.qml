import QtQuick 1.1
import com.nokia.meego 1.0
import "Utils.js" as Utils

Label {
    property string caption
    property variant address

    text: visible ?
              "<b>" + caption + ":</b> " + Utils.formatMailAddresses(address) :
              ""
    wrapMode: Text.Wrap
    visible: Utils.isMailAddressValid(address)
}
