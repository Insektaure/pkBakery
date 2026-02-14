#pragma once
#include "swish_crypto.h"
#include "donut.h"
#include "game_type.h"
#include <vector>
#include <string>

// SaveFile - manages a Pokemon save file for donut editing.
// SCBlock-based (ZA/SV/SwSh/LA) — only ZA has donut data.
class SaveFile {
public:
    void setGameType(GameType game);  // must call before load()

    bool load(const std::string& path);
    bool save(const std::string& path);

    Donut9a getDonut(int index);
    int donutCount() const;
    uint8_t* donutBlockData() { return donutData_; }

    bool isLoaded() const { return loaded_; }
    bool hasDonutBlock() const { return donutData_ != nullptr; }

    // Debug: verify encrypt(decrypt(file)) == file. Call right after load().
    std::string verifyRoundTrip();

private:
    std::vector<SCBlock> blocks_;
    uint8_t* donutData_ = nullptr;
    size_t donutDataLen_ = 0;
    bool loaded_ = false;

    GameType gameType_ = GameType::ZA;

    // Block key for donuts from SaveBlockAccessor9ZA.cs
    static constexpr uint32_t KDONUTS = 0xBE007476;

    // Original file data for round-trip verification (cleared after verify)
    std::vector<uint8_t> originalFileData_;
};
