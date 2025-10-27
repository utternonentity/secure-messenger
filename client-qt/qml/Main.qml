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
                onClicked: if (App && App.refreshUsers) App.refreshUsers()
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

            // ===== WhoAmI =====
            GroupBox {
                title: qsTr("Auth.WhoAmI")
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 6
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Пользователь: %1 (%2)")
                              .arg(String(App ? App.authInfoDisplayName : ""))
                              .arg(String(App ? App.authInfoUserId : ""))
                        font.pixelSize: 16
                    }
                    Label {
                        text: qsTr("Роли: %1")
                              .arg((App && App.roles) ? App.roles.join(", ") : "")
                        color: "#cccccc"
                    }
                    Label {
                        text: qsTr("Активное устройство: %1")
                              .arg(String(App && App.authInfoDeviceId ? App.authInfoDeviceId : ""))
                        color: "#cccccc"
                    }

                    Frame {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 160

                        // Один TextArea без дублирующих свойств
                        TextArea {
                            id: authCertificate
                            anchors.fill: parent
                            anchors.margins: 8
                            readOnly: true
                            wrapMode: TextArea.WrapAnywhere
                            selectByMouse: true
                            font.family: "monospace"
                            text: String(App && App.authCertificate ? App.authCertificate : "")
                            background: Rectangle { color: "transparent" }
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // ===== Directory (упрощённо: QStringList) =====
                GroupBox {
                    title: qsTr("Directory Service")
                    Layout.fillHeight: true
                    Layout.preferredWidth: 360

                    ListView {
                        id: userList
                        anchors.fill: parent
                        clip: true
                        spacing: 6
                        model: (App && App.userList) ? App.userList : []
                        delegate: ItemDelegate {
                            width: ListView.view.width
                            text: String(modelData)
                        }
                    }
                }

                // ===== Messaging =====
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
                                    text: String(App ? App.currentConversation : "")
                                    onTextEdited: { if (App) App.currentConversation = text }
                                    Connections {
                                        target: App
                                        function onCurrentConversationChanged() {
                                            conversationField.text = String(App.currentConversation)
                                        }
                                    }
                                }

                                Button {
                                    text: qsTr("Pull update")
                                    onClicked: if (App && App.simulatePull) App.simulatePull()
                                }
                            }

                            Frame {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ListView {
                                    id: chatView
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    clip: true
                                    spacing: 6
                                    model: (App && App.serverLog) ? App.serverLog : []
                                    delegate: Label {
                                        width: ListView.view.width
                                        text: String(modelData)
                                        wrapMode: Text.WordWrap
                                        color: "#dddddd"
                                        font.family: "monospace"
                                    }
                                    Connections {
                                        target: App
                                        function onServerLogChanged() { chatView.positionViewAtEnd() }
                                    }
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
                                        if (!App || input.text.length === 0) return
                                        App.send(input.text)
                                        input.text = ""
                                    }
                                }
                            }
                        }
                    }

                    // ===== Server debug log =====
                    GroupBox {
                        title: qsTr("Server debug log")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180

                        ListView {
                            id: logView
                            anchors.fill: parent
                            clip: true
                            model: (App && App.serverLog) ? App.serverLog : []
                            delegate: Label {
                                text: String(modelData)
                                font.family: "monospace"
                                color: "#cccccc"
                                wrapMode: Text.WordWrap
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
