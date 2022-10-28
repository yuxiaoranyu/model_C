#ifndef LICENSEDEVICE_H
#define LICENSEDEVICE_H

#include <QtCore/qglobal.h>

class Q_DECL_EXPORT LicenseDevice
{
public:
    bool verify(const QString &dir, const bool bCreateDeviceInfo);

private:
    QByteArray readFile(const QString &path);
    void saveFile(const QString &path, const QByteArray &data);
    QByteArray deviceInfo();
    void createDeviceFile(const QString &dir, const QByteArray &info);
};

#endif // LICENSEDEVICE_H
