import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    width: 900
    height: 600
    visible: true
    title: qsTr("SM — Secure Messenger")

    header: ToolBar {
        contentItem: RowLayout {
            anchors.fill: parent
            Label {
                text: qsTr("Подключено (mTLS) — MVP")
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 12

        RowLayout {
            anchors.fill: parent
            spacing: 12

            // Список пользователей
            ListView {
                id: users
                Layout.preferredWidth: 250
                Layout.fillHeight: true
                clip: true
                model: ["Иван Петров", "Мария Сидорова"]
                delegate: ItemDelegate {
                    width: ListView.view.width
                    text: modelData
                }
            }

            // Чат + поле ввода
            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    id: chat
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true

                    TextField {
                        id: input
                        placeholderText: qsTr("Сообщение (уже шифруем на клиенте)…")
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Отправить")
                        onClicked: {
                            if (input.text.length === 0) return;
                            App.send(input.text);
                            input.text = "";
                        }
                    }
                }
            }
        }
    }
}
