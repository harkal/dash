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

    CHash() { SetNull(); }

    CHash(Arith const &u) { toBigEndian(u, mData); }

    CHash(const base_blob<N * 8> &blob) {
        memcpy(mData.data(), blob.begin(), N);
    }

    explicit CHash(const Byte *data) { memcpy(mData.data(), data, N); }

    operator Arith () const { return fromBigEndian<Arith>( mData ); }

    bool operator == (CHash const &o) const { return mData == o.mData; }
    bool operator != (CHash const &o) const { return mData != o.mData; }

    friend inline bool operator<(const CHash& a, const CHash& b) { return memcmp(a.mData.data(), b.mData.data(), N) < 0; }

    void Randomize(boost::random_device)
    {
        static boost::random_device rdev;

        for (auto &i : mData)
            i = (Byte)boost::random::uniform_int_distribution<uint16_t>(0, 255)(rdev);
    }

    static CHash Random() { CHash ret; ret.Randomize(); return ret; }

    Bytes AsBytes() const { return Bytes(mData(), mData() + N); }

    void SetNull() { mData.fill(0); }
    bool IsNull() const {
        for (int i = 0 ; i < N ; i++) if (mData[i] != 0)
            return false;
        return true;
    }

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);

    std::string ToString() const { return GetHex(); };

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
