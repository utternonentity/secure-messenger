import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: window
    width: 1240
    height: 780
    minimumWidth: 960
    minimumHeight: 600
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
        id: mainToolbar
        padding: 12
        visible: App && App.registered
        height: visible ? implicitHeight : 0
        enabled: visible
        background: Rectangle { color: panelColor }

        contentItem: RowLayout {
            anchors.fill: parent
            spacing: 12

            ColumnLayout {
                spacing: 2

                Label {
                    text: qsTr("Защищённое подключение · демонстрация серверных сервисов")
                    font.pixelSize: 18
                    font.bold: true
                }

                Label {
                    text: App && App.clusterInfo ? App.clusterInfo : qsTr("mTLS, аудит, каталог, сообщения")
                    color: subtleText
                    font.pixelSize: 12
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Синхронизировать")
                icon.name: "refresh"
                onClicked: if (App && App.simulatePull) App.simulatePull()
            }

            Button {
                text: qsTr("Обновить справочник")
                icon.name: "reload"
                onClicked: if (App && App.refreshUsers) App.refreshUsers()
            }

            Button {
                text: qsTr("Сменить пользователя")
                icon.name: "logout"
                onClicked: if (App && App.resetRegistration) App.resetRegistration()
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: App && App.registered ? 1 : 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            SplitView {
                anchors.fill: parent
                orientation: Qt.Horizontal
                handle: Rectangle {
                    implicitWidth: 18
                    color: "transparent"

                    Rectangle {
                        anchors.centerIn: parent
                        width: 2
                        height: parent.height - 12
                        radius: 1
                        color: panelBorder
                    }
                }

                Item {
                    SplitView.minimumWidth: 280
                    SplitView.preferredWidth: 340
                    SplitView.maximumWidth: 420

                    Rectangle {
                        anchors.fill: parent
                        color: panelColor
                        border.color: panelBorder

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: qsTr("Активные чаты")
                                    font.bold: true
                                    font.pixelSize: 16
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    text: qsTr("%1").arg(App && App.conversationList ? App.conversationList.length : 0)
                                    color: subtleText
                                    font.pixelSize: 12
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: qsTr("Новый чат")
                                icon.name: "chat"
                                onClicked: {
                                    if (!App)
                                        return
                                    newChatField.text = ""
                                    newChatDialog.open()
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ListView {
                                    id: conversationsView
                                    anchors.fill: parent
                                    clip: true
                                    spacing: 8
                                    model: (App && App.conversationList) ? App.conversationList : []
                                    boundsBehavior: Flickable.StopAtBounds
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    delegate: Rectangle {
                                        width: conversationsView.width
                                        property var entry: modelData
                                        property string conversationId: String(entry["id"] || "")
                                        property bool active: App && App.currentConversation === conversationId
                                        radius: 12
                                        border.width: active ? 2 : 1
                                        border.color: active ? Material.accent : panelBorder
                                        color: active ? "#1f2937" : "transparent"
                                        implicitHeight: contentColumn.implicitHeight + 16

                                        ColumnLayout {
                                            id: contentColumn
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            spacing: 4

                                            RowLayout {
                                                Layout.fillWidth: true

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: String(entry["title"] || conversationId)
                                                    font.bold: true
                                                    font.pixelSize: 15
                                                    elide: Text.ElideRight
                                                }

                                                Label {
                                                    text: String(entry["lastTimestamp"] || "")
                                                    color: subtleText
                                                    font.pixelSize: 12
                                                }
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                visible: String(entry["subtitle"] || "").length > 0
                                                text: String(entry["subtitle"] || "")
                                                color: subtleText
                                                font.pixelSize: 11
                                                elide: Text.ElideRight
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: String(entry["lastMessage"] || "")
                                                color: "#e5e7eb"
                                                font.pixelSize: 12
                                                wrapMode: Text.WordWrap
                                                maximumLineCount: 2
                                                elide: Text.ElideRight
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: if (App) App.currentConversation = conversationId
                                        }
                                    }
                                }

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    visible: conversationsView.count === 0
                                    spacing: 8

                                    Label {
                                        text: qsTr("Чатов пока нет")
                                        font.pixelSize: 16
                                        font.bold: true
                                    }

                                    Label {
                                        text: qsTr("Создайте личный чат или дождитесь входящих сообщений.")
                                        color: subtleText
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.preferredWidth: 220
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    SplitView.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 16

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
                                    spacing: 8

                                    Label {
                                        Layout.fillWidth: true
                                        text: App && App.currentConversation
                                                  ? (function() {
                                                        var id = String(App.currentConversation)
                                                        var list = App.conversationList || []
                                                        for (var i = 0; i < list.length; ++i) {
                                                            var entry = list[i]
                                                            if (String(entry["id"]) === id)
                                                                return String(entry["title"] || id)
                                                        }
                                                        return id
                                                    })()
                                                  : qsTr("Чат не выбран")
                                        font.bold: true
                                        font.pixelSize: 18
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: App && App.currentConversation ? String(App.currentConversation) : ""
                                        color: subtleText
                                        font.pixelSize: 12
                                        visible: App && App.currentConversation
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 10
                                    color: "#111827"
                                    border.color: panelBorder

                                    ListView {
                                        id: chatView
                                        anchors.fill: parent
                                        anchors.margins: 12
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
        }

        
    }
    
    

    Dialog {
        id: newChatDialog
        modal: true
        x: (window.width - width) / 2
        y: (window.height - height) / 2
        width: Math.min(360, window.width - 80)
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        title: qsTr("Новый чат")

        onAccepted: {
            const trimmed = newChatField.text.trim()
            if (App && App.startConversationWith && trimmed.length > 0)
                App.startConversationWith(trimmed)
        }

        onClosed: newChatField.text = ""

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Label {
                text: qsTr("Введите идентификатор пользователя для начала диалога")
                wrapMode: Text.WordWrap
            }

            TextField {
                id: newChatField
                Layout.fillWidth: true
                placeholderText: qsTr("user-id")
                selectByMouse: true
                onAccepted: {
                    if (text.trim().length > 0)
                        newChatDialog.accept()
                }
            }

            Label {
                text: qsTr("Чат будет создан с использованием общего идентификатора канала")
                color: subtleText
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        footer: RowLayout {
            spacing: 12
            Layout.fillWidth: true

            Button {
                text: qsTr("Отмена")
                onClicked: newChatDialog.close()
            }

            Item { Layout.fillWidth: true }

            Button {
                id: confirmNewChat
                text: qsTr("Создать")
                icon.name: "chat"
                enabled: newChatField.text.trim().length > 0
                onClicked: {
                    if (enabled)
                        newChatDialog.accept()
                }
            }
        }

        Component.onCompleted: newChatField.text = ""
        onOpened: newChatField.forceActiveFocus()
    }

    Connections {
        target: App
        function onRegistrationChanged() {
            if (App && App.registered && App.refreshUsers)
                App.refreshUsers()
        }
    }

    Component.onCompleted: if (App && App.registered && App.refreshUsers) App.refreshUsers()
}
