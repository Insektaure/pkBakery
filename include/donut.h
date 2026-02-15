#pragma once
#include <cstdint>
#include <cstring>
#include <string>

// Donut9a - accessor for a single donut entry in the save file.
// Ported from PKHeX.Core DonutPocket9a.cs / Donut9a struct.
// Each donut is 0x48 (72) bytes stored contiguously in the SCBlock.
struct Donut9a {
    static constexpr int SIZE = 0x48;
    static constexpr int MAX_COUNT = 999;
    static constexpr int MAX_BERRIES = 8;
    static constexpr int MAX_FLAVORS = 3;

    uint8_t* data; // points into SCBlock data

    // --- Read accessors (little-endian) ---
    uint64_t millisecondsSince1970() const {
        uint64_t v; std::memcpy(&v, data + 0x00, 8); return v;
    }
    uint8_t stars() const { return data[0x08]; }
    uint8_t levelBoost() const { return data[0x09]; }
    uint16_t donutSprite() const {
        uint16_t v; std::memcpy(&v, data + 0x0A, 2); return v;
    }
    uint16_t calories() const {
        uint16_t v; std::memcpy(&v, data + 0x0C, 2); return v;
    }
    uint16_t berryName() const {
        uint16_t v; std::memcpy(&v, data + 0x0E, 2); return v;
    }
    uint16_t berry(int i) const {
        uint16_t v; std::memcpy(&v, data + 0x10 + i * 2, 2); return v;
    }
    uint64_t flavor(int i) const {
        uint64_t v; std::memcpy(&v, data + 0x28 + i * 8, 8); return v;
    }

    // --- Write accessors ---
    void setMillisecondsSince1970(uint64_t v) { std::memcpy(data + 0x00, &v, 8); }
    void setStars(uint8_t v) { data[0x08] = v; }
    void setLevelBoost(uint8_t v) { data[0x09] = v; }
    void setDonutSprite(uint16_t v) { std::memcpy(data + 0x0A, &v, 2); }
    void setCalories(uint16_t v) { std::memcpy(data + 0x0C, &v, 2); }
    void setBerryName(uint16_t v) { std::memcpy(data + 0x0E, &v, 2); }
    void setBerry(int i, uint16_t v) { std::memcpy(data + 0x10 + i * 2, &v, 2); }
    void setFlavor(int i, uint64_t v) { std::memcpy(data + 0x28 + i * 8, &v, 8); }

    bool isEmpty() const { return millisecondsSince1970() == 0; }
    void clear() { std::memset(data, 0, SIZE); }
    int flavorCount() const {
        if (flavor(0) == 0) return 0;
        if (flavor(1) == 0) return 1;
        if (flavor(2) == 0) return 2;
        return 3;
    }
    void copyTo(Donut9a& other) const { std::memcpy(other.data, data, SIZE); }
    void applyTimestamp(int bias = 0);
};

struct BerryDetail {
    uint16_t item;
    uint8_t donutIdx;
    uint8_t spicy, fresh, sweet, bitter, sour;
    uint8_t boost;
    uint16_t calories;
    int flavorScore() const { return spicy + fresh + sweet + bitter + sour; }
};

struct FlavorEntry {
    uint64_t hash;
    const char* name;
};

namespace DonutInfo {
    extern const BerryDetail BERRIES[];
    extern const int BERRY_COUNT;
    extern const FlavorEntry FLAVORS[];
    extern const int FLAVOR_COUNT;
    extern const uint16_t VALID_BERRY_IDS[];
    extern const int VALID_BERRY_COUNT;
    extern const uint8_t SHINY_TEMPLATE[Donut9a::SIZE];

    int findBerryByItem(uint16_t item);
    int findFlavorByHash(uint64_t hash);
    const char* getFlavorName(uint64_t hash);
    const char* getBerryName(uint16_t item);
    uint8_t calcStarRating(int flavorScore);
    int findValidBerryIndex(uint16_t item);

    void fillAllShiny(uint8_t* blockData);
    void fillAllRandomLv3(uint8_t* blockData);
    void compress(uint8_t* blockData);
    void cloneToAll(uint8_t* blockData, int sourceIndex);
    void deleteAll(uint8_t* blockData);

    std::string starsString(uint8_t count);

    // Flavor profile: sums berry contributions into flavors[5]
    // Order: [0]=Spicy, [1]=Fresh, [2]=Sweet, [3]=Bitter, [4]=Sour
    void calcFlavorProfile(const Donut9a& d, int flavors[5]);

    // Recalculate Stars, Calories, LevelBoost from berries
    void recalcStats(Donut9a& d);
}
