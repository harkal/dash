#ifndef COMMON_H
#define COMMON_H

#include <map>
#include <unordered_map>
#include <vector>
#include <set>
#include <unordered_set>
#include <functional>
#include <string>

#include <boost/multiprecision/cpp_int.hpp>

using Byte = unsigned char;
using Bytes = std::vector<Byte>;
extern Bytes const NullBytes;

// Primitive data types
using bigint = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<>>;
using U64 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<64, 64, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using U128 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<128, 128, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using U256 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using S256 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
using U160 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<160, 160, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using S160 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<160, 160, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
using U512 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<512, 512, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using S512 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<512, 512, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
using U256s = std::vector<U256>;
using U160s = std::vector<U160>;
using U256Set = std::set<U256>;
using U160Set = std::set<U160>;

// Maps
using StringMap = std::map<std::string, std::string>;

// Hashmaps
using StringHashMap = std::unordered_map<std::string, std::string>;

// Strings
using Strings = std::vector<std::string>;
extern std::string const EmptyString;

template <class T, class Out>
inline void toBigEndian(T a, Out& b)
{
    for (auto i = b.size() ; i != 0; a >>= 8, --i) {
        T v = a & (T)0xff;
        b[i-1] = (typename Out::value_type)(uint8_t)v;
    }
}

template <class Out, class T>
inline Out fromBigEndian(Out const& bytes)
{
    Out ret = (Out)0;

    for (auto i: bytes) {
        ret = (Out)( (ret << 8) | (Byte)(typename std::make_unsigned<decltype(i)>::type)i );
    }

    return ret;
}

inline Bytes toBigEndian(U160 val) { Bytes ret(20); toBigEndian(val, ret); return ret; }
inline Bytes toBigEndian(U256 val) { Bytes ret(32); toBigEndian(val, ret); return ret; }

#endif // COMMON_H
