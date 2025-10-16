#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "AppController.h"


int main(int argc, char *argv[]) {
QGuiApplication app(argc, argv);
QQmlApplicationEngine engine;
AppController controller;
engine.rootContext()->setContextProperty("App", &controller);
const QUrl url(u"qrc:/Main.qml"_qs);
QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl) {
if (!obj && url == objUrl) QCoreApplication::exit(-1);
}, Qt::QueuedConnection);
engine.load(url);
return app.exec();
}