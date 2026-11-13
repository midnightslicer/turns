import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1100
    height: 760
    minimumWidth: 640
    minimumHeight: 460
    visible: true
    title: shownPerson ? "Turns – " + shownPerson : "Turns"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool fullscreen: visibility === Window.FullScreen

    Binding { target: wakeLock; property: "active"; value: win.fullscreen || (backend.playing && backend.hasPhotos) }
    // The name changes when the photo appears, not when it starts loading.
    property string shownPerson: ""
    property Image front: layerA
    readonly property Image back: front === layerA ? layerB : layerA

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.togglePlaying() }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; onActivated: backend.next() }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; onActivated: backend.previous() }
    Shortcut { sequences: ["+", "="]; context: Qt.ApplicationShortcut; onActivated: win.changeSeconds(1) }
    Shortcut { sequence: "-"; context: Qt.ApplicationShortcut; onActivated: win.changeSeconds(-1) }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; onActivated: win.toggleFade() }
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; onActivated: backend.toggleName() }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; onActivated: backend.chooseFolder() }
    Shortcut { sequences: ["F", "F11"]; context: Qt.ApplicationShortcut; onActivated: win.visibility = win.fullscreen ? Window.Windowed : Window.FullScreen }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible)
                win.helpVisible = false
            else if (win.fullscreen)
                win.visibility = Window.Windowed
        }
    }

    function changeSeconds(delta) {
        backend.changeSeconds(delta)
        showToast(qsTr("%1 s per photo").arg(backend.seconds.toLocaleString(Qt.locale(translator.language), 'f', 0)))
    }

    function toggleFade() {
        backend.toggleFade()
        showToast(backend.fade ? qsTr("Fade on") : qsTr("Fade off"))
    }

    function showToast(message) {
        toastText.text = message
        toast.opacity = 1
        toastTimer.restart()
    }

    // The next photo decodes in the hidden layer and fades in once it's ready,
    // so the old one stays up meanwhile instead of flashing to black.
    function showPhoto() {
        if (backend.photo.toString() === "") {
            layerA.source = ""
            layerB.source = ""
            shownPerson = ""
            return
        }
        back.source = backend.photo
        settleBack()
    }

    function settleBack() {
        if (back.status === Image.Ready) {
            shownPerson = backend.person
            front = back
        } else if (back.status === Image.Error) {
            backend.photoFailed()
        }
    }

    Connections {
        target: backend
        function onPhotoChanged() { win.showPhoto() }
        function onNoticeChanged() { noticeTimer.restart() }
    }
    Component.onCompleted: showPhoto()

    Timer { id: noticeTimer; interval: 8000; running: true }
    Timer { id: toastTimer; interval: 1500; onTriggered: toast.opacity = 0 }
    Timer { id: cursorTimer; interval: 2000; running: win.fullscreen; onTriggered: pointer.idle = true }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Item {
            id: stage
            anchors.fill: parent

            // Decoding at the screen's size keeps a folder of 50-megapixel
            // photos from filling memory, and doesn't depend on the window's
            // size, so resizing the window doesn't reload the photo.
            component PhotoLayer: Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                autoTransform: true
                mipmap: true
                sourceSize: Qt.size(Screen.width * Screen.devicePixelRatio, Screen.height * Screen.devicePixelRatio)
                opacity: win.front === this ? 1 : 0
                // With the fade off, photos cut straight from one to the next.
                Behavior on opacity {
                    enabled: backend.fade
                    NumberAnimation { duration: 350; easing.type: Easing.InOutQuad }
                }
                onStatusChanged: if (win.back === this) win.settleBack()
            }

            PhotoLayer { id: layerA }
            PhotoLayer { id: layerB }
        }

        MouseArea {
            id: pointer
            property bool idle: false
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            hoverEnabled: true
            cursorShape: win.fullscreen && idle ? Qt.BlankCursor : Qt.ArrowCursor
            onPositionChanged: {
                idle = false
                cursorTimer.restart()
            }
        }

        Rectangle {
            id: nameTag
            visible: backend.nameVisible && win.shownPerson !== ""
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 24
            width: nameText.implicitWidth + 32
            height: nameText.implicitHeight + 16
            radius: theme.radius
            color: theme.accent

            Text {
                id: nameText
                anchors.centerIn: parent
                text: win.shownPerson
                color: theme.accentForeground
                font.pixelSize: 26
                font.bold: true
            }
        }

        Rectangle {
            visible: !backend.playing && backend.hasPhotos
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            width: pausedText.implicitWidth + 24
            height: pausedText.implicitHeight + 12
            radius: theme.radius
            color: "#cc0e0e10"

            Text {
                id: pausedText
                anchors.centerIn: parent
                text: qsTr("Paused")
                color: "white"
                font.pixelSize: 16
            }
        }

        Rectangle {
            id: toast
            opacity: 0
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 24
            width: toastText.implicitWidth + 24
            height: toastText.implicitHeight + 12
            radius: theme.radius
            color: "#cc0e0e10"
            Behavior on opacity { NumberAnimation { duration: 150 } }

            Text {
                id: toastText
                anchors.centerIn: parent
                color: "white"
                font.pixelSize: 16
            }
        }

        // Over a running slideshow a notice has its moment and then gets out
        // of the way; with no photos it's the point of the screen, so it stays.
        Rectangle {
            visible: backend.notice !== "" && backend.hasPhotos && noticeTimer.running
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 24
            width: Math.min(noticeText.implicitWidth + 24, parent.width - 48)
            height: noticeText.implicitHeight + 16
            radius: theme.radius
            color: "#cc0e0e10"

            Text {
                id: noticeText
                anchors.fill: parent
                anchors.margins: 8
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                text: backend.notice
                color: "white"
                font.pixelSize: 15
                wrapMode: Text.Wrap
            }
        }

        ColumnLayout {
            visible: !backend.hasPhotos
            anchors.centerIn: parent
            width: Math.min(parent.width - 96, 560)
            spacing: 16

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Choose a folder of photos")
                color: "white"
                font.pixelSize: 28
                font.bold: true
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Name each photo with a name and a number, like Ada-12.jpg. The names take turns.")
                color: "#b4b4bb"
                font.pixelSize: 16
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                visible: backend.notice !== ""
                horizontalAlignment: Text.AlignHCenter
                text: backend.notice
                color: theme.accent
                font.pixelSize: 15
                wrapMode: Text.Wrap
            }
            Button {
                id: openButton
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                text: qsTr("Open Folder")
                focusPolicy: Qt.NoFocus
                padding: 12
                leftPadding: 24
                rightPadding: 24
                onClicked: backend.chooseFolder()
                contentItem: Text {
                    text: openButton.text
                    color: theme.accentForeground
                    font.pixelSize: 16
                    font.bold: true
                }
                background: Rectangle {
                    radius: theme.radius
                    color: theme.accent
                    opacity: openButton.down ? 0.75 : openButton.hovered ? 0.88 : 1
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: "#b3000000"

            MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

            Rectangle {
                anchors.centerIn: parent
                width: helpLayout.implicitWidth + 64
                height: helpLayout.implicitHeight + 56
                radius: theme.radius
                color: "#1b1b1f"

                ColumnLayout {
                    id: helpLayout
                    anchors.centerIn: parent
                    spacing: 16

                    Text {
                        text: qsTr("Keyboard shortcuts")
                        color: "white"
                        font.pixelSize: 20
                        font.bold: true
                    }
                    GridLayout {
                        columns: 2
                        columnSpacing: 28
                        rowSpacing: 8

                        Repeater {
                            model: [
                                ["Space", qsTr("Pause or play")],
                                ["→", qsTr("Next photo")],
                                ["←", qsTr("Previous photo")],
                                ["+  −", qsTr("Slower or faster")],
                                ["T", qsTr("Fade on or off")],
                                ["N", qsTr("Hide or show the name")],
                                ["F", qsTr("Full screen")],
                                ["Esc", qsTr("Leave full screen")],
                                ["O", qsTr("Open Folder")],
                                ["?", qsTr("Show shortcuts")],
                                ["Q", qsTr("Quit")]
                            ]
                            delegate: RowLayout {
                                required property var modelData
                                Layout.columnSpan: 2
                                spacing: 28
                                Text {
                                    Layout.preferredWidth: 64
                                    text: parent.modelData[0]
                                    color: theme.accent
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                                Text {
                                    text: parent.modelData[1]
                                    color: "#e6e6ea"
                                    font.pixelSize: 15
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
