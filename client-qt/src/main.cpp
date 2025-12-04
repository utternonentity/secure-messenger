#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "AppController.h"

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName(QStringLiteral("SecureMessenger"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("example.com"));
    QCoreApplication::setApplicationName(QStringLiteral("SecureMessengerClient"));
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Secure Messenger Qt client"));
    parser.addHelpOption();
    QCommandLineOption serverIpOption(QStringList() << QStringLiteral("s") << QStringLiteral("server-ip"),
                                      QStringLiteral("IP-адрес сервера HTTP API (по умолчанию 127.0.0.1)."),
                                      QStringLiteral("ip"));
    parser.addOption(serverIpOption);
    parser.process(app);

    QString apiBaseUrlOverride = parser.value(serverIpOption).trimmed();

    QQmlApplicationEngine engine;

    AppController controller(apiBaseUrlOverride);
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
