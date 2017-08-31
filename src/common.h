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

namespace common {
    using byte = unsigned char;
    using bytes = std::vector<byte>;
    extern bytes const NullBytes;

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
    using strings = std::vector<std::string>;
    extern std::string const EmptyString;
}

#endif // COMMON_H
