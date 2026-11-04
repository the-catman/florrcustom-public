// Reads what Writer writes. Running off the end sets bad() and everything after reads as zero.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace net {

class Reader {
public:
    Reader(const uint8_t *b, size_t n) : b_(b), n_(n) {}

    bool bad() const { return bad_; }
    bool atEnd() const { return p_ >= n_; }
    size_t pos() const { return p_; }
    void fail() { bad_ = true; }
    void skip(size_t k) {
        align();
        p_ += k;
    }

    uint32_t u8() {
        align();
        return need(1) ? b_[p_++] : 0;
    }
    uint32_t u16() {
        align();
        if (!need(2)) return 0;
        uint32_t v = b_[p_] | b_[p_ + 1] << 8;
        p_ += 2;
        return v;
    }
    uint32_t u32() {
        align();
        if (!need(4)) return 0;
        uint32_t v;
        std::memcpy(&v, b_ + p_, 4);
        p_ += 4;
        return v;
    }
    uint32_t varint() {
        align();
        uint32_t v = 0;
        for (int shift = 0; shift < 35; shift += 7) {
            if (!need(1)) return 0;
            uint8_t c = b_[p_++];
            v |= uint32_t(c & 127) << shift;
            if (c < 128) break;
        }
        return v;
    }
    float f32() {
        uint32_t u = u32();
        float v;
        std::memcpy(&v, &u, 4);
        return v;
    }
    double f64() {
        align();
        if (!need(8)) return 0;
        double v;
        std::memcpy(&v, b_ + p_, 8);
        p_ += 8;
        return v;
    }
    std::string str() {
        align();
        size_t end = p_;
        while (end < n_ && b_[end]) end++;
        std::string s(reinterpret_cast<const char *>(b_ + p_), end - p_);
        if (end >= n_) bad_ = true;
        p_ = end < n_ ? end + 1 : n_;
        return s;
    }
    uint32_t ref() {
        uint32_t v = u32();
        return v << 14 | v >> 18;
    }
    bool bit() {
        if (!need(1)) return false;
        bool v = b_[p_] >> bitPos_ & 1;
        if (++bitPos_ == 8) {
            bitPos_ = 0;
            p_++;
        }
        return v;
    }

private:
    void align() {
        if (!bitPos_) return;
        bitPos_ = 0;
        p_++;
    }
    bool need(size_t k) {
        if (p_ + k <= n_) return true;
        bad_ = true;
        p_ = n_;
        return false;
    }

    const uint8_t *b_;
    size_t n_, p_ = 0;
    int bitPos_ = 0;
    bool bad_ = false;
};

}  // namespace net
