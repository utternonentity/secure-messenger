import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQml 2.15
import QtQuick.Dialogs 6.4

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
    property var messageDrafts: ({})
    property string previousConversationId: ""

    function syncDraftOnSwitch(newConversationId) {
        if (!input)
            return
        if (previousConversationId && previousConversationId.length > 0) {
            messageDrafts[previousConversationId] = input.text
        }
        previousConversationId = newConversationId || ""
        if (!previousConversationId || previousConversationId.length === 0) {
            input.text = ""
        } else {
            input.text = messageDrafts[previousConversationId] || ""
        }
    }

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

            Pane {
                anchors.centerIn: parent
                width: Math.min(window.width - 120, 560)
                padding: 0
                Material.elevation: 6
                background: Rectangle {
                    color: panelColor
                    radius: 18
                    border.color: panelBorder
                    border.width: 1
                }

                ScrollView {
                    id: authScroll
                    anchors.fill: parent
                    anchors.margins: 28
                    clip: true
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    contentWidth: availableWidth

                    ColumnLayout {
                        id: authForm
                        width: authScroll.availableWidth
                        spacing: 24

                        property bool registrationMode: false
                        property string formError: ""
                        property bool busy: App && App.authBusy

                          function performPrimaryAction() {
                              if (authForm.busy) {
                                  return;
                              }
                              if (!App) {
                                  formError = qsTr("Сервис недоступен");
                                  return;
                              }
                              if (registrationMode && authPassword.text !== authPasswordConfirm.text) {
                                  formError = qsTr("Пароли не совпадают");
                                  return;
                              }
                              var result = registrationMode
                                      ? (App.completeRegistration
                                                 ? App.completeRegistration(authNickname.text,
                                                                            authPassword.text,
                                                                            authCertificate.text)
                                               : qsTr("Сервис недоступен"))
                                    : (App.authenticate
                                               ? App.authenticate(authNickname.text,
                                                                  authPassword.text,
                                                                  authCertificate.text)
                                               : qsTr("Сервис недоступен"));
                                  if (result && result.length > 0) {
                                      formError = result;
                                  } else {
                                      formError = "";
                                      authPassword.text = "";
                                      authPasswordConfirm.text = "";
                                      authCertificate.text = "";
                                  }
                              }

                        Label {
                            text: authForm.registrationMode ? qsTr("Создайте новый профиль Secure Messenger")
                                                             : qsTr("Добро пожаловать в Secure Messenger")
                            font.pixelSize: 26
                            font.bold: true
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Label {
                            text: authForm.registrationMode
                                  ? qsTr("Укажите никнейм, пароль и сертификат устройства для регистрации.")
                                  : qsTr("Для входа укажите никнейм, пароль и сертификат зарегистрированного устройства.")
                            color: subtleText
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                          Frame {
                              Layout.fillWidth: true
                              implicitHeight: authFields.implicitHeight + 36
                              background: Rectangle {
                                  color: "#111827"
                                  radius: 12
                                  border.color: panelBorder
                              }

                              ColumnLayout {
                                  id: authFields
                                  anchors.fill: parent
                                  anchors.margins: 16
                                  spacing: 16

                                TextField {
                                    id: authNickname
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Никнейм")
                                    selectByMouse: true
                                    enabled: !authForm.busy
                                    onAccepted: authPassword.forceActiveFocus()
                                    Component.onCompleted: {
                                        if (!App || !App.registered)
                                            forceActiveFocus()
                                    }

                                    Connections {
                                        target: App
                                          function onRegistrationChanged() {
                                              if (App && !App.registered) {
                                                  authNickname.forceActiveFocus()
                                                  authPassword.text = ""
                                                  authPasswordConfirm.text = ""
                                                  authCertificate.text = ""
                                                  authForm.formError = ""
                                              }
                                          }
                                      }
                                }

                                  TextField {
                                      id: authPassword
                                      Layout.fillWidth: true
                                      placeholderText: qsTr("Пароль")
                                      echoMode: TextInput.Password
                                      selectByMouse: true
                                      enabled: !authForm.busy
                                      onAccepted: authForm.registrationMode
                                                  ? authPasswordConfirm.forceActiveFocus()
                                                  : authCertificate.forceActiveFocus()
                                  }

                                  TextField {
                                      id: authPasswordConfirm
                                      Layout.fillWidth: true
                                      visible: authForm.registrationMode
                                      placeholderText: qsTr("Подтвердите пароль")
                                      echoMode: TextInput.Password
                                      selectByMouse: true
                                      enabled: !authForm.busy
                                      onAccepted: authCertificate.forceActiveFocus()
                                  }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12

                                        TextField {
                                            id: authCertificate
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("Путь к сертификату устройства (PEM/DER)")
                                            selectByMouse: true
                                            enabled: !authForm.busy
                                            onAccepted: authForm.performPrimaryAction()
                                        }

                                        Button {
                                            Layout.preferredWidth: 120
                                            text: qsTr("Выбрать")
                                            icon.name: "folder"
                                            enabled: !authForm.busy
                                            onClicked: certFileDialog.open()
                                        }
                                    }

                                    Label {
                                        text: authForm.registrationMode
                                              ? qsTr("Этот сертификат будет привязан к устройству при регистрации.")
                                              : qsTr("Мы сверим выбранный сертификат с привязанным к вашей учётной записи.")
                                        color: subtleText
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }

                        Label {
                            visible: authForm.formError.length > 0
                            text: authForm.formError
                            color: "#f87171"
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                              Button {
                                  id: primaryAction
                                  Layout.fillWidth: true
                                  Layout.preferredWidth: 200
                                  text: authForm.registrationMode ? qsTr("Зарегистрироваться") : qsTr("Войти")
                                  icon.name: authForm.registrationMode ? "account-circle" : "login"
                                  enabled: !authForm.busy
                                           && authNickname.text.trim().length > 0
                                           && authPassword.text.length > 0
                                           && (!authForm.registrationMode
                                               || (authPasswordConfirm.text.length > 0
                                                   && authPasswordConfirm.text === authPassword.text))
                                           && authCertificate.text.trim().length > 0
                                           && App
                                           && (authForm.registrationMode ? App.completeRegistration : App.authenticate)
                                  onClicked: authForm.performPrimaryAction()
                              }

                                BusyIndicator {
                                    Layout.preferredHeight: primaryAction.implicitHeight
                                    Layout.preferredWidth: Layout.preferredHeight
                                    running: authForm.busy
                                    visible: running
                                }
                            }

                          }

                          Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: toggleLabel.implicitHeight + 16
                            radius: 12
                            color: "transparent"
                            border.color: "transparent"

                            Label {
                                id: toggleLabel
                                anchors.centerIn: parent
                                text: authForm.registrationMode ? qsTr("Уже есть аккаунт? Войдите.")
                                                               : qsTr("Нет аккаунта? Зарегистрируйтесь.")
                                color: subtleText
                                font.pixelSize: 13
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: !authForm.busy
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                      onClicked: {
                                          authForm.registrationMode = !authForm.registrationMode
                                          authForm.formError = ""
                                          authPassword.text = ""
                                          authPasswordConfirm.text = ""
                                          authCertificate.text = ""
                                          authNickname.forceActiveFocus()
                                      }
                                  }
                              }

                        Frame {
                            Layout.fillWidth: true
                            Layout.minimumHeight: 160
                            Layout.maximumHeight: Math.max(180, window.height * 0.25)
                            background: Rectangle {
                                color: "#111827"
                                radius: 12
                                border.color: panelBorder
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        text: qsTr("Журнал событий")
                                        font.pixelSize: 14
                                        font.bold: true
                                    }

                                    Item { Layout.fillWidth: true }

                                    ToolButton {
                                        visible: App && App.serverLog && App.serverLog.length > 0
                                        icon.name: "refresh"
                                        onClicked: preregLog.positionViewAtEnd()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Прокрутить к последней записи")
                                    }
                                }

                                ListView {
                                    id: preregLog
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    spacing: 4
                                    model: (App && App.serverLog) ? App.serverLog : []
                                    boundsBehavior: Flickable.StopAtBounds
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    delegate: Item {
                                        width: preregLog.width
                                        implicitHeight: logText.implicitHeight + 4

                                        Label {
                                            id: logText
                                            anchors.fill: parent
                                            anchors.margins: 2
                                            text: String(modelData)
                                            font.family: "monospace"
                                            font.pixelSize: 12
                                            color: "#d1d5db"
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                FileDialog {
                    id: certFileDialog
                    title: qsTr("Выберите сертификат устройства")
                    nameFilters: [qsTr("Сертификаты (*.pem *.cer *.crt *.der)")]
                    onAccepted: {
                        // Qt 6: selectedFile (url) или selectedFiles (list<url>)
                        var u = certFileDialog.selectedFile
                        if (!u && certFileDialog.selectedFiles && certFileDialog.selectedFiles.length > 0)
                            u = certFileDialog.selectedFiles[0]

                        if (u) {
                            // универсально: если это QUrl — toLocalFile(); иначе строка
                            authCertificate.text = (u.toLocalFile ? u.toLocalFile() : String(u))
                        }
                    }
                }
            }
        }

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
                                        property int unreadCount: Number(entry["unreadCount"] || 0)
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

                                                Rectangle {
                                                    visible: unreadCount > 0
                                                    color: Material.accent
                                                    radius: 10
                                                    implicitHeight: 20
                                                    implicitWidth: Math.max(unreadBadge.implicitWidth + 10, 20)
                                                    Layout.alignment: Qt.AlignVCenter

                                                    Label {
                                                        id: unreadBadge
                                                        anchors.centerIn: parent
                                                        text: unreadCount
                                                        font.pixelSize: 12
                                                        font.bold: true
                                                        color: "white"
                                                    }
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

                                                    Item {
                                                        width: bubbleContent.width
                                                        height: message && message.outgoing ? 16 : 0
                                                        visible: message && message.outgoing

                                                        Row {
                                                            anchors.right: parent.right
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            spacing: 4

                                                            Label {
                                                                text: "\u2713"
                                                                color: (message && message.delivered) ? "#93c5fd" : "#6b7280"
                                                                font.pixelSize: 12
                                                                font.bold: true
                                                            }

                                                            Label {
                                                                visible: message && message.read
                                                                text: "\u2713"
                                                                color: Material.accent
                                                                font.pixelSize: 12
                                                                font.bold: true
                                                            }
                                                        }
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
                                        enabled: App && App.currentConversation && App.currentConversation.length > 0
                                        readOnly: !enabled
                                        onAccepted: sendButton.clicked()
                                        onTextChanged: {
                                            if (App && App.currentConversation && App.currentConversation.length > 0)
                                                messageDrafts[App.currentConversation] = text
                                        }
                                    }

                                    Button {
                                        id: sendButton
                                        text: qsTr("Отправить")
                                        enabled: App && App.send && App.currentConversation && App.currentConversation.length > 0
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
                            Layout.preferredHeight: 160
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
        width: Math.min(460, window.width - 80)
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        title: qsTr("Новый чат")
        property string selectedUserId: ""

        onAccepted: {
            const trimmed = newChatField.text.trim()
            const target = newChatDialog.selectedUserId.length > 0 ? newChatDialog.selectedUserId : trimmed
            if (App && App.startConversationWith && target.length > 0)
                App.startConversationWith(target)
        }

        onClosed: {
            newChatField.text = ""
            newChatDialog.selectedUserId = ""
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            Label {
                Layout.fillWidth: true
                text: qsTr("Введите никнейм или выберите запись из справочника")
                wrapMode: Text.WordWrap
            }

            TextField {
                id: newChatField
                Layout.fillWidth: true
                placeholderText: qsTr("Никнейм или идентификатор")
                selectByMouse: true
                onAccepted: {
                    if (text.trim().length > 0)
                        newChatDialog.accept()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                radius: 10
                color: "#111827"
                border.color: panelBorder

                ListView {
                    id: directoryList
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    spacing: 8
                    model: (App && App.userList) ? App.userList : []
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        width: directoryList.width
                        property var entry: modelData
                        property string userId: String(entry["userId"] || "")
                        property string nickname: String(entry["nickname"] || qsTr("Неизвестный"))
                        property string currentUser: String(App && App.authInfo ? App.authInfo.userId : "")
                        visible: userId.length > 0 && userId !== currentUser
                        implicitHeight: visible ? delegateContent.implicitHeight + 12 : 0
                        radius: 8
                        border.width: newChatDialog.selectedUserId === userId ? 2 : 1
                        border.color: newChatDialog.selectedUserId === userId ? Material.accent : panelBorder
                        color: newChatDialog.selectedUserId === userId ? "#1f2937" : "transparent"

                        ColumnLayout {
                            id: delegateContent
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 2

                            Label {
                                text: nickname
                                font.pixelSize: 15
                                font.bold: true
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (!visible)
                                    return
                                newChatDialog.selectedUserId = userId
                                newChatField.text = nickname
                                newChatDialog.accept()
                            }
                        }
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - 32
                    spacing: 6
                    visible: directoryList.count === 0 || directoryList.contentHeight === 0

                    Label {
                        text: qsTr("Справочник пока пуст. Подождите синхронизации или попробуйте позже.")
                        color: subtleText
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

              Label {
                  text: qsTr("Чат будет создан с использованием общего идентификатора канала")
                  color: subtleText
                  font.pixelSize: 12
                  wrapMode: Text.WordWrap
                  Layout.fillWidth: true
                  Layout.bottomMargin: 18
              }
        }

        footer: RowLayout {
            spacing: 12
            Layout.fillWidth: true

            Button {
                text: qsTr("Отмена")
                onClicked: newChatDialog.close()
                Layout.leftMargin: 10
            }

            Item { Layout.fillWidth: true }

            Button {
                id: confirmNewChat
                text: qsTr("Создать")
                icon.name: "chat"
                enabled: newChatField.text.trim().length > 0 || newChatDialog.selectedUserId.length > 0
                onClicked: {
                    if (enabled)
                        newChatDialog.accept()
                }
                Layout.rightMargin: 10
            }
        }

        Component.onCompleted: newChatField.text = ""
        onOpened: {
            if (App && App.refreshUsers)
                App.refreshUsers()
            newChatDialog.selectedUserId = ""
            newChatField.forceActiveFocus()
        }
    }

    Connections {
        target: App
        function onCurrentConversationChanged() {
            window.syncDraftOnSwitch(App.currentConversation)
        }
        function onRegistrationChanged() {
            if (App && App.registered && App.refreshUsers)
                App.refreshUsers()
        }
    }

    Component.onCompleted: {
        window.previousConversationId = App && App.currentConversation ? App.currentConversation : ""
        window.syncDraftOnSwitch(window.previousConversationId)
        if (App && App.registered && App.refreshUsers)
            App.refreshUsers()
    }
}
