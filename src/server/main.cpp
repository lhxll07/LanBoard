#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

#include "serverapp.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("LanBoardServer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("LanBoard dedicated server"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOption(
        {QStringLiteral("p"), QStringLiteral("port")},
        QStringLiteral("Port to listen on."),
        QStringLiteral("port"),
        QStringLiteral("44567"));
    parser.addOption(portOption);
    parser.process(app);

    bool ok = false;
    const int parsedPort = parser.value(portOption).toInt(&ok);
    if (!ok || parsedPort < 1 || parsedPort > 65535) {
        qCritical().noquote() << QStringLiteral("Invalid port: %1").arg(parser.value(portOption));
        return 1;
    }

    ServerApp server;
    if (!server.start(static_cast<quint16>(parsedPort))) {
        qCritical().noquote() << QStringLiteral("Failed to start server on port %1").arg(parsedPort);
        return 1;
    }

    return app.exec();
}
