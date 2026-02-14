#pragma once
#include <cstdint>
#include <vector>
#include <bit>

// SCXorShift32 - PRNG used to encrypt/decrypt SCBlock fields.
// Ported from PKHeX.Core/Saves/Encryption/SwishCrypto/SCXorShift32.cs
class SCXorShift32 {
public:
    explicit SCXorShift32(uint32_t seed) : state_(getInitialState(seed)) {}

    uint8_t next() {
        uint8_t result = static_cast<uint8_t>(state_ >> (counter_ << 3));
        if (counter_ == 3) {
            state_ = xorshiftAdvance(state_);
            counter_ = 0;
        } else {
            ++counter_;
        }
        return result;
    }

    int32_t next32() {
        return next() | (next() << 8) | (next() << 16) | (next() << 24);
    }

private:
    int counter_ = 0;
    uint32_t state_;

    static uint32_t getInitialState(uint32_t state) {
        int pop = std::popcount(state);
        for (int i = 0; i < pop; i++)
            state = xorshiftAdvance(state);
        return state;
    }

    static uint32_t xorshiftAdvance(uint32_t state) {
        state ^= state << 2;
        state ^= state >> 15;
        state ^= state << 13;
        return state;
    }
};

// SCTypeCode - block data types.
enum class SCTypeCode : uint8_t {
    None   = 0,
    Bool1  = 1,
    Bool2  = 2,
    Bool3  = 3,
    Object = 4,
    Array  = 5,
    Byte   = 8,
    UInt16 = 9,
    UInt32 = 10,
    UInt64 = 11,
    SByte  = 12,
    Int16  = 13,
    Int32  = 14,
    Int64  = 15,
    Single = 16,
    Double = 17,
};

inline int getTypeSize(SCTypeCode type) {
    switch (type) {
        case SCTypeCode::Bool3:  return 1;
        case SCTypeCode::Byte:   return 1;
        case SCTypeCode::SByte:  return 1;
        case SCTypeCode::UInt16: return 2;
        case SCTypeCode::Int16:  return 2;
        case SCTypeCode::UInt32: return 4;
        case SCTypeCode::Int32:  return 4;
        case SCTypeCode::Single: return 4;
        case SCTypeCode::UInt64: return 8;
        case SCTypeCode::Int64:  return 8;
        case SCTypeCode::Double: return 8;
        default: return 0;
    }
}

struct SCBlock {
    uint32_t key;
    SCTypeCode type;
    SCTypeCode subType = SCTypeCode::None;
    std::vector<uint8_t> data;

    static SCBlock readFromOffset(const uint8_t* buf, size_t bufLen, size_t& offset);
    size_t writeBlock(uint8_t* out) const;
    size_t encodedSize() const;
};
