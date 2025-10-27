import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    width: 1000
    height: 700
    visible: true
    title: qsTr("SM — Secure Messenger")

    header: ToolBar {
        contentItem: RowLayout {
            anchors.fill: parent
            Label {
                text: qsTr("Подключено (mTLS) — демонстрация возможностей сервера")
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Обновить пользователей")
                onClicked: App.refreshUsers()
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 12

        ColumnLayout {
            id: rootLayout
            width: parent.width
            spacing: 12

            GroupBox {
                title: qsTr("Auth.WhoAmI")
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 6
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Пользователь: %1 (%2)").arg(App.authInfo.displayName, App.authInfo.userId)
                        font.pixelSize: 16
                    }
                    Label {
                        text: qsTr("Роли: %1").arg(App.authInfo.roles.join(", "))
                        color: "#cccccc"
                    }
                    Label {
                        text: qsTr("Активное устройство: %1").arg(App.authInfo.deviceId)
                        color: "#cccccc"
                    }
                    TextArea {
                        text: App.authInfo.certificate
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        font.family: "monospace"
                        background: Rectangle { color: "#1e1e1e"; radius: 4 }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                GroupBox {
                    title: qsTr("Directory Service")
                    Layout.preferredWidth: 320
                    Layout.fillHeight: true

                    ListView {
                        id: userList
                        anchors.fill: parent
                        clip: true
                        model: App.userList
                        spacing: 8
                        delegate: ColumnLayout {
                            width: ListView.view.width
                            spacing: 6

                            property var user: modelData

                            Label {
                                text: qsTr("%1 (%2)").arg(user.displayName, user.userId)
                                font.bold: true
                            }

                            Repeater {
                                model: user.devices
                                delegate: Frame {
                                    Layout.fillWidth: true
                                    property var device: modelData
                                    background: Rectangle {
                                        color: device.revoked ? "#402020" : "#202a40"
                                        radius: 4
                                    }

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 4

                                        Label {
                                            text: qsTr("Device %1").arg(device.deviceId)
                                            font.bold: true
                                        }
                                        Label {
                                            text: device.revoked ? qsTr("Статус: revoked") : qsTr("Статус: active")
                                            color: device.revoked ? "#ff8080" : "#80ff80"
                                        }
                                        TextArea {
                                            text: device.certificate
                                            readOnly: true
                                            wrapMode: TextArea.Wrap
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 60
                                            font.family: "monospace"
                                            background: Rectangle { color: "transparent" }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8

                                            Button {
                                                text: qsTr("Rotate")
                                                enabled: true
                                                onClicked: App.rotateDevice(user.userId, device.deviceId)
                                            }
                                            Button {
                                                text: qsTr("Revoke")
                                                enabled: !device.revoked
                                                onClicked: App.revokeDevice(user.userId, device.deviceId)
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#303030"
                                visible: index < userList.count - 1
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    GroupBox {
                        title: qsTr("Messaging Service")
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label { text: qsTr("Conversation:") }
                                TextField {
                                    id: conversationField
                                    Layout.fillWidth: true
                                    text: App.currentConversation
                                    onTextEdited: App.currentConversation = text
                                    Connections {
                                        target: App
                                        function onCurrentConversationChanged() { conversationField.text = App.currentConversation }
                                    }
                                }
                                Button {
                                    text: qsTr("Pull update")
                                    onClicked: App.simulatePull()
                                }
                            }

                            ListView {
                                id: chatView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 10
                                clip: true
                                model: App.conversation
                                delegate: Item {
                                    width: ListView.view.width
                                    implicitHeight: bubble.implicitHeight + meta.implicitHeight + 8

                                    property var msg: modelData

                                    Column {
                                        anchors.left: msg.outgoing ? undefined : parent.left
                                        anchors.right: msg.outgoing ? parent.right : undefined
                                        anchors.leftMargin: msg.outgoing ? 120 : 0
                                        anchors.rightMargin: msg.outgoing ? 0 : 120
                                        width: parent.width - 80
                                        spacing: 4

                                        Label {
                                            id: meta
                                            text: qsTr("%1 • %2 • %3").arg(msg.author, msg.timestamp, msg.serverMsgId)
                                            color: "#bbbbbb"
                                            horizontalAlignment: msg.outgoing ? Text.AlignRight : Text.AlignLeft
                                            wrapMode: Text.Wrap
                                        }

                                        Rectangle {
                                            id: bubble
                                            color: msg.outgoing ? "#3c6cc1" : "#2d2d2d"
                                            radius: 8
                                            anchors.left: msg.outgoing ? undefined : parent.left
                                            anchors.right: msg.outgoing ? parent.right : undefined

                                            Text {
                                                text: msg.text
                                                wrapMode: Text.WordWrap
                                                color: "white"
                                                anchors.margins: 10
                                                anchors.fill: parent
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
                                spacing: 8

                                TextField {
                                    id: input
                                    placeholderText: qsTr("Сообщение (шифруется на клиенте перед отправкой)…")
                                    Layout.fillWidth: true
                                    onAccepted: sendButton.clicked()
                                }

                                Button {
                                    id: sendButton
                                    text: qsTr("Отправить")
                                    onClicked: {
                                        if (input.text.length === 0)
                                            return
                                        App.send(input.text)
                                        input.text = ""
                                    }
                                }
                            }
                        }
                    }

                    GroupBox {
                        title: qsTr("Server debug log")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180

                        ListView {
                            id: logView
                            anchors.fill: parent
                            clip: true
                            model: App.serverLog
                            delegate: Label {
                                text: modelData
                                font.family: "monospace"
                                color: "#cccccc"
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

    Component.onCompleted: App.refreshUsers()
}
