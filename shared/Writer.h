// Byte-level encoding shared by every message: little-endian numbers, LEB128 varints,
// NUL-terminated strings, entity ids rotated by 14 bits, and LSB-first bit fields.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace net {

class Writer {
public:
    std::vector<uint8_t> b;

    void u8(uint32_t v) {
        endBits();
        b.push_back(uint8_t(v));
    }
    void u16(uint32_t v) {
        endBits();
        b.push_back(uint8_t(v));
        b.push_back(uint8_t(v >> 8));
    }
    void u32(uint32_t v) {
        endBits();
        for (int i = 0; i < 4; i++) b.push_back(uint8_t(v >> (8 * i)));
    }
    void varint(uint32_t v) {
        endBits();
        while (v >= 128) {
            b.push_back(uint8_t(v | 128));
            v >>= 7;
        }
        b.push_back(uint8_t(v));
    }
    void f32(float v) {
        uint32_t u;
        std::memcpy(&u, &v, 4);
        u32(u);
    }
    void f64(double v) {
        endBits();
        uint8_t t[8];
        std::memcpy(t, &v, 8);
        b.insert(b.end(), t, t + 8);
    }
    void str(const std::string &s) {
        endBits();
        b.insert(b.end(), s.begin(), s.end());
        b.push_back(0);
    }
    void ref(uint32_t id) { u32(id >> 14 | id << 18); }
    void bit(bool v) {
        if (!bitPos_) b.push_back(0);
        if (v) b.back() |= uint8_t(1 << bitPos_);
        bitPos_ = (bitPos_ + 1) & 7;
    }
    void bytes(const uint8_t *p, size_t n) {
        endBits();
        b.insert(b.end(), p, p + n);
    }
    size_t size() const { return b.size(); }

private:
    void endBits() { bitPos_ = 0; }

    int bitPos_ = 0;
};

}  // namespace net
