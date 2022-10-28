#include "LicenseManager.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#ifndef Q_OS_WIN
#include "Aes256CBC.h"
#endif

namespace {
const char *s_key_dev   = "SFJL@!A~SKDE#I234";
const char *s_key_auth  = "23ASDNZXCLIW3FL#@";
const QByteArray s_iv   = QCryptographicHash::hash("", QCryptographicHash::Md5);
}

bool LicenseManager::verify(const QString &dir)
{
#ifdef Q_OS_WIN
    Q_UNUSED(dir)
    return true;
#else
    QByteArray info = deviceInfo();
    if (!info.isEmpty()) {
        QByteArray key = QCryptographicHash::hash(s_key_auth, QCryptographicHash::Sha3_256);
        QDirIterator it(dir, {"*.dat"}, QDir::Files | QDir::NoSymLinks);
        Aes256CBC aes;
        while (it.hasNext()) {
            if (aes.decode(readFile(it.next()), key, s_iv) == info) {
                return true;
            }
        }
        createDeviceFile(dir, info);
    }
    return false;
#endif
}

QByteArray LicenseManager::deviceInfo()
{
#ifdef Q_OS_WIN
    return QByteArray();
#else
    QProcess process;
    QByteArray info;

    process.start("sh", QStringList() << "-c" << "sudo fdisk -l | grep 'Disk identifier' | awk {'print $3'}");
    process.waitForFinished();
    info.append(process.readAllStandardOutput());

    process.start("sh", QStringList() << "-c" << "sudo dmidecode -t 4 | grep ID | sort -u | awk -F ': ' '{print $2}'");
    process.waitForFinished();
    info.append(process.readAllStandardOutput());

    return info;
#endif
}

QByteArray LicenseManager::readFile(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        return file.readAll();
    }
    return QByteArray();
}

void LicenseManager::createDeviceFile(const QString &dir, const QByteArray &info)
{
#ifdef Q_OS_WIN
    Q_UNUSED(dir)
    Q_UNUSED(info)
#else
    QDir outDir(dir);
    if (outDir.mkpath(dir)) {
        QByteArray key = QCryptographicHash::hash(s_key_dev, QCryptographicHash::Sha3_256);
        Aes256CBC aes;
        QByteArray data = aes.encode(info, key, s_iv);
        QString name = QString("%1.sdat").arg(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex().data());
        QFile file(outDir.filePath(name));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            qDebug() << "LicenseManager: create device info file" << file.fileName();
        }
    }
#endif
}
