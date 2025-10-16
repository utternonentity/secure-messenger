import QtQuick 2.15
import QtQuick.Controls 2.15


ApplicationWindow {
width: 900; height: 600; visible: true; title: "SM — Secure Messenger"
header: ToolBar { Label { text: "Подключено (mTLS) — MVP" } }
Row {
anchors.fill: parent; spacing: 12; padding: 12
ListView {
id: users; width: 250; model: ["Иван Петров", "Мария Сидорова"]
delegate: ItemDelegate { text: modelData }
}
Column {
spacing: 8
TextArea { id: chat; readOnly: true; width: 580; height: 480 }
Row {
spacing: 8
TextField { id: input; width: 500; placeholderText: "Сообщение (уже шифруем на клиенте)…" }
Button { text: "Отправить"; onClicked: App.send(input.text); input.text = "" }
}
}
}
}