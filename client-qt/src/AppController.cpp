#include "AppController.h"
#include <QDebug>


AppController::AppController(QObject* parent) : QObject(parent) {}


void AppController::send(const QString& text) {
// TODO: здесь: шифрование (E2E) + gRPC вызов Messaging.Send
qDebug() << "MVP send:" << text;
}