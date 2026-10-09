// AntCloud Native additions, GPL-3.0-or-later, 2026.
import QtQuick
import QtQuick.Window
import QtQml
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: appWindow

    readonly property color accent: "#0867d2"
    readonly property color errorColor: theme.window.hslLightness < 0.5 ? "#ff8a80" : "#b42318"
    property bool allowClose: false
    required property var controller
    readonly property int currentStage: stageNumbers[appWindow.controller.phase] === undefined ? -1 : stageNumbers[appWindow.controller.phase]
    readonly property var stageNumbers: ({
            "connecting": 0,
            "booting": 1,
            "host": 2,
            "pairing": 3,
            "launching": 3,
            "video": 4,
            "connected": 5
        })
    readonly property var stepModel: buildStepModel(currentStage, accent, theme.mid)

    function buildStepModel(stage, selectedColor, pendingColor) {
        var names = ["Provider session", "VM boot", "Native host", "Mac pairing", "Video and audio"];
        var result = [];
        for (var index = 0; index < names.length; ++index) {
            result[index] = {
                "title": names[index],
                "number": index + 1,
                "done": index < stage,
                "current": index === stage,
                "future": index > stage,
                "color": index <= stage ? selectedColor : pendingColor
            };
        }
        return result;
    }

    color: theme.window
    font.family: Qt.platform.os === "osx" ? ".AppleSystemUIFont" : "sans-serif"
    font.pixelSize: 14
    height: 740
    minimumHeight: 540
    minimumWidth: 600
    objectName: "antcloudWindow"
    palette.base: theme.base
    palette.button: theme.button
    palette.buttonText: theme.buttonText
    palette.highlight: appWindow.accent
    palette.highlightedText: "white"
    palette.mid: theme.mid
    palette.text: theme.text
    palette.window: theme.window
    palette.windowText: theme.windowText
    title: "AntCloud Native"
    visible: true
    width: 860

    menuBar: MenuBar {
        Menu {
            title: "Session"

            Action {
                enabled: appWindow.controller.signedIn && !appWindow.controller.busy
                text: "Connect"

                onTriggered: appWindow.controller.start()
            }
            Action {
                enabled: appWindow.controller.active
                text: "Disconnect"

                onTriggered: appWindow.controller.disconnectStream()
            }
            Action {
                enabled: appWindow.controller.active && appWindow.controller.phase !== "connecting"
                text: "Shutdown VM…"

                onTriggered: shutdownDialog.open()
            }
            MenuSeparator {
            }
            Action {
                enabled: appWindow.controller.signedIn && !appWindow.controller.busy
                text: "Sign out"

                onTriggered: appWindow.controller.signOut()
            }
            Action {
                // Qt maps Ctrl to Command on macOS; stream input uses SDL physical keys.
                shortcut: "Ctrl+Q"
                text: "Quit AntCloud Native"

                onTriggered: appWindow.close()
            }
        }
        Menu {
            title: "Help"

            Action {
                text: "AntCloud dashboard"

                onTriggered: Qt.openUrlExternally("https://antcloud.co")
            }
            Action {
                text: "Keyboard and mouse controls"

                onTriggered: controlsDialog.open()
            }
            Action {
                text: "Export diagnostic report…"

                onTriggered: reportDialog.open()
            }
            Action {
                text: "Clear diagnostic history"

                onTriggered: appWindow.controller.clearDiagnostics()
            }
            Action {
                text: "About AntCloud Native"

                onTriggered: aboutDialog.open()
            }
        }
    }

    Component.onCompleted: appWindow.controller.initialize(appWindow)
    onClosing: function (event) {
        if (appWindow.controller.busy && !allowClose) {
            event.accepted = false;
            closeDialog.open();
        }
    }

    SystemPalette {
        id: theme
        colorGroup: SystemPalette.Active
    }
    ScrollView {
        objectName: "accountScroll"
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.fill: parent
        clip: true
        Flickable {
            id: accountViewport
            contentWidth: width
            contentHeight: Math.max(height, accountContent.implicitHeight + 48)
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: accountContent
                objectName: "accountContent"
                spacing: 22
                width: Math.min(accountViewport.width - 56, 720)
                height: implicitHeight
                x: Math.max(28, (accountViewport.width - width) / 2)
                y: Math.max(24, (accountViewport.height - implicitHeight) / 2)

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        Accessible.ignored: true
                        Layout.preferredHeight: 38
                        Layout.preferredWidth: 38
                        color: appWindow.accent
                        radius: 9

                        Canvas {
                            id: appIcon
                            anchors.fill: parent

                            onPaint: {
                                var ctx = appIcon.getContext("2d");
                                ctx.strokeStyle = "white";
                                ctx.lineWidth = 1.7;
                                ctx.strokeRect(9, 10, 20, 14);
                                ctx.beginPath();
                                ctx.moveTo(19, 24);
                                ctx.lineTo(19, 29);
                                ctx.moveTo(14, 29);
                                ctx.lineTo(24, 29);
                                ctx.stroke();
                            }
                        }
                    }
                    ColumnLayout {
                        spacing: 2

                        Label {
                            font.pixelSize: 19
                            font.weight: Font.DemiBold
                            text: "AntCloud Native"
                        }
                        Label {
                            font.pixelSize: 13
                            opacity: 0.75
                            text: "Windows streaming for macOS"
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Button {
                        enabled: !appWindow.controller.busy
                        text: "Sign out"
                        visible: appWindow.controller.signedIn

                        onClicked: appWindow.controller.signOut()
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: theme.mid
                    opacity: 0.35
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 18
                    visible: !appWindow.controller.signedIn

                    Label {
                        font.pixelSize: 27
                        font.weight: Font.DemiBold
                        text: "Sign in to your VM"
                    }
                    Label {
                        Layout.fillWidth: true
                        opacity: 0.8
                        text: "Your AntCloud account, native video and Mac controls in one app."
                        wrapMode: Text.WordWrap
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columnSpacing: 20
                        columns: 2
                        rowSpacing: 14

                        Label {
                            Layout.preferredWidth: 84
                            text: "Email"
                        }
                        TextField {
                            id: email
                            Accessible.name: "AntCloud email"
                            Layout.fillWidth: true
                            enabled: !appWindow.controller.busy
                            inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoPredictiveText
                            maximumLength: 320
                            objectName: "emailField"
                            placeholderText: "AntCloud email"
                            selectByMouse: true

                            onAccepted: password.forceActiveFocus()
                        }
                        Label {
                            text: "Password"
                        }
                        TextField {
                            id: password
                            Accessible.name: "AntCloud password"
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            enabled: !appWindow.controller.busy
                            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                            maximumLength: 4096
                            objectName: "passwordField"

                            onAccepted: signInButton.clicked()
                        }
                        Item {
                        }
                        CheckBox {
                            id: remember
                            checked: true
                            enabled: !appWindow.controller.busy
                            text: "Remember login in macOS Keychain"
                        }
                        Item {
                        }
                        Button {
                            id: signInButton
                            Layout.preferredWidth: 180
                            enabled: !appWindow.controller.busy && email.text.trim().length > 0 && password.text.length > 0
                            highlighted: true
                            objectName: "signInButton"
                            text: appWindow.controller.busy ? "Signing in…" : "Sign in"

                            onClicked: {
                                if (!enabled)
                                    return;
                                appWindow.controller.login(email.text, password.text, remember.checked);
                                password.clear();
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        opacity: 0.75
                        text: "Your password is never saved. A remembered access token stays in Keychain."
                        wrapMode: Text.WordWrap
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 20
                    visible: appWindow.controller.signedIn

                    RowLayout {
                        Layout.fillWidth: true

                        ColumnLayout {
                            spacing: 6

                            Label {
                                font.pixelSize: 27
                                font.weight: Font.DemiBold
                                text: appWindow.controller.phase === "connected" ? "Connected to Windows" : (appWindow.controller.active && appWindow.currentStage >= 0 ? "Connecting to Windows" : "Your Windows desktop")
                            }
                            Label {
                                font.pixelSize: 13
                                opacity: 0.75
                                text: "Account resolution limit: " + appWindow.controller.resolutionLimit + "p"
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Button {
                            enabled: !appWindow.controller.busy
                            highlighted: true
                            objectName: "connectButton"
                            text: appWindow.controller.active && appWindow.currentStage >= 0 ? "Connecting…" : (appWindow.controller.busy ? "Finishing…" : "Connect")

                            onClicked: appWindow.controller.start()
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columnSpacing: 18
                        columns: appWindow.width < 720 ? 2 : 3
                        enabled: !appWindow.controller.busy
                        rowSpacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 280
                            Layout.preferredWidth: 280

                            Label {
                                font.weight: Font.DemiBold
                                text: "Resolution"
                            }
                            ComboBox {
                                Accessible.name: "Stream resolution"
                                Layout.fillWidth: true
                                currentIndex: appWindow.controller.resolutionIndex
                                model: ["720p · 1280 × 720", "1080p · 1920 × 1080", "2560 × 1440 · Experimental", "3840 × 2160 · Experimental"]
                                objectName: "resolutionPicker"

                                onActivated: appWindow.controller.resolutionIndex = currentIndex
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 100
                            Layout.preferredWidth: 120

                            Label {
                                font.weight: Font.DemiBold
                                text: "Frame rate"
                            }
                            ComboBox {
                                Accessible.name: "Stream frame rate"
                                Layout.fillWidth: true
                                objectName: "frameRatePicker"
                                currentIndex: appWindow.controller.fpsIndex
                                model: ["30 FPS", "60 FPS", "120 FPS"]

                                onActivated: appWindow.controller.fpsIndex = currentIndex
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 220
                            Layout.preferredWidth: 240

                            Label {
                                font.weight: Font.DemiBold
                                text: "Quality / bitrate"
                            }
                            ComboBox {
                                id: bitratePicker

                                property var rates: [3000, 6000, 12000, 20000, 40000]

                                Accessible.name: "Video bitrate"
                                Layout.fillWidth: true
                                objectName: "bitratePicker"
                                currentIndex: Math.max(0, rates.indexOf(appWindow.controller.bitrate))
                                model: ["3 Mbps · low bandwidth", "6 Mbps · balanced", "12 Mbps · high", "20 Mbps · very high", "40 Mbps · maximum"]

                                onActivated: appWindow.controller.bitrate = rates[currentIndex]
                            }
                        }
                    }
                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        enabled: !appWindow.controller.busy
                        spacing: 20

                        CheckBox {
                            checked: appWindow.controller.fullscreen
                            text: "Fullscreen"

                            onToggled: appWindow.controller.fullscreen = checked
                        }
                        CheckBox {
                            checked: appWindow.controller.relativeMouse
                            text: "Relative mouse"

                            onToggled: appWindow.controller.relativeMouse = checked
                        }
                        CheckBox {
                            checked: appWindow.controller.showStats
                            text: "Show stream stats"

                            onToggled: appWindow.controller.showStats = checked
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: theme.mid
                        opacity: 0.35
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                font.weight: Font.DemiBold
                                text: appWindow.controller.active && appWindow.currentStage >= 0 ? "Connection progress" : "Session"
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Label {
                                font.pixelSize: 13
                                opacity: 0.75
                                text: Math.floor(appWindow.controller.elapsed / 60) + "m " + (appWindow.controller.elapsed % 60) + "s elapsed"
                                visible: appWindow.controller.active
                            }
                        }
                        Label {
                            Accessible.name: "Connection status: " + text
                            Layout.fillWidth: true
                            objectName: "connectionDetail"
                            text: appWindow.controller.detail
                            wrapMode: Text.WordWrap
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            visible: appWindow.controller.active && appWindow.currentStage >= 0

                            Repeater {
                                model: appWindow.stepModel

                                delegate: RowLayout {
                                    id: connectionStep

                                    required property var modelData

                                    Layout.fillWidth: true

                                    Rectangle {
                                        Layout.preferredHeight: 22
                                        Layout.preferredWidth: 22
                                        color: connectionStep.modelData.color
                                        radius: 11

                                        Label {
                                            anchors.centerIn: parent
                                            color: "white"
                                            font.pixelSize: 12
                                            text: connectionStep.modelData.done ? "✓" : String(connectionStep.modelData.number)
                                        }
                                    }
                                    Label {
                                        font.weight: connectionStep.modelData.current ? Font.DemiBold : Font.Normal
                                        opacity: connectionStep.modelData.future ? 0.6 : 1
                                        text: connectionStep.modelData.title
                                    }
                                    Label {
                                        font.pixelSize: 12
                                        opacity: 0.7
                                        text: "In progress"
                                        visible: connectionStep.modelData.current
                                    }
                                }
                            }
                        }
                        RowLayout {
                            visible: appWindow.controller.active

                            Button {
                                text: "Disconnect"

                                onClicked: appWindow.controller.disconnectStream()
                            }
                            Button {
                                enabled: appWindow.controller.phase !== "connecting"
                                objectName: "shutdownButton"
                                text: "Shutdown VM…"

                                onClicked: shutdownDialog.open()
                            }
                        }
                    }
                }
                Label {
                    Accessible.name: text
                    Layout.fillWidth: true
                    color: appWindow.errorColor
                    font.weight: Font.DemiBold
                    objectName: "errorMessage"
                    text: "Connection issue\n" + appWindow.controller.error
                    visible: appWindow.controller.error.length > 0
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: theme.mid
                    opacity: 0.35
                    visible: appWindow.controller.shutdownStatus.length > 0
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    objectName: "powerStatusSection"
                    spacing: 8
                    visible: appWindow.controller.shutdownStatus.length > 0

                    Label {
                        font.weight: Font.DemiBold
                        text: "VM power"
                    }
                    Label {
                        Layout.fillWidth: true
                        objectName: "powerStatus"
                        text: appWindow.controller.shutdownStatus
                        wrapMode: Text.WordWrap
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: appWindow.controller.notice
                    visible: appWindow.controller.notice.length > 0
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    objectName: "streamAdvisory"
                    text: appWindow.controller.streamWarning
                    visible: appWindow.controller.active && appWindow.controller.streamWarning.length > 0
                    wrapMode: Text.WordWrap
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: theme.mid
                    opacity: 0.35
                }
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        font.pixelSize: 12
                        objectName: "controlsHint"
                        opacity: 0.8
                        text: "Local stream controls: Control + Command + Shift + P"
                        wrapMode: Text.WordWrap
                    }
                    Button {
                        flat: true
                        text: "Controls"

                        onClicked: controlsDialog.open()
                    }
                }
            }
        }
    }
    Dialog {
        id: shutdownDialog
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape
        focus: true
        modal: true
        objectName: "shutdownDialog"
        palette: appWindow.palette
        title: "Shutdown your Windows VM?"
        width: Math.min(480, appWindow.width - 40)

        contentItem: Label {
            text: "Save your work first. This sends AntCloud’s VM shutdown command once. Check the dashboard for the final power state."
            wrapMode: Text.WordWrap
        }
        footer: RowLayout {
            spacing: 10

            Item {
                Layout.fillWidth: true
            }
            Button {
                id: cancelShutdown
                focus: true
                objectName: "cancelShutdown"
                text: "Cancel"

                Keys.onEnterPressed: clicked()
                Keys.onReturnPressed: clicked()
                onClicked: shutdownDialog.close()
            }
            Button {
                objectName: "confirmShutdown"
                text: "Shutdown VM"

                Keys.onEnterPressed: clicked()
                Keys.onReturnPressed: clicked()
                onClicked: {
                    shutdownDialog.close();
                    appWindow.controller.shutdown();
                }
            }
            Item {
                Layout.preferredWidth: 12
            }
        }

        onOpened: shutdownFocusTimer.start()

        Timer {
            id: shutdownFocusTimer
            interval: 0

            onTriggered: cancelShutdown.forceActiveFocus()
        }
    }
    Dialog {
        id: closeDialog
        anchors.centerIn: parent
        modal: true
        palette: appWindow.palette
        title: "Disconnect and quit?"
        width: Math.min(480, appWindow.width - 40)

        contentItem: Label {
            text: "Your native stream will disconnect. No explicit VM shutdown command will be sent; AntCloud’s idle policy may still stop the VM."
            wrapMode: Text.WordWrap
        }
        footer: RowLayout {
            spacing: 10

            Item {
                Layout.fillWidth: true
            }
            Button {
                id: cancelClose
                focus: true
                text: "Cancel"

                Keys.onEnterPressed: clicked()
                Keys.onReturnPressed: clicked()
                onClicked: closeDialog.close()
            }
            Button {
                text: "Disconnect and quit"

                Keys.onEnterPressed: clicked()
                Keys.onReturnPressed: clicked()
                onClicked: {
                    closeDialog.close();
                    appWindow.controller.disconnectStream();
                    appWindow.allowClose = true;
                    appWindow.close();
                }
            }
            Item {
                Layout.preferredWidth: 12
            }
        }

        onOpened: closeFocusTimer.start()

        Timer {
            id: closeFocusTimer
            interval: 0

            onTriggered: cancelClose.forceActiveFocus()
        }
    }
    Dialog {
        id: controlsDialog
        anchors.centerIn: parent
        modal: true
        palette: appWindow.palette
        standardButtons: Dialog.Close
        title: "Mac stream controls"
        width: Math.min(540, appWindow.width - 40)

        contentItem: Label {
            text: "Windows modifiers\nCommand → Alt · Option → Windows · Control → Ctrl\n\nLocal shortcuts\nControl + Command + Shift + P: stream controls\nControl + Command + Shift + Z: release mouse / controls\nControl + Command + Shift + Q: disconnect stream\nControl + Command + Shift + X: toggle fullscreen\nControl + Command + Shift + S: toggle stats\nControl + Command + Shift + M: toggle mouse mode\nControl + Command + Shift + V: type Mac clipboard text (host support required)\n\nStats show native round-trip time, network-dropped video frames, FPS, received video bitrate and stream dimensions. Frame loss is not a packet-loss counter.\n\nFn is managed by macOS; a separate Windows Fn key is not guaranteed. Clipboard sync and file transfer are not included in this release."
            wrapMode: Text.WordWrap
        }
    }
    Dialog {
        id: aboutDialog
        anchors.centerIn: parent
        modal: true
        palette: appWindow.palette
        standardButtons: Dialog.Close
        title: "AntCloud Native " + Qt.application.version
        width: Math.min(460, appWindow.width - 40)

        contentItem: Label {
            text: "Personal macOS client, based on Moonlight 6.2.0.\n\nNative C++ / Qt with the existing Moonlight video, audio and input engine. Not an official AntCloud release.\n\nGPL-3.0-or-later. Full source and upstream notices are included in the source package.\n\nDiagnostics are limited to the last 64 safe session events in memory. A report is written only when you export it."
            wrapMode: Text.WordWrap
        }
    }
    FileDialog {
        id: reportDialog
        defaultSuffix: "txt"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Text report (*.txt)"]
        title: "Export a redacted diagnostic report"

        onAccepted: appWindow.controller.exportDiagnostics(selectedFile)
    }
}
