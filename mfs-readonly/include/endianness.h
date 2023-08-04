#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ || \
    defined(__BIG_ENDIAN__)
template <typename T> T swap_be(T x) {
    return x;
}
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ || \
    defined(__LITTLE_ENDIAN__)
template <typename T> T swap_be(T x) {
    union s {
        T x;
        uint8_t p[sizeof(T)];
    } in, out;
    in.x = x;

    for (size_t i = 0; i < sizeof(T); i++) {
        out.p[i] = in.p[(sizeof(T) - i) - 1];
    }

    return out.x;
}
#else
// we were not able to figure out the endianness of this machine at compile time so we test it at runtime
template <typename T> T swap_be(T x) {
    uint32_t test = 0x11223344;
    if (*((uint8_t *)&test) == 0x11) {
        return x;
    }

    union s {
        T x;
        uint8_t p[sizeof(T)];
    } in, out;
    in.x = x;

    for (size_t i = 0; i < sizeof(T); i++) {
        out.p[i] = in.p[(sizeof(T) - i) - 1];
    }

    return out.x;
}
#endif
