#ifndef AES256CBC_H
#define AES256CBC_H

#include <QtGlobal>
#include <openssl/crypto.h>

class QByteArray;

class Aes256CBC {
public:
    Aes256CBC();
    ~Aes256CBC();

public:
    QByteArray decode(const QByteArray &data, const QByteArray &key, const QByteArray &iv) const;
    QByteArray encode(const QByteArray &data, const QByteArray &key, const QByteArray &iv) const;

private:
    Q_DISABLE_COPY(Aes256CBC)

    EVP_CIPHER_CTX *m_ctx;
};

#endif // AES256CBC_H
