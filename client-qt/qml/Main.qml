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
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: App && App.registered ? 1 : 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 40
                spacing: 24

                Pane {
                    Layout.fillWidth: true
                    padding: 24
                    background: Rectangle {
                        color: panelColor
                        radius: 16
                        border.color: panelBorder
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Label {
                            text: qsTr("Secure Messenger Demo")
                            font.pixelSize: 22
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: subtleText
                            text: qsTr("Демонстрационный клиент защищённого корпоративного мессенджера. Использует mTLS, каталоги пользователей и репликацию переписки для изолированной тестовой среды.")
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: subtleText
                            text: qsTr("Перед началом работы укажите никнейм, под которым устройство будет входить в систему.")
                        }
                    }
                }

                Pane {
                    Layout.fillWidth: true
                    padding: 24
                    background: Rectangle {
                        color: panelColor
                        radius: 16
                        border.color: panelBorder
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 16

                        function submitRegistration() {
                            const trimmed = nicknameField.text.trim();
                            if (trimmed.length < 3) {
                                nicknameField.forceActiveFocus();
                                return;
                            }
                            if (App && App.completeRegistration)
                                App.completeRegistration(trimmed);
                        }

                        Label {
                            text: qsTr("Регистрация устройства")
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: subtleText
                            text: qsTr("Никнейм отображается для остальных участников и сохраняется локально на этом устройстве.")
                        }

                        TextField {
                            id: nicknameField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Ваш никнейм")
                            selectByMouse: true
                            focus: true
                            onAccepted: submitRegistration()
                            Component.onCompleted: forceActiveFocus()
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Button {
                                id: registerButton
                                text: qsTr("Зарегистрироваться")
                                icon.name: "user"
                                enabled: nicknameField.text.trim().length >= 3
                                onClicked: submitRegistration()
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: subtleText
                                visible: nicknameField.text.trim().length < 3
                                text: qsTr("Минимум 3 символа. Вы всегда можете изменить никнейм в настройках устройства.")
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 18
        

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
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
                SplitView.minimumWidth: 320
                SplitView.preferredWidth: 380
                SplitView.maximumWidth: 460

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 16

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
                                wrapMode: Text.WordWrap
                            }

                            Flow {
                                width: parent.width
                                spacing: 6
                                visible: App && App.authInfo && App.authInfo.roles && App.authInfo.roles.length > 0

                                Repeater {
                                    model: App && App.authInfo && App.authInfo.roles ? App.authInfo.roles : []

                                    delegate: Rectangle {
                                        radius: 14
                                        color: "#1f2937"
                                        border.color: panelBorder
                                        implicitWidth: roleLabel.implicitWidth + 20
                                        implicitHeight: roleLabel.implicitHeight + 8

                                        Label {
                                            id: roleLabel
                                            anchors.centerIn: parent
                                            text: String(modelData)
                                            color: "#d1d5db"
                                            font.pixelSize: 12
                                        }
                                    }
                                }
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

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: qsTr("Сервисы доступны через единый защищённый сервер: каталог пользователей, обмен сообщениями, аудит, управление устройствами.")
                                color: subtleText
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
                                    text: qsTr("Активные пользователи")
                                    font.bold: true
                                    font.pixelSize: 16
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    text: qsTr("%1 онлайн").arg(App && App.userList ? App.userList.length : 0)
                                    color: subtleText
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: subtleText
                                visible: App && App.userList && App.userList.length > 0
                                text: App && App.userList
                                      ? qsTr("Directory Service вернул %1 профиля (включая ваш аккаунт).").arg(App.userList.length)
                                      : ""
                            }

                            StackLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                currentIndex: App && App.userList && App.userList.length > 0 ? 1 : 0

                                Item {
                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 8

                                        Label {
                                            text: qsTr("Пока нет данных каталога")
                                            font.pixelSize: 16
                                            font.bold: true
                                        }

                                        Label {
                                            text: qsTr("Нажмите \"Обновить справочник\", чтобы запросить актуальный список пользователей")
                                            color: subtleText
                                            wrapMode: Text.WordWrap
                                            horizontalAlignment: Text.AlignHCenter
                                            Layout.preferredWidth: 240
                                        }
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
                                        readonly property bool isSelf: String(entry.userId || "") === (App && App.authInfo ? String(App.authInfo.userId || "") : "")
                                        implicitHeight: card.implicitHeight + 12

                                        Rectangle {
                                            id: card
                                            anchors {
                                                top: parent.top
                                                left: parent.left
                                                right: parent.right
                                                margins: 6
                                            }
                                            radius: 12
                                            color: isSelf ? "#243046" : "#1b2735"
                                            border.color: panelBorder
                                            implicitHeight: contentColumn.implicitHeight + 24

                                            ColumnLayout {
                                                id: contentColumn
                                                anchors.fill: parent
                                                anchors.margins: 16
                                                spacing: 12

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 12

                                                    Rectangle {
                                                        width: 40
                                                        height: 40
                                                        radius: 20
                                                        color: isSelf ? "#4f83ff" : "#374151"

                                                        Label {
                                                            anchors.centerIn: parent
                                                            text: String(entry.displayName || "?").charAt(0)
                                                            font.pixelSize: 18
                                                            font.bold: true
                                                        }
                                                    }

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 2

                                                        Label {
                                                            Layout.fillWidth: true
                                                            text: String(entry.displayName || "")
                                                            font.bold: true
                                                            font.pixelSize: 15
                                                        }

                                                        Label {
                                                            Layout.fillWidth: true
                                                            text: qsTr("ID: %1").arg(String(entry.userId || ""))
                                                            color: subtleText
                                                            font.pixelSize: 12
                                                        }
                                                    }

                                                    Rectangle {
                                                        width: 10
                                                        height: 10
                                                        radius: 5
                                                        color: entry.online === false ? "#f87171" : "#34d399"
                                                        visible: entry.online !== undefined
                                                        Layout.alignment: Qt.AlignVCenter
                                                    }
                                                }

                                                ColumnLayout {
                                                    id: deviceList
                                                    Layout.fillWidth: true
                                                    spacing: 8

                                                    Repeater {
                                                        model: entry.devices || []

                                                        delegate: Rectangle {
                                                            Layout.fillWidth: true
                                                            radius: 8
                                                            color: (modelData["revoked"] === true) ? "#3f1d29" : "#1f2d3d"
                                                            border.color: panelBorder
                                                            implicitHeight: deviceContent.implicitHeight + 12

                                                            ColumnLayout {
                                                                id: deviceContent
                                                                anchors.fill: parent
                                                                anchors.margins: 12
                                                                spacing: 8

                                                                ColumnLayout {
                                                                    Layout.fillWidth: true
                                                                    spacing: 4

                                                                    Label {
                                                                        Layout.fillWidth: true
                                                                        text: qsTr("Устройство %1").arg(String(modelData["deviceId"] || ""))
                                                                        font.pixelSize: 13
                                                                        font.bold: true
                                                                    }

                                                                    Label {
                                                                        Layout.fillWidth: true
                                                                        text: String(modelData["label"] || "")
                                                                        color: subtleText
                                                                        font.pixelSize: 12
                                                                        visible: text.length > 0
                                                                    }

                                                                    Label {
                                                                        Layout.fillWidth: true
                                                                        text: {
                                                                            const cert = String(modelData["certificate"] || "")
                                                                            return cert.length > 36 ? cert.slice(0, 36) + "…" : cert
                                                                        }
                                                                        font.family: "monospace"
                                                                        font.pixelSize: 11
                                                                        color: subtleText
                                                                        wrapMode: Text.WrapAnywhere
                                                                    }

                                                                    Label {
                                                                        Layout.fillWidth: true
                                                                        text: (modelData["revoked"] === true)
                                                                              ? qsTr("Статус: отозван")
                                                                              : qsTr("Статус: активен")
                                                                        color: (modelData["revoked"] === true) ? "#f87171" : "#34d399"
                                                                        font.pixelSize: 12
                                                                    }
                                                                }

                                                                RowLayout {
                                                                    Layout.fillWidth: true
                                                                    spacing: 8

                                                                    Item { Layout.fillWidth: true }

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

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 8
                                                    readonly property string myId: App && App.authInfo ? String(App.authInfo.userId || "") : ""
                                                    visible: String(entry.userId || "") !== myId
                                                    Layout.preferredHeight: visible ? implicitHeight : 0
                                                    Layout.minimumHeight: 0
                                                    Layout.maximumHeight: visible ? implicitHeight : 0

                                                    Label {
                                                        text: qsTr("Личное сообщение")
                                                        color: subtleText
                                                    }

                                                    Item { Layout.fillWidth: true }

                                                    Button {
                                                        text: qsTr("Написать")
                                                        icon.name: "chat"
                                                        enabled: App && App.startConversationWith
                                                        onClicked: if (App) App.startConversationWith(String(entry.userId || ""))
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

            Item {
                SplitView.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
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
                                spacing: 12

                                ColumnLayout {
                                    spacing: 2

                                    Label {
                                        text: qsTr("Messaging Service")
                                        font.bold: true
                                        font.pixelSize: 16
                                    }

                                    Label {
                                        text: App && App.currentConversation ? qsTr("Подписка: %1").arg(String(App.currentConversation)) : qsTr("Выберите пользователя или введите conversation id")
                                        color: subtleText
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }
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
