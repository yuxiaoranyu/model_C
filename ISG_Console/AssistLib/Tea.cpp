#include "Tea.h"
#include <QtEndian>

uint TEA::ntoh(uint netlong)
{
    return m_isNetByte ? qFromBigEndian(netlong) : netlong;
}

uint TEA::hton(uint hostlong)
{
    return m_isNetByte ? qToBigEndian(hostlong) : hostlong;
}

char TEA::intToHexChar(int x)
{
    static const char HEX[16] =
    {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'A', 'B',
        'C', 'D', 'E', 'F'
    };

    return HEX[x];
}

string TEA::wrapString(string &input)
{
    int n1 = 16 - (int)input.length();
    char p[256];
    int k = 0;
    for (int i = 0; i < n1; i++)
    {
        p[k++] = input.at(i);
        p[k++] = ' ';
    }
    for (int i = n1; i < 8; i++)
    {
        p[k++] = input.at(n1 + 2 * (i - n1));
        p[k++] = input.at(n1 + 2 * (i - n1) + 1);
        p[k++] = ' ';
    }
    p[k++] = '\0';

    return string(p);
}

int TEA::hexCharToInt(char hex)
{
    hex = toupper(hex);
    if (isdigit(hex))
        return (hex - '0');
    if (isalpha(hex))
        return (hex - 'A' + 10);
    return 0;
}

string TEA::bytesToHexString(const byte *in, size_t size)
{
    string str;
    for (size_t i = 0; i < size; i++)
    {
        int t = in[i];
        int a = t / 16;
        int b = t % 16;
        str.append(1, intToHexChar(a));
        str.append(1, intToHexChar(b));
        if (i != size - 1)
            str.append(1, ' ');
    }
    return str;
}

size_t TEA::hexStringToBytes(const string &str, byte *out)
{
    vector<string> vec;
    string::size_type currPos = 0, prevPos = 0;
    while ((currPos = str.find(' ', prevPos)) != string::npos)
    {
        string b(str.substr(prevPos, currPos - prevPos));
        vec.push_back(b);
        prevPos = currPos + 1;
    }

    if (prevPos < str.size())
    {
        string b(str.substr(prevPos));
        vec.push_back(b);
    }

    typedef vector<string>::size_type sz_type;
    sz_type size = vec.size();
    for (sz_type i = 0; i < size; i++)
    {
        int a = hexCharToInt(vec[i][0]);
        int b = hexCharToInt(vec[i][1]);
        out[i] = a * 16 + b;
    }

    return size;
}

TEA::TEA(const byte *key, int round /*= 32*/, bool isNetByte /*= false*/)
    : m_round(round),
      m_isNetByte(isNetByte)
{
    if (key != 0)
        memcpy(m_key, key, 16);
    else
        memset(m_key, 0, 16);
}

TEA::TEA(const TEA &rhs)
    : m_round(rhs.m_round), m_isNetByte(rhs.m_isNetByte)
{
    memcpy(m_key, rhs.m_key, 16);
}

TEA &TEA::operator=(const TEA &rhs)
{
    if (&rhs != this)
    {
        m_round = rhs.m_round;
        m_isNetByte = rhs.m_isNetByte;
        memcpy(m_key, rhs.m_key, 16);
    }

    return *this;
}

void TEA::encrypt(const byte *in, byte *out)
{
    encrypt((const uint *)in, (uint *)out);
}

void TEA::decrypt(const byte *in, byte *out)
{
    decrypt((const uint *)in, (uint *)out);
}

void TEA::encrypt(const uint *in, uint *out)
{
    uint *k = (uint *)m_key;
    register uint y = ntoh(in[0]);
    register uint z = ntoh(in[1]);
    register uint a = ntoh(k[0]);
    register uint b = ntoh(k[1]);
    register uint c = ntoh(k[2]);
    register uint d = ntoh(k[3]);
    register uint delta = 0x9E3779B9; /* (sqrt(5)-1)/2*2^32 */
    register int round = m_round;
    register uint sum = 0;

    while (round--)
    { /* basic cycle start */
        sum += delta;
        y += ((z << 4) + a) ^ (z + sum) ^ ((z >> 5) + b);
        z += ((y << 4) + c) ^ (y + sum) ^ ((y >> 5) + d);
    } /* end cycle */
    out[0] = ntoh(y);
    out[1] = ntoh(z);
}

void TEA::decrypt(const uint *in, uint *out)
{
    uint *k = (uint *)m_key;
    register uint y = ntoh(in[0]);
    register uint z = ntoh(in[1]);
    register uint a = ntoh(k[0]);
    register uint b = ntoh(k[1]);
    register uint c = ntoh(k[2]);
    register uint d = ntoh(k[3]);
    register uint delta = 0x9E3779B9; /* (sqrt(5)-1)/2*2^32 */
    register int round = m_round;
    register uint sum = 0;

    if (round == 32)
        sum = 0xC6EF3720; /* delta << 5*/
    else if (round == 16)
        sum = 0xE3779B90; /* delta << 4*/
    else
        sum = delta << static_cast<int>(logbase(2, round));

    while (round--) /* basic cycle start */
    {
        z -= ((y << 4) + c) ^ (y + sum) ^ ((y >> 5) + d);
        y -= ((z << 4) + a) ^ (z + sum) ^ ((z >> 5) + b);
        sum -= delta;
    } /* end cycle */
    out[0] = ntoh(y);
    out[1] = ntoh(z);
}

//清除字符串里的空格符号，返回目的字符串的长度，如果目的buffer太小，则返回-1
int delStringSpace(char *DestBuffer, int iBufferSize, const char *SrcStr)
{
    int iDest = 0;
    int iSrc = 0;

    while (SrcStr[iSrc] != 0)
    {
        if (SrcStr[iSrc] != ' ')
        {
            DestBuffer[iDest] = SrcStr[iSrc];
            iDest++;
            if (iDest >= iBufferSize)
                return -1;
        }

        iSrc++;
    }

    DestBuffer[iDest] = 0;
    return iDest;
}

bool verifyEncryptCode(const char *szPlainCode, const QString szEncryptCode)
{
    std::string temp_szEncryptCode = szEncryptCode.toStdString();
    string strPlainCode = szPlainCode;
    char szPureEncryptCode[MAX_NAME_LEN]; //删除空格后的
    delStringSpace(szPureEncryptCode, MAX_NAME_LEN, temp_szEncryptCode.c_str());

    const string keyStr("BA DA 7D 21 DB E2 DB B3 11 B4 55 01 A5 C6 EA DF");
    const int SIZE_IN = 256, SIZE_OUT = 8, SIZE_KEY = 16;
    byte plain[SIZE_IN], crypt[SIZE_OUT], key[SIZE_KEY];

    if (strlen(szPureEncryptCode) != SIZE_OUT * 2)
    {
        return false;
    }

    strPlainCode = TEA::wrapString(strPlainCode);

    TEA::hexStringToBytes(strPlainCode, plain);
    TEA::hexStringToBytes(keyStr, key);

    TEA tea(key, SIZE_KEY, true);
    tea.encrypt(plain, crypt);

    char q[3 * SIZE_OUT + 1];
    for (int i = 0; i < SIZE_OUT; i++)
    {
        q[3 * i] = szPureEncryptCode[2 * i];
        q[3 * i + 1] = szPureEncryptCode[2 * i + 1];
        q[3 * i + 2] = ' ';
    }
    q[3 * SIZE_OUT] = '\0';

    byte enterCodeBytes[SIZE_OUT];
    TEA::hexStringToBytes(string(q), enterCodeBytes);

    if (memcmp(crypt, enterCodeBytes, SIZE_OUT) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}
