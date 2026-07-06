#include <QGuiApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <qqml.h>
#include <QtGlobal>
#include <QQuickStyle>

#include "src/app/appcontroller.h"
#include "src/game/survivorrenderitem.h"

namespace {

QString logFilePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("LanBoard-startup.log");
}

void appendLogLine(const QString &line)
{
    QFile file(logFilePath());
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << line << '\n';
}

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    const QString level = [type]() {
        switch (type) {
        case QtDebugMsg:   return QStringLiteral("DEBUG");
        case QtInfoMsg:    return QStringLiteral("INFO");
        case QtWarningMsg: return QStringLiteral("WARN");
        case QtCriticalMsg: return QStringLiteral("CRITICAL");
        case QtFatalMsg:   return QStringLiteral("FATAL");
        }
        return QStringLiteral("LOG");
    }();

    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    appendLogLine(QStringLiteral("[%1] %2 %3").arg(timestamp, level, message));

    if (type == QtFatalMsg)
        abort();
}

}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("LanBoard"));
    QCoreApplication::setApplicationName(QStringLiteral("LanBoard"));
    qInstallMessageHandler(messageHandler);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QFile::remove(logFilePath());
    appendLogLine(QStringLiteral("=== LanBoard startup ==="));
    appendLogLine(QStringLiteral("App dir: %1").arg(QCoreApplication::applicationDirPath()));

    QQmlApplicationEngine engine;
    qmlRegisterType<SurvivorRenderItem>("LanBoard", 1, 0, "SurvivorRenderItem");

    // Create and expose AppController to QML
    AppController appCtrl;
    engine.rootContext()->setContextProperty(QStringLiteral("AppCtrl"), &appCtrl);

    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
                         for (const QQmlError &warning : warnings)
                             appendLogLine(QStringLiteral("[QML WARNING] %1").arg(warning.toString()));
                     });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            appendLogLine(QStringLiteral("[ENGINE] objectCreationFailed emitted"));
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    appendLogLine(QStringLiteral("[ENGINE] loadFromModule(LanBoard, Main)"));
    engine.loadFromModule("LanBoard", "Main");
    appendLogLine(QStringLiteral("[ENGINE] rootObjects=%1").arg(engine.rootObjects().size()));

    return app.exec();
}
