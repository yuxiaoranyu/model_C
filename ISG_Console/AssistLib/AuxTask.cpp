#include "AuxTask.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QRegularExpression>
#include <QJsonValue>
#include <QStringList>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QStorageInfo>
#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>
#include <QProcess>
#include <QDirIterator>

QString AuxTask::s_logString;
QAtomicInt AuxTask::s_logCount;
QtMessageHandler AuxTask::s_messageHandler;

AuxTask::AuxTask()
{
    s_messageHandler = qInstallMessageHandler(outPutLog);

    moveToThread(&m_thread);
    m_thread.setObjectName("auxTask");
    m_thread.start();
}

AuxTask::~AuxTask()
{
    m_thread.quit();
    m_thread.wait();
    qInstallMessageHandler(nullptr);
}

void AuxTask::outPutLog(const QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex fileLock;
    QString qstrText;
    QString msgType;

    s_messageHandler(type, context, msg);

    switch (type)
    {
        case QtDebugMsg:
            return;
        case QtInfoMsg:
            msgType = "Info:";
            break;
        case QtWarningMsg:
            msgType = "Warning:";
            break;
        case QtCriticalMsg:
            msgType = "Critical:";
            break;
        case QtFatalMsg:
            msgType = "Fatal:";
            break;
        default:
            return;
    }

    QString cppfileName(context.file);
    //从全路径的代码文件名提取文件名，存在match.captured(1)中，match.captured(0)存放filename信息
    static QRegularExpression rxlen(R"((?:[/\\])?([^/\\]+)$)", QRegularExpression::OptimizeOnFirstUsageOption);
    /*
     * R"()"        表示对之间的内容不做转义
     * (?:[/\\])    表示匹配"杠反斜杠"，但不计入匹配结果
     * (?:[/\\])?   表示匹配"杠反斜杠"，可匹配零次或一次，不计入匹配结果
     * [^/\\]+      表示匹配一次或多次非"杠反斜杠"字符
     * $            针对杠反斜杠的匹配，取最后的匹配，作为输出
     */
    auto match = rxlen.match(cppfileName);
    if (match.hasMatch())
    {
        //时间 [文件名:行号] 日志内容
        qstrText = QString(msgType + "%1 [%2:%3] %4\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(match.captured(1)).arg(context.line).arg(msg);
    }
    else
    {
        //时间 日志内容
        qstrText = QString(msgType + "%1 %2\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(msg);
    }

    QMutexLocker lock(&fileLock);

    s_logString += qstrText;
    s_logCount++;
    if (s_logCount >= MAX_LOG_COUNT) {
        QString logFilePath = QCoreApplication::applicationDirPath() + "/log";
        QString logFileDayPath = logFilePath + "/" + QDateTime::currentDateTime().toString("yyyy-MM-dd");
        QString logFileName = logFileDayPath + "/" + QDateTime::currentDateTime().toString("yyyyMMddhh") + ".log";

        QDir logQDir(logFilePath);
        if (!logQDir.exists())
        {
            logQDir.mkpath(logFilePath);
        }

        QDir logDayQDir(logFileDayPath);
        if (!logDayQDir.exists())
        {
            logDayQDir.mkpath(logFileDayPath);
        }

        QFile logQFile(logFileName);

        //以年月日小时为名称，记录日志，文件最大不超过指定的SIZE
        if (logQFile.size() < MAX_LOG_FILE_SIZE)
        {
            if (logQFile.open(QIODevice::WriteOnly | QIODevice::Append))
            {
                logQFile.write(s_logString.toStdString().c_str());
                logQFile.close();
            }
        }
        s_logString.clear();
        s_logCount = 0;
    }
}

void AuxTask::saveImage(const QImage &image, const QString &saveFilePath, const QString &saveFileName, const QString &saveFileFormat)
{
    if (s_saveImageCount < MAX_IMG_SAVE_QUEUE_SIZE) {
        s_saveImageCount++;
        QMetaObject::invokeMethod(this, "saveImageAsync",
                                  Q_ARG(const QImage &, image),
                                  Q_ARG(const QString &, saveFilePath),
                                  Q_ARG(const QString &, saveFileName),
                                  Q_ARG(const QString &, saveFileFormat));
    } else {
        qInfo() << "AuxTask: image save queue is full";
    }
}

void AuxTask::saveImageAsync(const QImage &image, const QString &saveFilePath, const QString &saveFileName, const QString &saveFileFormat)
{
    s_saveImageCount--;

    QDir saveQDir(saveFilePath);

    if (!saveQDir.exists())
    {
        saveQDir.mkpath(saveFilePath);
    }

    image.save(saveFilePath + saveFileName + "." + saveFileFormat);
}


void AuxTask::saveConfigFile(const QString &jsonFileName, const QJsonObject &jsonFileObj)
{
    QMetaObject::invokeMethod(this, "saveConfigFileAsync", Q_ARG(const QString &, jsonFileName), Q_ARG(const QJsonObject &, jsonFileObj));
}

void AuxTask::saveConfigFileAsync(const QString &jsonFileName, const QJsonObject &jsonFileObj)
{
    static QMutex fileLock;

    QMutexLocker lock(&fileLock);
    QByteArray json = QJsonDocument(jsonFileObj).toJson();
    QString temp = jsonFileName + ".tmp";
    QFile file(temp);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(json);
        file.flush();
        file.close();
    }
    else
    {
        qInfo().noquote() << QString("AuxTask: failed to write temp file %1").arg(temp);

        return;
    }
    // 备份当前配置
    QString backup = jsonFileName + ".back";
    if (QFile::exists(backup) && !QFile::remove(backup))
    {
        qInfo().noquote() << QString("AuxTask: failed to remove backup file %1").arg(backup);

        return;
    }
    if (!QFile::rename(jsonFileName, backup))
    {
        qInfo().noquote() << QString("AuxTask: failed to create backup file %1").arg(backup);

        return;
    }

    // 临时文件转成正式文件
    if (!file.rename(jsonFileName))
    {
        qInfo().noquote() << QString("AuxTask: failed to remove temp file %1").arg(temp);
        return;
    }

    // 同步文件系统
    QProcess::execute("sync");
}

void AuxTask::checkConfigFile(const QString &jsonFileName)
{
    QString backup = jsonFileName + ".back";
    QFile file(jsonFileName);

    if (QFile::exists(jsonFileName) && QFile::exists(backup) && (file.size() == 0))
    {
        qInfo().noquote() << QString("config file is empty! generate it with the back file");
        QFile::remove(jsonFileName);
        QFile::copy(backup, jsonFileName);
    }
}

void AuxTask::cleanExpiredLogFiles(const int logFileKeepDays)
{
    QMetaObject::invokeMethod(this, "cleanExpiredLogFilesAsync", Q_ARG(const int, logFileKeepDays));
}

void AuxTask::cleanExpiredLogFilesAsync(const int logFileKeepDays)
{
    QDir logfileDir(QCoreApplication::applicationDirPath() + "/log");
    QString expireFileName = QDateTime::currentDateTime().addDays(-logFileKeepDays).toString("yyyy-MM-dd");

    logfileDir.setNameFilters(QStringList() << "*-*-*");
    QDirIterator logfileDirIterator(logfileDir);
    while (logfileDirIterator.hasNext()) //遍历各日志文件
    {
        logfileDirIterator.next();
        if (QString::compare(expireFileName, logfileDirIterator.fileName()) > 0 )
        {
            QDir path(logfileDirIterator.filePath());
            qInfo() << "Clean expired log file: " << logfileDirIterator.filePath();
            path.removeRecursively();
        }
    }
}

void AuxTask::cleanExpiredImageDirectory(const QString &imageRootPath, const int imageFileKeepDays)
{
    QMetaObject::invokeMethod(this, "cleanExpiredImageDirectoryAsync", Q_ARG(const QString &, imageRootPath),  Q_ARG(const int, imageFileKeepDays));
}

void AuxTask::cleanExpiredImageDirectoryAsync(const QString &imageRootPath, const int imageFileKeepDays)
{
    QString expireDate = QDateTime::currentDateTime().addDays(-imageFileKeepDays).toString("yyyyMMdd");

    //image路径信息：imageRootPath/相机id/AllImg/yyyyMMdd/hh/相机id#xxxxxx.jpg
    QDir imageRootDir(imageRootPath);
    imageRootDir.setFilter(QDir::Dirs | QDir::NoDot | QDir::NoDotDot);
    QDirIterator cameraIdPathIterator(imageRootDir);
    while (cameraIdPathIterator.hasNext()) //遍历各相机
    {
        cameraIdPathIterator.next();

        QString typePath = cameraIdPathIterator.filePath() + "/AllImg";
        QDir typePathDir(typePath);
        typePathDir.setFilter(QDir::Dirs | QDir::NoDot | QDir::NoDotDot);

        QDirIterator dateIterator(typePathDir);
        while (dateIterator.hasNext()) //遍历各日期
        {
            dateIterator.next();
            if (QString::compare(expireDate, dateIterator.fileName()) > 0)
            {
                QDir dateDir(dateIterator.filePath());

                qInfo() << "Clean expired image directory: " << dateIterator.filePath();
                dateDir.removeRecursively();
            }
        }
    }
}

void AuxTask::cleanImageDirectoryForNoSpace(const QString &imageRootPath)
{
    if (s_cleanImageCount < MAX_IMG_CLEAN_QUEUE_SIZE) {
        s_cleanImageCount++;
        QMetaObject::invokeMethod(this, "cleanImageDirectoryForNoSpaceAsync", Q_ARG(const QString &, imageRootPath));
    }
}

void AuxTask::cleanImageDirectoryForNoSpaceAsync(const QString &imageRootPath)
{
    s_cleanImageCount--;

    QStorageInfo storage(imageRootPath);
    storage.refresh(); //获得最新磁盘信息

    qint64 freeSpaceSize = storage.bytesAvailable() / 1000 / 1000;

    if (freeSpaceSize >= MIN_FREE_SPACE)
    {
        return;
    }

    QDir imageRootDir(imageRootPath);
    imageRootDir.setFilter(QDir::Dirs | QDir::NoDot | QDir::NoDotDot);

    //所有相机，删除最早的一个日期目录；
    {
        QDirIterator cameraIdPathIterator(imageRootDir);
        bool bCleanDateOccured = false;
        while (cameraIdPathIterator.hasNext()) //遍历各相机
        {
            cameraIdPathIterator.next();
            QString typePath = cameraIdPathIterator.filePath() + "/AllImg";
            QDir typePathDir(typePath);
            QFileInfoList dateInfolist = typePathDir.entryInfoList(QDir::Dirs | QDir::NoDot | QDir::NoDotDot, QDir::Name);
            if (dateInfolist.size() > 1) //如果相机中存在多个日期目录，删除最老的一个目录
            {
                QFileInfo dateInfo = dateInfolist[0];
                QDir dateDir(dateInfo.filePath());
                qDebug() << "No Space, clean oldest date directory: " << dateInfo.filePath();
                dateDir.removeRecursively();

                bCleanDateOccured = true;
            }
        }

        if (bCleanDateOccured)
        {
            return;
        }
    }

    //所有相机日期目录只有一个，删除最早的一个小时目录
    {
        QDirIterator cameraIdPathIterator(imageRootDir);

        while (cameraIdPathIterator.hasNext()) //遍历各相机
        {
            cameraIdPathIterator.next();
            QString typePath;
            typePath = cameraIdPathIterator.filePath() + "/AllImg";
            QDir typePathDir(typePath);
            typePathDir.setFilter(QDir::Dirs | QDir::NoDot | QDir::NoDotDot);
            QDirIterator dateIterator(typePathDir);
            while (dateIterator.hasNext()) //遍历各日期
            {
                dateIterator.next();
                QDir hourDir(dateIterator.filePath());
                QFileInfoList hourInfolist = hourDir.entryInfoList(QDir::Dirs | QDir::NoDot | QDir::NoDotDot, QDir::Name);
                if (hourInfolist.size() > 1) //如果相机中存在多个小时目录，删除最老的一个目录
                {
                    QFileInfo hourInfo = hourInfolist[0];
                    QDir hourDir(hourInfo.filePath());
                    qInfo() << "No Space, clean oldest hour directory: " << hourInfo.filePath();
                    hourDir.removeRecursively();
                }
            }
        }
    }
}

cv::Mat AuxTask::QImageToCvMat(const QImage &qImageInput)
{
    switch (qImageInput.format())
    {
        case QImage::Format_Grayscale8:
            return cv::Mat( cv::Size(qImageInput.width(), qImageInput.height()),
                            CV_8UC1,
                            (void*)qImageInput.constBits()).clone();

        case QImage::Format_RGB32:
            return cv::Mat( cv::Size(qImageInput.width(), qImageInput.height()),
                            CV_8UC4,
                            (void*)qImageInput.constBits()).clone();

        case QImage::Format_BGR888:
        default:
            return cv::Mat( cv::Size(qImageInput.width(), qImageInput.height()),
                            CV_8UC3,
                            (void*)qImageInput.constBits()).clone();
    }
}
