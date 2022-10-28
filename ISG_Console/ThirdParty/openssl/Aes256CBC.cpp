#include "Aes256CBC.h"
#include <QByteArray>
#include <openssl/evp.h>

Aes256CBC::Aes256CBC()
{
    m_ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(m_ctx, EVP_aes_256_cbc(), nullptr, nullptr, nullptr);
}

Aes256CBC::~Aes256CBC()
{
    EVP_CIPHER_CTX_free(m_ctx);
}

QByteArray Aes256CBC::decode(const QByteArray &data, const QByteArray &key, const QByteArray &iv) const
{
    Q_ASSERT(key.size() == EVP_CIPHER_CTX_key_length(m_ctx));
    Q_ASSERT(iv.size() == EVP_CIPHER_CTX_iv_length(m_ctx));
    if (!EVP_DecryptInit_ex(m_ctx, nullptr, nullptr,
                            reinterpret_cast<const unsigned char *>(key.data()),
                            reinterpret_cast<const unsigned char *>(iv.data()))) {
        return QByteArray();
    }
    int bytes;
    QByteArray out;
    out.reserve(data.size() + EVP_CIPHER_CTX_block_size(m_ctx));
    if (!EVP_DecryptUpdate(m_ctx, reinterpret_cast<unsigned char *>(out.data()), &bytes,
                           reinterpret_cast<const unsigned char *>(data.data()), data.size())) {
        return QByteArray();
    }
    out.resize(bytes);
    if (!EVP_DecryptFinal_ex(m_ctx, reinterpret_cast<unsigned char *>(out.data()) + bytes, &bytes)) {
        return QByteArray();
    }
    out.resize(out.size() + bytes);
    return out;
}

QByteArray Aes256CBC::encode(const QByteArray &data, const QByteArray &key, const QByteArray &iv) const
{
    Q_ASSERT(key.size() == EVP_CIPHER_CTX_key_length(m_ctx));
    Q_ASSERT(iv.size() == EVP_CIPHER_CTX_iv_length(m_ctx));
    if (!EVP_EncryptInit_ex(m_ctx, nullptr, nullptr,
                            reinterpret_cast<const unsigned char *>(key.data()),
                            reinterpret_cast<const unsigned char *>(iv.data()))) {
        return QByteArray();
    }
    int bytes;
    QByteArray out;
    out.reserve(data.size() + EVP_CIPHER_CTX_block_size(m_ctx));
    if (!EVP_EncryptUpdate(m_ctx, reinterpret_cast<unsigned char *>(out.data()), &bytes,
                           reinterpret_cast<const unsigned char *>(data.data()), data.size())) {
        return QByteArray();
    }
    out.resize(bytes);
    if (!EVP_EncryptFinal_ex(m_ctx, reinterpret_cast<unsigned char *>(out.data()) + bytes, &bytes)) {
        return QByteArray();
    }
    out.resize(out.size() + bytes);
    return out;
}
