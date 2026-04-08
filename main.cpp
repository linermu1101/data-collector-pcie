#include "mainprocess.h"

#include <QApplication>

#include "QsLog.h"
#include "QsLogDest.h"


bool logConfig()
{
    QsLogging::Logger& logger = QsLogging::Logger::instance();
    // Trace、Debug、Info、Warn、Error、Fatal、Off
    logger.setLoggingLevel(QsLogging::TraceLevel);

    // 设置log位置
    QString logFolderPath = QDir(QCoreApplication::applicationDirPath()).filePath("log");
    QDir logDir(logFolderPath);
    if (!logDir.exists())
    {
        if (!logDir.mkpath("."))
        {
            qWarning() << "Failed to create log directory:" << logFolderPath;
            return false;
        }
    }
    QString sLogPath = logDir.filePath("log.txt");

    // 添加两个destination
    QsLogging::DestinationPtr fileDestination(QsLogging::DestinationFactory::MakeFileDestination(
        sLogPath, QsLogging::EnableLogRotation, QsLogging::MaxSizeBytes(512000), QsLogging::MaxOldLogCount(10)));
    QsLogging::DestinationPtr debugDestination(QsLogging::DestinationFactory::MakeDebugOutputDestination());

    logger.addDestination(debugDestination);
    logger.addDestination(fileDestination);

    return true;
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    logConfig();

    // LOG samples
    // QLOG_TRACE() << "Here's a" << QString::fromUtf8("trace") << "message";
    // QLOG_DEBUG() << "Here's a" << static_cast<int>(QsLogging::DebugLevel) << "message";
    QLOG_INFO() << "欢迎使用铀青软件 LOG LEVER" << static_cast<int>(QsLogging::DebugLevel);
    // QLOG_WARN()  << "Uh-oh!";
    // QLOG_ERROR() << "An error has occurred";
    // QLOG_FATAL() << "Fatal error!";

    MainProcess w;
    w.show();
    return a.exec();
}
