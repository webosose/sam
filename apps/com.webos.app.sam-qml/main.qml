import QtQuick 2.4
import Eos.Window 0.1
import QtQuick.Window 2.2

WebOSWindow {
    id: root
    title: "Bare Eos Window"
    displayAffinity: params["displayAffinity"]
    width: 1920
    height: 1080
    visible: true
    color: "yellow"
}
