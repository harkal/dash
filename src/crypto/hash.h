// Copyright (c) 2017 Harry Kalogirou (harkal@gmail.com)
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef HASH_H
#define HASH_H

#include <array>

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/functional/hash.hpp>

#include <../common.h>
#include "../uint256.h"

template <unsigned N>
class CHash
{
public:
    // The arithmetic type
    using Arith = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<N * 8, N * 8, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;

    static const unsigned SIZE = N;

    CHash() { SetNull(); }

    CHash(Arith const &u) { toBigEndian(u, mData); }

    CHash(const base_blob<N * 8> &blob) {
        memcpy(mData.data(), blob.begin(), N);
    }

    CHash(uint64_t v) {
        SetNull();
        mData[0] = (uint32_t)v;
        mData[1] = (uint32_t)(v >> 32);
    }

    explicit CHash(const Byte *data) { memcpy(mData.data(), data, N); }
    explicit CHash(const std::vector<Byte>& data) { mData(data.begin(), data.end()); }

    operator Arith() const { return fromBigEndian<Arith>( mData ); }

    bool operator == (CHash const &o) const { return mData == o.mData; }
    bool operator != (CHash const &o) const { return mData != o.mData; }

    friend inline bool operator<(const CHash& a, const CHash& b) { return memcmp(a.mData.data(), b.mData.data(), N) < 0; }
    friend inline bool operator<=(const CHash& a, const CHash& b) { return memcmp(a.mData.data(), b.mData.data(), N) <= 0; }
    friend inline bool operator>(const CHash& a, const CHash& b) { return memcmp(a.mData.data(), b.mData.data(), N) > 0; }
    friend inline bool operator>=(const CHash& a, const CHash& b) { return memcmp(a.mData.data(), b.mData.data(), N) >= 0; }
    friend inline const CHash operator|(const CHash& a, const CHash& b) { return CHash(a) |= b; }

    friend inline const CHash operator+(const CHash& a, const CHash& b) { return CHash(a) += b; }
    friend inline const CHash operator-(const CHash& a, const CHash& b) { return CHash(a) -= b; }
    friend inline const CHash operator*(const CHash& a, const CHash& b) { return CHash(a) *= b; }
    friend inline const CHash operator/(const CHash& a, const CHash& b) { return CHash(a) /= b; }

    CHash& operator*=(uint32_t b32) { *this = ((Arith)*this) * b32; return *this; }

    CHash& operator*=(const CHash& b) { *this = ((Arith)*this) * (Arith)b; return *this; }
    CHash& operator/=(const CHash& b) { *this = ((Arith)*this) / (Arith)b; return *this; }
    CHash& operator+=(const CHash& b) { *this = ((Arith)*this) + (Arith)b; return *this; }
    CHash& operator-=(const CHash& b) { *this = ((Arith)*this) - (Arith)b; return *this; }

    const CHash& operator~() const { return ~((Arith)*this); }

    CHash& operator|=(const CHash& b)
    {
        for (int i = 0; i < N; ++i)
            mData[i] |= b.mData[i];

        return *this;
    }

    void Randomize(boost::random_device)
    {
        static boost::random_device rdev;

        for (auto &i : mData)
            i = (Byte)boost::random::uniform_int_distribution<uint16_t>(0, 255)(rdev);
    }

    static CHash Random() { CHash ret; ret.Randomize(); return ret; }

    Bytes AsBytes() const { return Bytes(mData.begin(), mData.end()); }

    void SetNull() { mData.fill(0); }
    bool IsNull() const {
        for (unsigned int i = 0 ; i < N ; i++) if (mData[i] != 0)
            return false;
        return true;
    }

    /** A cheap hash function that just returns 64 bits from the result, it can be
     * used when the contents are considered uniformly random. It is not appropriate
     * when the value can easily be influenced from outside as e.g. a network adversary could
     * provide values to trigger worst-case behavior.
     */
    uint64_t GetCheapHash() const
    {
        return ReadLE64(mData.data());
    }

    uint64_t GetHash(const CHash<N>& salt) const;

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);

    std::string ToString() const { return GetHex(); }

    typename std::array<Byte, N>::iterator begin() { return mData.begin(); }
    typename std::array<Byte, N>::iterator end() { return mData.end(); }
    const typename std::array<Byte, N>::const_iterator begin() const { return mData.begin(); }
    const typename std::array<Byte, N>::const_iterator end() const { return mData.end(); }

    unsigned int size() const { return N; }

    uint32_t GetCompact(bool fNegative = false) const;
    CHash& SetCompact(uint32_t nCompact, bool *pfNegative = NULL, bool* pfOverflow = NULL);

    uint32_t Bits() const;

    uint64_t GetLow64() const
    {
        assert(N >= 2);
        return mData[0] | (uint64_t)mData[1] << 32;
    }

    // Serializaton support
    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return N;
    }

    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        s.write((char *)mData.data(), N);
    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        s.read((char *)mData.data(), N);
    }
private:
    std::array<Byte, N> mData;
};

// Define hash types
using H2048 = CHash<256>;
using H1024 = CHash<128>;
using H520  = CHash<65>;
using H512  = CHash<64>;
using H256  = CHash<32>;
using H160  = CHash<20>;
using H128  = CHash<16>;
using H64   = CHash<8>;

using Address = H160;

#endif // HASH_H
