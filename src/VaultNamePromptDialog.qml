import QtQuick
import QtQuick.Controls

// Small modal text prompt used for naming/renaming vault entries, matching
// UnsavedChangesDialog's square, flat styling.
Dialog {
    id: root

    property string promptTitle: "New note"
    property string initialText: ""
    property bool darkMode: true
    property color textColor: darkMode ? "#d0d0d0" : "#42464c"
    property color strongTextColor: darkMode ? "#eeeeee" : "#222324"
    property color activeButtonColor: "#428bca"
    property int containerWidth: 420
    property int containerHeight: 320
    property real textScale: 1

    signal nameAccepted(string name)

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(360, containerWidth - 48)
    x: Math.round((containerWidth - width) / 2)
    y: Math.round((containerHeight - height) / 2)
    padding: 20

    onOpened: {
        nameField.text = root.initialText;
        nameField.forceActiveFocus();
        nameField.selectAll();
    }

    background: Rectangle {
        color: root.darkMode ? "#1a1a1a" : "#ffffff"
        border.color: root.darkMode ? "#343434" : "#d8d8d8"
        radius: 0
    }

    contentItem: Column {
        spacing: 12

        Label {
            text: root.promptTitle
            color: root.strongTextColor
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(16 * root.textScale)
            font.bold: true
        }

        TextField {
            id: nameField
            width: parent.width
            color: root.textColor
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(13 * root.textScale)
            selectByMouse: true
            background: Rectangle {
                color: "transparent"
                border.color: root.darkMode ? "#424242" : "#c8c8c8"
                radius: 0
            }
            Keys.onReturnPressed: root.accept()
            Keys.onEnterPressed: root.accept()
            Keys.onEscapePressed: root.reject()
        }
    }

    footer: Item {
        implicitHeight: dialogButtons.implicitHeight + 20

        Row {
            id: dialogButtons
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            SquareDialogButton {
                text: "Cancel"
                darkMode: root.darkMode
                textScale: root.textScale
                labelColor: root.textColor
                onClicked: root.reject()
            }

            SquareDialogButton {
                id: confirmButton
                text: "OK"
                primary: true
                darkMode: root.darkMode
                textScale: root.textScale
                activeColor: root.activeButtonColor
                onClicked: root.accept()
            }
        }
    }

    onAccepted: {
        var trimmed = nameField.text.trim();
        if (trimmed.length > 0)
            nameAccepted(trimmed);
    }
}
