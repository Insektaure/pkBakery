#pragma once
#include "sc_block.h"
#include <vector>
#include <cstdint>

namespace SwishCrypto {
    void cryptStaticXorpadBytes(uint8_t* data, size_t len);
    std::vector<SCBlock> decrypt(uint8_t* fileData, size_t fileSize);
    std::vector<uint8_t> encrypt(const std::vector<SCBlock>& blocks);
    SCBlock* findBlock(std::vector<SCBlock>& blocks, uint32_t key);
    const SCBlock* findBlock(const std::vector<SCBlock>& blocks, uint32_t key);
}
