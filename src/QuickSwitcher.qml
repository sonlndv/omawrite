import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Ctrl+Shift+O quick switcher: fuzzy note-title jump, with a second mode
// (Tab, or Ctrl+Shift+O again) for vault-wide full-text search. Reuses the
// same Pane/TextInput look as the in-document find bar in Main.qml rather
// than inventing a new widget language.
Popup {
    id: root

    property bool darkMode: true
    property real textScale: 1
    property color backgroundColor: "#141414"
    property color textColor: "#d0d0d0"
    property color mutedColor: "#8a8a8a"
    property color accentColor: "#428bca"

    // "titles" or "content"
    property string mode: "titles"
    property var results: []

    signal openRequested(string relativePath)
    signal titleQueryChanged(string query)
    signal contentQueryChanged(string query)

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * textScale));
    }

    function reset() {
        mode = "titles";
        queryField.text = "";
        results = [];
        listView.currentIndex = -1;
    }

    function toggleMode() {
        mode = (mode === "titles") ? "content" : "titles";
        runQuery();
    }

    function runQuery() {
        if (mode === "titles")
            root.titleQueryChanged(queryField.text);
        else
            root.contentQueryChanged(queryField.text);
        listView.currentIndex = results.length > 0 ? 0 : -1;
    }

    function openCurrent() {
        if (listView.currentIndex < 0 || listView.currentIndex >= results.length)
            return;
        root.openRequested(results[listView.currentIndex].path);
        root.close();
    }

    onOpened: {
        reset();
        queryField.forceActiveFocus();
    }

    modal: true
    focus: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    width: Math.min(560, (parent ? parent.width : 560) * 0.8)
    height: scaledSize(mode === "titles" ? 360 : 420)
    anchors.centerIn: parent ? parent : undefined

    background: Rectangle {
        radius: 10
        color: root.darkMode ? "#1c1c19" : "#fffef2"
        border.color: root.darkMode ? "#3a3a35" : "#d9d9c8"
        border.width: 1
    }

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: accentColor

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.scaledSize(48)
            Layout.margins: 8
            spacing: 8

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextInput {
                    id: queryField
                    anchors.fill: parent
                    verticalAlignment: TextInput.AlignVCenter
                    leftPadding: 8
                    selectByMouse: true
                    color: root.textColor
                    font.pixelSize: root.scaledSize(18)
                    clip: true
                    onTextChanged: root.runQuery()

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Down) {
                            listView.currentIndex = Math.min(
                                listView.currentIndex + 1, root.results.length - 1);
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Up) {
                            listView.currentIndex = Math.max(listView.currentIndex - 1, 0);
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Tab) {
                            root.toggleMode();
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.openCurrent();
                            event.accepted = true;
                        }
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        text: root.mode === "titles"
                            ? "Jump to note\u2026"
                            : "Search note contents\u2026"
                        visible: queryField.text.length === 0
                        color: root.mutedColor
                        font.pixelSize: root.scaledSize(18)
                    }
                }
            }

            Label {
                text: root.mode === "titles" ? "TITLES" : "CONTENT"
                color: root.accentColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaledSize(10)
                font.letterSpacing: 1
                Layout.alignment: Qt.AlignVCenter

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.toggleMode()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: root.mutedColor
            opacity: 0.25
        }

        ListView {
            id: listView
            objectName: "quickSwitcherList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.results
            currentIndex: -1
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: ItemDelegate {
                id: rowDelegate
                required property var modelData
                required property int index

                width: listView.width
                height: root.scaledSize(root.mode === "titles" ? 32 : 48)
                highlighted: index === listView.currentIndex

                background: Rectangle {
                    color: rowDelegate.highlighted
                        ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.22)
                        : (rowDelegate.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                }

                onClicked: {
                    listView.currentIndex = index;
                    root.openCurrent();
                }

                contentItem: Column {
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: 10
                    spacing: 2

                    Label {
                        text: rowDelegate.modelData.name
                        color: root.textColor
                        elide: Text.ElideRight
                        font.family: "IBM Plex Mono"
                        font.pixelSize: root.scaledSize(13)
                    }

                    Label {
                        text: rowDelegate.modelData.snippet || ""
                        visible: root.mode === "content"
                        color: root.mutedColor
                        elide: Text.ElideRight
                        width: parent.width - 20
                        font.pixelSize: root.scaledSize(11)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: listView.count === 0 && queryField.text.length > 0
                text: "No matches"
                color: root.mutedColor
                font.pixelSize: root.scaledSize(13)
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 8
            text: "Tab: switch titles/content \u00b7 \u2191\u2193: navigate \u00b7 Enter: open \u00b7 Esc: close"
            color: root.mutedColor
            opacity: 0.6
            font.pixelSize: root.scaledSize(10)
        }
    }
}
