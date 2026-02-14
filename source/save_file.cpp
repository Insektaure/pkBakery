#include "save_file.h"
#include <fstream>
#include <cstring>
#include <cstdio>

void SaveFile::setGameType(GameType game) {
    gameType_ = game;
}

bool SaveFile::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    auto fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    file.close();

    // Keep original for round-trip verification
    originalFileData_ = fileData;

    blocks_ = SwishCrypto::decrypt(fileData.data(), fileData.size());

    SCBlock* donutBlock = SwishCrypto::findBlock(blocks_, KDONUTS);
    if (donutBlock) {
        donutData_ = donutBlock->data.data();
        donutDataLen_ = donutBlock->data.size();
    }

    loaded_ = true;
    return true;
}

bool SaveFile::save(const std::string& path) {
    if (!loaded_)
        return false;

    std::vector<uint8_t> encrypted = SwishCrypto::encrypt(blocks_);

    // Open for in-place writing (r+b) to avoid truncating the file.
    // The Switch save filesystem journal can break if we truncate + rewrite.
    // Our encrypted output is always the exact same size as the original.
    FILE* f = std::fopen(path.c_str(), "r+b");
    if (!f) {
        // File doesn't exist yet - create it
        f = std::fopen(path.c_str(), "wb");
    }
    if (!f)
        return false;

    size_t written = std::fwrite(encrypted.data(), 1, encrypted.size(), f);
    std::fclose(f);
    return written == encrypted.size();
}

std::string SaveFile::verifyRoundTrip() {
    if (originalFileData_.empty())
        return "No original data to verify";

    std::vector<uint8_t> reEncrypted = SwishCrypto::encrypt(blocks_);

    if (reEncrypted.size() != originalFileData_.size()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Size mismatch: original %zu, re-encrypted %zu",
                      originalFileData_.size(), reEncrypted.size());
        originalFileData_.clear();
        return buf;
    }

    for (size_t i = 0; i < reEncrypted.size(); i++) {
        if (reEncrypted[i] != originalFileData_[i]) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "Mismatch at byte %zu: original 0x%02X, got 0x%02X",
                          i, originalFileData_[i], reEncrypted[i]);
            originalFileData_.clear();
            return buf;
        }
    }

    originalFileData_.clear();
    return "OK";
}

Donut9a SaveFile::getDonut(int index) {
    Donut9a d{};
    if (!donutData_ || index < 0 || index >= Donut9a::MAX_COUNT)
        d.data = nullptr;
    else
        d.data = donutData_ + index * Donut9a::SIZE;
    return d;
}

int SaveFile::donutCount() const {
    if (!donutData_)
        return 0;
    int count = 0;
    for (int i = 0; i < Donut9a::MAX_COUNT; i++) {
        uint64_t ts;
        std::memcpy(&ts, donutData_ + i * Donut9a::SIZE, 8);
        if (ts != 0)
            count++;
    }
    return count;
}
