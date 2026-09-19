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
    implicitHeight: 32

    Row {
        id: content
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Label {
            anchors.verticalCenter: parent.verticalCenter
            color: "#A3A7B6"
            font.family: "Segoe UI"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            text: "AUDITION TARGET"
        }

        Row {
            spacing: 2
            StudioButton {
                id: preparedButton
                objectName: "auditionPreparedButton"
                text: "PREPARED"
                minimumControlWidth: 92
                contentPadding: 10
                enabled: auditionSelector.preparedAvailable
                selected: auditionSelector.activeTarget === "PREPARED"
                accentColor: "#00C8FF"
                onClicked: auditionSelector.selectPrepared()
                KeyNavigation.backtab: root.previousTabItem
                KeyNavigation.tab: processedButton
                Accessible.name: "Audition Prepared realization"
                Accessible.description: enabled ? (selected ? "Selected" : "Available audition target") : "Unavailable audition target"
            }
            StudioButton {
                id: processedButton
                objectName: "auditionProcessedButton"
                text: "PROCESSED"
                minimumControlWidth: 98
                contentPadding: 10
                enabled: auditionSelector.processedAvailable
                selected: auditionSelector.activeTarget === "PROCESSED"
                accentColor: "#9A73FF"
                onClicked: auditionSelector.selectProcessed()
                KeyNavigation.backtab: preparedButton
                KeyNavigation.tab: goldButton
                Accessible.name: "Audition Processed realization"
                Accessible.description: enabled ? (selected ? "Selected" : "Available audition target") : "Unavailable audition target"
            }
            StudioButton {
                id: goldButton
                objectName: "auditionGoldButton"
                text: "GOLD"
                tone: "gold"
                minimumControlWidth: 70
                contentPadding: 10
                enabled: auditionSelector.goldAvailable
                selected: auditionSelector.activeTarget === "GOLD"
                accentColor: "#F2B632"
                onClicked: auditionSelector.selectGold()
                KeyNavigation.backtab: processedButton
                KeyNavigation.tab: root.nextTabItem
                Accessible.name: "Audition Gold Reference"
                Accessible.description: enabled ? (selected ? "Selected" : "Available audition target") : "Unavailable audition target"
            }
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
