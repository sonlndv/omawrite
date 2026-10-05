import QtQuick
import QtQuick.Controls

// Plain folder/note tree for the vault. Deliberately not an Obsidian clone:
// no graph, no backlinks panel, no search — just browse, open, and the four
// file operations (new note, new folder, rename, delete).
Item {
    id: root

    property string vaultRoot: ""
    property var vaultEntries: []
    property string currentRelativePath: ""
    property bool darkMode: true
    property real textScale: 1
    property color backgroundColor: "#141414"
    property color textColor: "#d0d0d0"
    property color mutedColor: "#8a8a8a"
    property color accentColor: "#428bca"

    signal openRequested(string relativePath)
    signal newNoteRequested(string parentRelativePath)
    signal newFolderRequested(string parentRelativePath)
    signal renameRequested(string relativePath, bool isFolder)
    signal deleteRequested(string relativePath, bool isFolder)
    signal chooseVaultRequested()

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * textScale));
    }

    // Build a flat, depth-ordered row model (folders before files within a
    // directory, both sorted by name) from the backend's flat entry list.
    readonly property var rows: {
        var byParent = {};
        for (var i = 0; i < vaultEntries.length; i++) {
            var entry = vaultEntries[i];
            var parent = entry.parent;
            if (!byParent[parent])
                byParent[parent] = [];
            byParent[parent].push(entry);
        }
        for (var key in byParent) {
            byParent[key].sort(function(a, b) {
                if (a.isDir !== b.isDir)
                    return a.isDir ? -1 : 1;
                return a.name.localeCompare(b.name);
            });
        }

        var result = [];
        function walk(parentPath, depth) {
            var children = byParent[parentPath] || [];
            for (var j = 0; j < children.length; j++) {
                var child = children[j];
                result.push({path: child.path, name: child.name,
                            isDir: child.isDir, depth: depth});
                if (child.isDir)
                    walk(child.path, depth + 1);
            }
        }
        walk("", 0);
        return result;
    }

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
    }

    Column {
        anchors.fill: parent

        Item {
            width: parent.width
            height: root.scaledSize(36)

            Label {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: "NOTES"
                color: root.mutedColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaledSize(10)
                font.letterSpacing: 1
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                ToolButton {
                    text: "+N"
                    font.family: "IBM Plex Mono"
                    font.pixelSize: root.scaledSize(10)
                    ToolTip.visible: hovered
                    ToolTip.text: "New note"
                    onClicked: root.newNoteRequested("")
                }

                ToolButton {
                    text: "+F"
                    font.family: "IBM Plex Mono"
                    font.pixelSize: root.scaledSize(10)
                    ToolTip.visible: hovered
                    ToolTip.text: "New folder"
                    onClicked: root.newFolderRequested("")
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: root.mutedColor
            opacity: 0.25
        }

        ListView {
            id: treeView
            objectName: "vaultTree"
            width: parent.width
            height: parent.height - root.scaledSize(36) - vaultRootRow.height - 1
            clip: true
            model: root.rows
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: ItemDelegate {
                id: rowDelegate
                required property var modelData
                required property int index

                width: treeView.width
                height: root.scaledSize(26)
                highlighted: !modelData.isDir
                    && modelData.path === root.currentRelativePath

                background: Rectangle {
                    color: rowDelegate.highlighted
                        ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.22)
                        : (rowDelegate.hovered
                            ? Qt.rgba(1, 1, 1, 0.06)
                            : "transparent")
                }

                onClicked: {
                    if (!modelData.isDir)
                        root.openRequested(modelData.path);
                }

                contentItem: Row {
                    spacing: 6
                    leftPadding: 10 + modelData.depth * 14

                    Label {
                        text: modelData.isDir ? "\u25B8" : "\u2022"
                        color: root.mutedColor
                        font.pixelSize: root.scaledSize(10)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Label {
                        text: modelData.name
                        color: root.textColor
                        elide: Text.ElideRight
                        font.family: "IBM Plex Mono"
                        font.pixelSize: root.scaledSize(12)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: entryMenu.popup()
                }

                Menu {
                    id: entryMenu
                    MenuItem {
                        text: "New note"
                        visible: rowDelegate.modelData.isDir
                        height: visible ? implicitHeight : 0
                        onTriggered: root.newNoteRequested(rowDelegate.modelData.path)
                    }
                    MenuItem {
                        text: "New folder"
                        visible: rowDelegate.modelData.isDir
                        height: visible ? implicitHeight : 0
                        onTriggered: root.newFolderRequested(rowDelegate.modelData.path)
                    }
                    MenuItem {
                        text: "Rename"
                        onTriggered: root.renameRequested(rowDelegate.modelData.path,
                                                          rowDelegate.modelData.isDir)
                    }
                    MenuItem {
                        text: "Delete"
                        onTriggered: root.deleteRequested(rowDelegate.modelData.path,
                                                          rowDelegate.modelData.isDir)
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: root.mutedColor
            opacity: 0.25
        }

        Item {
            id: vaultRootRow
            width: parent.width
            height: root.scaledSize(30)

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: root.vaultRoot
                elide: Text.ElideMiddle
                color: root.mutedColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaledSize(9)

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.chooseVaultRequested()
                }
            }
        }
    }
}
