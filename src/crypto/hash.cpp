#include "crypto/hash.h"

template <unsigned N>
std::string CHash<N>::getHex() const
{
    char psz[N * 2 + 1];
    for (unsigned int i = 0; i < N ; i++)
        sprintf(psz + i * 2, "%02x", data[N - i - 1]);
    return std::string(psz, psz + N * 2);
}

template <unsigned N>
void CHash<N>::SetHex(const char* psz)
{
    SetNull();

    // skip leading spaces
    while (isspace(*psz))
        psz++;

    // skip 0x
    if (psz[0] == '0' && tolower(psz[1]) == 'x')
        psz += 2;

    // hex string to uint
    const char* pbegin = psz;
    while (::HexDigit(*psz) != -1)
        psz++;
    psz--;
    unsigned char* p1 = (unsigned char*)data;
    unsigned char* pend = p1 + N;
    while (psz >= pbegin && p1 < pend) {
        *p1 = ::HexDigit(*psz--);
        if (psz >= pbegin) {
            *p1 |= ((unsigned char)::HexDigit(*psz--) << 4);
            p1++;
        }
    }
}

template <unsigned N>
void CHash<N>::SetHex(const std::string& str)
{
    SetHex(str.c_str());
}

template <unsigned N>
uint64_t CHash<N>::GetHash(const CHash<N>& salt) const
{
    uint32_t a, b, c;
    const uint32_t *pn = (const uint32_t*)mData.data();
    const uint32_t *salt_pn = (const uint32_t*)salt.mData.data();
    a = b = c = 0xdeadbeef + WIDTH;

    a += pn[0] ^ salt_pn[0];
    b += pn[1] ^ salt_pn[1];
    c += pn[2] ^ salt_pn[2];
    HashMix(a, b, c);
    a += pn[3] ^ salt_pn[3];
    b += pn[4] ^ salt_pn[4];
    c += pn[5] ^ salt_pn[5];
    HashMix(a, b, c);
    a += pn[6] ^ salt_pn[6];
    b += pn[7] ^ salt_pn[7];
    HashFinal(a, b, c);

    return ((((uint64_t)b) << 32) | c);
}
