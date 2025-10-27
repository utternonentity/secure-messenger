#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "AppController.h"

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName(QStringLiteral("SecureMessenger"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("example.com"));
    QCoreApplication::setApplicationName(QStringLiteral("SecureMessengerClient"));
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    AppController controller;
    engine.rootContext()->setContextProperty("App", &controller);

    // Если используешь qt_add_qml_module(sm_client ... QML_FILES qml/Main.qml),
    // путь обычно такой:
    const QUrl url(QStringLiteral("qrc:/smclient/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
