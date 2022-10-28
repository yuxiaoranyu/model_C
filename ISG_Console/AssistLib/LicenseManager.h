#ifndef LICENSEMANAGER_H
#define LICENSEMANAGER_H

#include <QByteArray>

class LicenseManager {
public:
    static bool verify(const QString &dir);

private:
    static QByteArray deviceInfo();
    static QByteArray readFile(const QString &path);
    static void createDeviceFile(const QString &dir, const QByteArray &info);

};

#endif // LICENSEMANAGER_H
