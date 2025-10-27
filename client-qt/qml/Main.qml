import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: window
    width: 1240
    height: 780
    visible: true
    title: qsTr("SM — Secure Messenger Demo")
    color: "#0d1117"

    Material.theme: Material.Dark
    Material.accent: "#4f83ff"

    readonly property color panelColor: "#161b22"
    readonly property color panelBorder: "#1f242f"
    readonly property color subtleText: "#9ca3af"
    readonly property color bubbleOutgoing: "#244c7a"
    readonly property color bubbleIncoming: "#1f6f43"

    header: ToolBar {
        padding: 12
        background: Rectangle { color: "#161b22" }

        contentItem: RowLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                text: qsTr("Подключено по mTLS · демонстрация серверных сервисов")
                font.bold: true
                font.pixelSize: 18
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Имитация pull")
                icon.name: "refresh"
                onClicked: if (App && App.simulatePull) App.simulatePull()
            }

            Button {
                text: qsTr("Обновить справочник")
                icon.name: "reload"
                onClicked: if (App && App.refreshUsers) App.refreshUsers()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18

            ColumnLayout {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                spacing: 18

                Pane {
                    Layout.fillWidth: true
                    padding: 16
                    background: Rectangle {
                        color: panelColor
                        radius: 12
                        border.color: panelBorder
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Label {
                            text: qsTr("Идентификация")
                            font.bold: true
                            font.pixelSize: 16
                        }

                        Label {
                            text: qsTr("%1 (%2)")
                                      .arg(App && App.authInfo ? App.authInfo.displayName || "" : "")
                                      .arg(App && App.authInfo ? App.authInfo.userId || "" : "")
                            color: "white"
                            font.pixelSize: 20
                        }

                        Label {
                            text: qsTr("Роли: %1")
                                      .arg(App && App.authInfo && App.authInfo.roles ? App.authInfo.roles.join(", ") : "")
                            color: subtleText
                        }

                        Label {
                            text: qsTr("Активное устройство: %1")
                                      .arg(App && App.authInfo ? App.authInfo.deviceId || "" : "")
                            color: subtleText
                        }

                        Frame {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 140
                            padding: 0
                            background: Rectangle {
                                color: "#0f172a"
                                radius: 8
                                border.color: panelBorder
                            }

                            TextArea {
                                anchors.fill: parent
                                anchors.margins: 12
                                readOnly: true
                                wrapMode: TextArea.WrapAnywhere
                                selectByMouse: true
                                font.family: "monospace"
                                color: "#e5e7eb"
                                text: String(App && App.authInfo ? App.authInfo.certificate || "" : "")
                                background: Rectangle { color: "transparent" }
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            }
                        }
                    }
                }

                Pane {
                    Layout.fillWidth: true
                    padding: 16
                    background: Rectangle {
                        color: panelColor
                        radius: 12
                        border.color: panelBorder
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Label {
                            text: qsTr("Серверные возможности")
                            font.bold: true
                            font.pixelSize: 16
                        }

                        Flow {
                            width: parent.width
                            spacing: 8

                            Repeater {
                                model: [
                                    qsTr("mTLS аутентификация"),
                                    qsTr("Directory Service"),
                                    qsTr("Messaging Service"),
                                    qsTr("Аудит действий"),
                                    qsTr("Ротация сертификатов"),
                                    qsTr("Отзыв устройств")
                                ]

                                delegate: Rectangle {
                                    radius: 14
                                    color: "#1f2937"
                                    border.color: panelBorder
                                    implicitWidth: badgeLabel.implicitWidth + 24
                                    implicitHeight: badgeLabel.implicitHeight + 12

                                    Label {
                                        id: badgeLabel
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: "#d1d5db"
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }

                Pane {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    padding: 16
                    background: Rectangle {
                        color: panelColor
                        radius: 12
                        border.color: panelBorder
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("Directory Service")
                                font.bold: true
                                font.pixelSize: 16
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: qsTr("Выберите пользователя для операций")
                                color: subtleText
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 12
                            model: (App && App.userList) ? App.userList : []
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: Item {
                                width: ListView.view.width
                                property var entry: modelData
                                implicitHeight: card.implicitHeight + 12

                                Rectangle {
                                    id: card
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    color: "#1b2735"
                                    radius: 12
                                    border.color: panelBorder

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 10

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Label {
                                                text: String(entry.displayName || "")
                                                font.bold: true
                                                font.pixelSize: 15
                                            }

                                            Label {
                                                text: qsTr("ID: %1").arg(String(entry.userId || ""))
                                                color: subtleText
                                                font.pixelSize: 12
                                            }
                                        }

                                        Repeater {
                                            model: entry.devices || []

                                            delegate: Rectangle {
                                                width: parent.width
                                                radius: 8
                                                color: (modelData["revoked"] === true) ? "#3f1d29" : "#1f2d3d"
                                                border.color: panelBorder
                                                implicitHeight: deviceRow.implicitHeight + 12

                                                RowLayout {
                                                    id: deviceRow
                                                    anchors.fill: parent
                                                    anchors.margins: 12
                                                    spacing: 10

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 4

                                                        Label {
                                                            text: qsTr("Устройство %1")
                                                                      .arg(String(modelData["deviceId"] || ""))
                                                            font.pixelSize: 13
                                                            font.bold: true
                                                        }

                                                        Label {
                                                            text: {
                                                                const cert = String(modelData["certificate"] || "")
                                                                return cert.length > 36 ? cert.slice(0, 36) + "…" : cert
                                                            }
                                                            font.family: "monospace"
                                                            font.pixelSize: 11
                                                            color: subtleText
                                                        }

                                                        Label {
                                                            text: (modelData["revoked"] === true)
                                                                  ? qsTr("Статус: отозван")
                                                                  : qsTr("Статус: активен")
                                                            color: (modelData["revoked"] === true) ? "#f87171" : "#34d399"
                                                            font.pixelSize: 12
                                                        }
                                                    }

                                                    Button {
                                                        text: qsTr("Rotate cert")
                                                        enabled: App && App.rotateDevice
                                                        onClicked: if (App) App.rotateDevice(String(entry.userId || ""), String(modelData["deviceId"] || ""))
                                                    }

                                                    Button {
                                                        text: (modelData["revoked"] === true) ? qsTr("Отозвано") : qsTr("Revoke")
                                                        enabled: App && App.revokeDevice && !(modelData["revoked"] === true)
                                                        onClicked: if (App) App.revokeDevice(String(entry.userId || ""), String(modelData["deviceId"] || ""))
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 18

                Pane {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    padding: 16
                    background: Rectangle {
                        color: panelColor
                        radius: 12
                        border.color: panelBorder
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Label {
                                text: qsTr("Messaging Service")
                                font.bold: true
                                font.pixelSize: 16
                            }

                            Item { Layout.fillWidth: true }

                            TextField {
                                id: conversationField
                                Layout.preferredWidth: 240
                                placeholderText: qsTr("conversation id")
                                text: String(App ? App.currentConversation : "")
                                onEditingFinished: {
                                    if (App) {
                                        App.currentConversation = text
                                    }
                                }

                                Connections {
                                    target: App
                                    function onCurrentConversationChanged() {
                                        conversationField.text = String(App.currentConversation)
                                    }
                                }
                            }

                            Button {
                                text: qsTr("Подписаться")
                                onClicked: {
                                    if (App) {
                                        App.currentConversation = conversationField.text
                                    }
                                }
                            }
                        }

                        ListView {
                            id: chatView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 12
                            model: (App && App.conversation) ? App.conversation : []
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            Component.onCompleted: positionViewAtEnd()

                            delegate: Item {
                                width: chatView.width
                                property var message: modelData
                                implicitHeight: bubble.implicitHeight + 8

                                Rectangle {
                                    id: bubble
                                    anchors.margins: 4
                                    anchors.left: message && message.outgoing ? undefined : parent.left
                                    anchors.right: message && message.outgoing ? parent.right : undefined
                                    color: message && message.outgoing ? bubbleOutgoing : bubbleIncoming
                                    radius: 12
                                    border.color: panelBorder
                                    implicitWidth: Math.min(chatView.width * 0.75, bubbleContent.implicitWidth + 32)
                                    implicitHeight: bubbleContent.implicitHeight + 24

                                    Column {
                                        id: bubbleContent
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 4

                                        Label {
                                            text: qsTr("%1 · %2")
                                                      .arg(String(message.author || ""))
                                                      .arg(String(message.timestamp || ""))
                                            font.pixelSize: 11
                                            color: "#cbd5f5"
                                        }

                                        Label {
                                            text: String(message.text || "")
                                            font.pixelSize: 14
                                            color: "#f9fafb"
                                            wrapMode: Text.WordWrap
                                            width: Math.min(chatView.width * 0.72, 480)
                                        }
                                    }
                                }
                            }

                            Connections {
                                target: App
                                function onConversationChanged() { chatView.positionViewAtEnd() }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            TextField {
                                id: input
                                Layout.fillWidth: true
                                placeholderText: qsTr("Сообщение (шифруется на клиенте перед отправкой)…")
                                onAccepted: sendButton.clicked()
                            }

                            Button {
                                id: sendButton
                                text: qsTr("Отправить")
                                enabled: App && App.send
                                onClicked: {
                                    if (!App || input.text.length === 0)
                                        return
                                    App.send(input.text)
                                    input.text = ""
                                }
                            }
                        }
                    }
                }

                Pane {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220
                    padding: 16
                    background: Rectangle {
                        color: panelColor
                        radius: 12
                        border.color: panelBorder
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("Server debug log")
                                font.bold: true
                                font.pixelSize: 16
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: qsTr("последние события")
                                color: subtleText
                            }
                        }

                        ListView {
                            id: logView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 6
                            model: (App && App.serverLog) ? App.serverLog : []
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            Component.onCompleted: positionViewAtEnd()

                            delegate: Item {
                                width: logView.width
                                implicitHeight: logLine.implicitHeight + 8

                                Label {
                                    id: logLine
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    text: String(modelData)
                                    font.family: "monospace"
                                    font.pixelSize: 12
                                    color: "#d1d5db"
                                    wrapMode: Text.WordWrap
                                }
                            }

                            Connections {
                                target: App
                                function onServerLogChanged() { logView.positionViewAtEnd() }
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: if (App && App.refreshUsers) App.refreshUsers()
}
