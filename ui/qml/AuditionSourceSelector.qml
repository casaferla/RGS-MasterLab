import QtQuick
import QtQuick.Controls

Item {
    id: root
    objectName: "auditionSourceSelectorPanel"

    property Item previousTabItem
    property Item nextTabItem
    property alias firstTarget: preparedButton
    property alias lastTarget: goldButton

    implicitWidth: content.implicitWidth
    implicitHeight: 36

    Row {
        id: content
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        Label {
            anchors.verticalCenter: parent.verticalCenter
            color: "#8A97A3"
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.weight: Font.DemiBold
            text: "AUDITION TARGET"
        }

        StudioButton {
            id: preparedButton
            objectName: "auditionPreparedButton"
            text: "PREPARED"
            contentPadding: 12
            implicitHeight: 30
            enabled: auditionSelector.preparedAvailable
            selected: auditionSelector.activeTarget === "PREPARED"
            onClicked: auditionSelector.selectPrepared()
            KeyNavigation.backtab: root.previousTabItem
            KeyNavigation.tab: processedButton
            Accessible.name: "Audition Prepared realization"
            Accessible.description: selected ? "Selected" : "Available audition target"
        }

        StudioButton {
            id: processedButton
            objectName: "auditionProcessedButton"
            text: "PROCESSED"
            contentPadding: 12
            implicitHeight: 30
            enabled: auditionSelector.processedAvailable
            selected: auditionSelector.activeTarget === "PROCESSED"
            onClicked: auditionSelector.selectProcessed()
            KeyNavigation.backtab: preparedButton
            KeyNavigation.tab: goldButton
            Accessible.name: "Audition Processed realization"
            Accessible.description: enabled ? (selected ? "Selected" : "Available audition target")
                                            : "Unavailable audition target"
        }

        StudioButton {
            id: goldButton
            objectName: "auditionGoldButton"
            text: "GOLD"
            tone: "gold"
            contentPadding: 12
            implicitHeight: 30
            enabled: auditionSelector.goldAvailable
            selected: auditionSelector.activeTarget === "GOLD"
            onClicked: auditionSelector.selectGold()
            KeyNavigation.backtab: processedButton
            KeyNavigation.tab: root.nextTabItem
            Accessible.name: "Audition Gold Reference"
            Accessible.description: enabled ? (selected ? "Selected" : "Available audition target")
                                            : "Unavailable audition target"
        }
    }

    Label {
        objectName: "activeAuditionTargetLabel"
        visible: false
        text: "Active: " + auditionSelector.activeTarget
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }
}
