#ifndef TEA_H
#define TEA_H
#include <QString>
#include <cstring> //for memcpy,memset
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
using namespace std;
typedef unsigned char byte;
typedef unsigned long ulong;
typedef unsigned int uint;

#define MAX_NAME_LEN 256

class TEA
{
public:
    TEA(const byte *key, int round = 32, bool isNetByte = false);
    TEA(const TEA &rhs);
    TEA &operator=(const TEA &rhs);
    void encrypt(const byte *in, byte *out);
    void decrypt(const byte *in, byte *out);
    static size_t hexStringToBytes(const string &str, byte *out);
    static string bytesToHexString(const byte *in, size_t size);
    static int hexCharToInt(char hex);
    static char intToHexChar(int x);
    static string wrapString(string &input);
    static inline double logbase(double base, double x)
    {
        return log(x) / log(base);
    }

private:
    void encrypt(const uint *in, uint *out);
    void decrypt(const uint *in, uint *out);
    uint ntoh(uint netlong);  //{ return _isNetByte ? ntohl(netlong) : netlong; }
    uint hton(uint hostlong); // { return _isNetByte ? htonl(hostlong) : hostlong; }
private:
    int m_round;      //iteration round to encrypt or decrypt
    bool m_isNetByte; //whether input bytes come from network
    byte m_key[16];   //encrypt or decrypt key
};

bool verifyEncryptCode(const char *szPlainCode, const QString szEncryptCode);

#endif // TEA_H
