#include "crypto/hash.h"
#include "utilstrencodings.h"

template <unsigned N>
CHash<N>& CHash<N>::SetCompact(uint32_t nCompact, bool *pfNegative, bool* pfOverflow)
{
    int nSize = nCompact >> 24;
    uint32_t nWord = nCompact & 0x007fffff;
    if (nSize <= 3) {
        nWord >>= 8 * (3 - nSize);
        *this = nWord;
    } else {
        Arith bWord = nWord;
        bWord <<= 8 * (nSize - 3);
        *this = bWord;
    }
    if (pfNegative)
        *pfNegative = nWord != 0 && (nCompact & 0x00800000) != 0;
    if (pfOverflow)
        *pfOverflow = nWord != 0 && ((nSize > 34) ||
                                     (nWord > 0xff && nSize > 33) ||
                                     (nWord > 0xffff && nSize > 32));
    return *this;
}

template <unsigned N>
uint32_t CHash<N>::GetCompact(bool fNegative) const
{
    int nSize = (Bits() + 7) / 8;
    uint32_t nCompact = 0;
    if (nSize <= 3) {
        nCompact = GetLow64() << 8 * (3 - nSize);
    } else {
        H256 bn = (H256::Arith)*this >> 8 * (nSize - 3);
        nCompact = bn.GetLow64();
    }
    // The 0x00800000 bit denotes the sign.
    // Thus, if it is already set, divide the mantissa by 256 and increase the exponent.
    if (nCompact & 0x00800000) {
        nCompact >>= 8;
        nSize++;
    }
    assert((nCompact & ~0x007fffff) == 0);
    assert(nSize < 256);
    nCompact |= nSize << 24;
    nCompact |= (fNegative && (nCompact & 0x007fffff) ? 0x00800000 : 0);
    return nCompact;
}


template <unsigned N>
std::string CHash<N>::GetHex() const
{
    char psz[N * 2 + 1];
    for (unsigned int i = 0; i < N ; i++)
        sprintf(psz + i * 2, "%02x", mData[i]);
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
    unsigned char* p1 = (unsigned char*)mData.data();
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

static void inline HashMix(uint32_t& a, uint32_t& b, uint32_t& c)
{
    // Taken from lookup3, by Bob Jenkins.
    a -= c;
    a ^= ((c << 4) | (c >> 28));
    c += b;
    b -= a;
    b ^= ((a << 6) | (a >> 26));
    a += c;
    c -= b;
    c ^= ((b << 8) | (b >> 24));
    b += a;
    a -= c;
    a ^= ((c << 16) | (c >> 16));
    c += b;
    b -= a;
    b ^= ((a << 19) | (a >> 13));
    a += c;
    c -= b;
    c ^= ((b << 4) | (b >> 28));
    b += a;
}

static void inline HashFinal(uint32_t& a, uint32_t& b, uint32_t& c)
{
    // Taken from lookup3, by Bob Jenkins.
    c ^= b;
    c -= ((b << 14) | (b >> 18));
    a ^= c;
    a -= ((c << 11) | (c >> 21));
    b ^= a;
    b -= ((a << 25) | (a >> 7));
    c ^= b;
    c -= ((b << 16) | (b >> 16));
    a ^= c;
    a -= ((c << 4) | (c >> 28));
    b ^= a;
    b -= ((a << 14) | (a >> 18));
    c ^= b;
    c -= ((b << 24) | (b >> 8));
}

template <unsigned N>
uint64_t CHash<N>::GetHash(const CHash<N>& salt) const
{
    uint32_t a, b, c;
    const uint32_t *pn = (const uint32_t*)mData.data();
    const uint32_t *salt_pn = (const uint32_t*)salt.mData.data();
    a = b = c = 0xdeadbeef + N;

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

template <unsigned N>
uint32_t CHash<N>::Bits() const
{
    for (int pos = N - 1; pos >= 0; pos--) {
        if (mData[pos]) {
            for (int bits = 31; bits > 0; bits--) {
                if (mData[pos] & 1 << bits)
                    return 32 * pos + bits + 1;
            }
            return 32 * pos + 1;
        }
    }
    return 0;
}

template CHash<32>& CHash<32>::SetCompact(uint32_t nCompact, bool *pfNegative, bool* pfOverflow);
template uint32_t CHash<32>::GetCompact(bool fNegative) const;
template uint64_t CHash<32>::GetHash(const CHash<32>& salt) const;
template uint32_t CHash<32>::Bits() const;
template std::string CHash<32>::GetHex() const;
template void CHash<32>::SetHex(const char* psz);
template void CHash<32>::SetHex(const std::string& str);

template std::string CHash<20>::GetHex() const;
