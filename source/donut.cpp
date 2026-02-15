#include "donut.h"
#include <cstdio>
#include <ctime>
#include <cstring>

// --- Donut9a ---

void Donut9a::applyTimestamp(int bias) {
    auto now = static_cast<uint64_t>(time(nullptr)) * 1000ULL;
    if (bias != 0)
        now += static_cast<uint64_t>(bias);
    setMillisecondsSince1970(now);

    // Also set DateTime1900 packed bitfield at offset 0x20
    // Format: bits[0:11]=year-1900, [12:15]=month(0-idx), [16:20]=day,
    //         [21:25]=hour, [26:31]=minute, byte[4]=second
    time_t secs = static_cast<time_t>(now / 1000);
    struct tm* t = localtime(&secs);
    if (t) {
        uint32_t raw = 0;
        raw |= static_cast<uint32_t>(t->tm_year) & 0xFFF;
        raw |= (static_cast<uint32_t>(t->tm_mon) & 0xF) << 12;
        raw |= (static_cast<uint32_t>(t->tm_mday) & 0x1F) << 16;
        raw |= (static_cast<uint32_t>(t->tm_hour) & 0x1F) << 21;
        raw |= (static_cast<uint32_t>(t->tm_min) & 0x3F) << 26;
        std::memcpy(data + 0x20, &raw, 4);
        data[0x24] = static_cast<uint8_t>(t->tm_sec);
        data[0x25] = 0;
        data[0x26] = 0;
        data[0x27] = 0;
    }
}

// --- Simple PRNG for batch operations ---

static uint32_t s_rand = 12345;

static void seedRand() {
    s_rand = static_cast<uint32_t>(time(nullptr));
    if (s_rand == 0) s_rand = 1;
}

static uint32_t nextRand() {
    s_rand ^= s_rand << 13;
    s_rand ^= s_rand >> 17;
    s_rand ^= s_rand << 5;
    return s_rand;
}

// --- Data Tables ---

// Berry data from PKHeX.Core DonutInfo.Berries
// (item, donutIdx, spicy, fresh, sweet, bitter, sour, boost, calories)
const BerryDetail DonutInfo::BERRIES[] = {
    {149,  0, 10,  0,  0,  0,  0, 1,  60},
    {150,  1,  0, 10,  0,  0,  0, 1,  60},
    {151,  2,  0,  0, 10,  0,  0, 1,  60},
    {152,  3,  0,  0,  0, 10,  0, 1,  60},
    {153,  4,  0,  0,  0,  0, 10, 1,  60},
    {155,  5,  5,  5,  0,  5,  5, 1,  60},
    {156,  6,  5,  5,  5,  0,  5, 1,  60},
    {157,  7,  5,  5,  5,  5,  0, 2,  65},
    {158,  8,  0,  5,  5,  5,  5, 2,  65},
    {169,  9, 10,  0, 10, 10,  0, 2,  65},
    {170, 10,  0, 10,  0, 10, 10, 2,  65},
    {171, 11, 10,  0, 10,  0, 10, 2,  65},
    {172, 12, 10, 10,  0, 10,  0, 2,  65},
    {173, 13,  0, 10, 10,  0, 10, 2,  65},
    {174, 14, 15, 10,  0,  0,  0, 2,  65},
    {184, 15, 15,  0, 10,  0,  0, 3,  70},
    {185, 16,  0, 15,  0, 10,  0, 3,  70},
    {186, 17,  0,  0, 15,  0, 10, 3,  70},
    {187, 18, 10,  0,  0, 15,  0, 3,  70},
    {188, 19,  0, 10,  0,  0, 15, 3,  70},
    {189, 20, 15,  0,  0, 10,  0, 3,  70},
    {190, 21,  0, 15,  0,  0, 10, 3,  70},
    {191, 22, 10,  0, 15,  0,  0, 3,  70},
    {192, 23,  0, 10,  0, 15,  0, 3,  70},
    {193, 24,  0,  0, 10,  0, 15, 3,  70},
    {194, 25, 20,  0,  0,  0, 10, 3,  70},
    {195, 26, 10, 20,  0,  0,  0, 3,  70},
    {196, 27,  0, 10, 20,  0,  0, 3,  70},
    {197, 28,  0,  0, 10, 20,  0, 3,  70},
    {198, 29,  0,  0,  0, 10, 20, 3,  70},
    {199, 30, 25, 10,  0,  0,  0, 3,  70},
    {200, 31,  0, 25, 10,  0,  0, 3,  70},
    {686, 32,  0,  0, 25, 10,  0, 3,  70},
    // Z-A berries (2651-2683)
    {2651,  0, 40,  0,  0,  0,  0, 5,  80},
    {2652,  1,  0, 40,  0,  0,  0, 3, 100},
    {2653,  2,  0,  0, 40,  0,  0, 2, 100},
    {2654,  3,  0,  0,  0, 40,  0, 3, 110},
    {2655,  4,  0,  0,  0,  0, 40, 4,  90},
    {2656,  5, 20,  0, 10, 15, 15, 6,  90},
    {2657,  6, 15, 20,  0, 10, 15, 4, 110},
    {2658,  7, 15, 15, 20,  0, 10, 3, 110},
    {2659,  8, 10, 15, 15, 20,  0, 4, 120},
    {2660,  9, 35,  5, 30,  0,  0, 7, 140},
    {2661, 10,  0, 35,  5, 30,  0, 5, 160},
    {2662, 11,  0,  0, 35,  5, 30, 4, 160},
    {2663, 12,  5, 30,  0,  0, 35, 6, 150},
    {2664, 13, 60,  5,  0,  0, 25, 8, 140},
    {2665, 14, 25, 60,  5,  0,  0, 6, 180},
    {2666, 15,  0, 25, 60,  5,  0, 5, 180},
    {2667, 16,  0,  0, 25, 60,  5, 6, 200},
    {2668, 17,  5,  0,  0, 25, 60, 7, 160},
    {2669, 18, 55, 25, 15,  5,  0, 9, 210},
    {2670, 19,  0, 55, 25, 15,  5, 7, 250},
    {2671, 20,  5,  0, 55, 25, 15, 6, 250},
    {2672, 21, 15,  5,  0, 55, 25, 7, 270},
    {2673, 22, 25, 15,  5,  0, 55, 8, 230},
    {2674, 23, 95,  5, 10, 10,  0,10, 240},
    {2675, 24,  0, 95,  5, 10, 10, 8, 300},
    {2676, 25, 10,  0, 95,  5, 10, 7, 300},
    {2677, 26, 10, 10,  0, 95,  5, 8, 330},
    {2678, 27,  5, 10, 10,  0, 95, 9, 270},
    {2679, 28,  0, 65, 85,  0,  0, 8, 370},
    {2680, 29,  0, 85,  0,  0, 65, 9, 370},
    {2681, 30,  0,  0,  0, 85, 65, 9, 400},
    {2682, 31, 85,  0,  0, 65,  0, 9, 370},
    {2683, 32, 65,  0,  0,  0, 85,10, 340},
};

const int DonutInfo::BERRY_COUNT = sizeof(DonutInfo::BERRIES) / sizeof(DonutInfo::BERRIES[0]);

// Valid berry item IDs for cycling in editor (0 = none)
const uint16_t DonutInfo::VALID_BERRY_IDS[] = {
    0,
    149, 150, 151, 152, 153, 155, 156, 157, 158,
    169, 170, 171, 172, 173, 174,
    184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200,
    686,
    2651, 2652, 2653, 2654, 2655, 2656, 2657, 2658, 2659,
    2660, 2661, 2662, 2663, 2664, 2665, 2666, 2667, 2668,
    2669, 2670, 2671, 2672, 2673, 2674, 2675, 2676, 2677, 2678,
    2679, 2680, 2681, 2682, 2683,
};

const int DonutInfo::VALID_BERRY_COUNT = sizeof(DonutInfo::VALID_BERRY_IDS) / sizeof(DonutInfo::VALID_BERRY_IDS[0]);

// Flavor hash table — hashes from PKHeX.Core (FnvHash.HashFnv1a_64), display names from game text
const FlavorEntry DonutInfo::FLAVORS[] = {
    {0, "(none)"},
    // Sweet — Alpha Power / Sparkling Power
    {0xCCFCBB9681D321F1, "Alpha Power (Lv. 1)"},
    {0xCCFCB89681D31CD8, "Alpha Power (Lv. 2)"},
    {0xCCFCB99681D31E8B, "Alpha Power (Lv. 3)"},
    {0xA92EF5B2B4003DDF, "Sparkling Power: Normal (Lv. 1)"},
    {0xA92EF6B2B4003F92, "Sparkling Power: Normal (Lv. 2)"},
    {0xA92EF7B2B4004145, "Sparkling Power: Normal (Lv. 3)"},
    {0x6F78E974FC99251E, "Sparkling Power: Fire (Lv. 1)"},
    {0x6F78E874FC99236B, "Sparkling Power: Fire (Lv. 2)"},
    {0x6F78E774FC9921B8, "Sparkling Power: Fire (Lv. 3)"},
    {0xC5C2B39FF7DDDEF5, "Sparkling Power: Water (Lv. 1)"},
    {0xC5C2B09FF7DDD9DC, "Sparkling Power: Water (Lv. 2)"},
    {0xC5C2B19FF7DDDB8F, "Sparkling Power: Water (Lv. 3)"},
    {0xF9BF21792E9AF2DC, "Sparkling Power: Electric (Lv. 1)"},
    {0xF9BF24792E9AF7F5, "Sparkling Power: Electric (Lv. 2)"},
    {0xF9BF23792E9AF642, "Sparkling Power: Electric (Lv. 3)"},
    {0xB9EB85668F2373C3, "Sparkling Power: Grass (Lv. 1)"},
    {0xB9EB86668F237576, "Sparkling Power: Grass (Lv. 2)"},
    {0xB9EB87668F237729, "Sparkling Power: Grass (Lv. 3)"},
    {0xBA2A639672CE56D2, "Sparkling Power: Ice (Lv. 1)"},
    {0xBA2A629672CE551F, "Sparkling Power: Ice (Lv. 2)"},
    {0xBA2A619672CE536C, "Sparkling Power: Ice (Lv. 3)"},
    {0x541E83577FBC1009, "Sparkling Power: Fighting (Lv. 1)"},
    {0x541E80577FBC0AF0, "Sparkling Power: Fighting (Lv. 2)"},
    {0x541E81577FBC0CA3, "Sparkling Power: Fighting (Lv. 3)"},
    {0x1F44DF68B728B1EF, "Sparkling Power: Poison (Lv. 1)"},
    {0x1F44E068B728B3A2, "Sparkling Power: Poison (Lv. 2)"},
    {0x1F44E168B728B555, "Sparkling Power: Poison (Lv. 3)"},
    {0x742F1750D66962F8, "Sparkling Power: Ground (Lv. 1)"},
    {0x742F1A50D6696811, "Sparkling Power: Ground (Lv. 2)"},
    {0x742F1950D669665E, "Sparkling Power: Ground (Lv. 3)"},
    {0xC138697B4F4B4CC1, "Sparkling Power: Flying (Lv. 1)"},
    {0xC138667B4F4B47A8, "Sparkling Power: Flying (Lv. 2)"},
    {0xC138677B4F4B495B, "Sparkling Power: Flying (Lv. 3)"},
    {0x69FD2589A16AC1CA, "Sparkling Power: Psychic (Lv. 1)"},
    {0x69FD2489A16AC017, "Sparkling Power: Psychic (Lv. 2)"},
    {0x69FD2389A16ABE64, "Sparkling Power: Psychic (Lv. 3)"},
    {0x05EC3138449C8AD3, "Sparkling Power: Bug (Lv. 1)"},
    {0x05EC3238449C8C86, "Sparkling Power: Bug (Lv. 2)"},
    {0x05EC3338449C8E39, "Sparkling Power: Bug (Lv. 3)"},
    {0x16A60F5F34A05A6C, "Sparkling Power: Rock (Lv. 1)"},
    {0x16A6125F34A05F85, "Sparkling Power: Rock (Lv. 2)"},
    {0x16A6115F34A05DD2, "Sparkling Power: Rock (Lv. 3)"},
    {0xBBEBD92E8CB57645, "Sparkling Power: Ghost (Lv. 1)"},
    {0xBBEBD62E8CB5712C, "Sparkling Power: Ghost (Lv. 2)"},
    {0xBBEBD72E8CB572DF, "Sparkling Power: Ghost (Lv. 3)"},
    {0x0E989541C730682E, "Sparkling Power: Dragon (Lv. 1)"},
    {0x0E989441C730667B, "Sparkling Power: Dragon (Lv. 2)"},
    {0x0E989341C73064C8, "Sparkling Power: Dragon (Lv. 3)"},
    {0xFB824941B7F1E4A7, "Sparkling Power: Dark (Lv. 1)"},
    {0xFB824A41B7F1E65A, "Sparkling Power: Dark (Lv. 2)"},
    {0xFB824B41B7F1E80D, "Sparkling Power: Dark (Lv. 3)"},
    {0xBB417F175A6600D0, "Sparkling Power: Steel (Lv. 1)"},
    {0xBB4182175A6605E9, "Sparkling Power: Steel (Lv. 2)"},
    {0xBB4181175A660436, "Sparkling Power: Steel (Lv. 3)"},
    {0x0D37506AA6ECCEFC, "Sparkling Power: Fairy (Lv. 1)"},
    {0x0D37536AA6ECD415, "Sparkling Power: Fairy (Lv. 2)"},
    {0x0D37526AA6ECD262, "Sparkling Power: Fairy (Lv. 3)"},
    {0xD373B02CEF7A3063, "Sparkling Power: All Types (Lv. 1)"},
    {0xD373B12CEF7A3216, "Sparkling Power: All Types (Lv. 2)"},
    {0xD373B22CEF7A33C9, "Sparkling Power: All Types (Lv. 3)"},
    // Spicy — Attack Power / Move Power / Speed Power
    {0x64DCD76EF1844453, "Attack Power (Lv. 1)"},
    {0x64DCD86EF1844606, "Attack Power (Lv. 2)"},
    {0x64DCD96EF18447B9, "Attack Power (Lv. 3)"},
    {0x6D893B78741821AE, "Sp. Atk Power (Lv. 1)"},
    {0x6D893A7874181FFB, "Sp. Atk Power (Lv. 2)"},
    {0x6D89397874181E48, "Sp. Atk Power (Lv. 3)"},
    {0x1ADC7F65399D2FC5, "Move Power: Normal (Lv. 1)"},
    {0x1ADC7C65399D2AAC, "Move Power: Normal (Lv. 2)"},
    {0x1ADC7D65399D2C5F, "Move Power: Normal (Lv. 3)"},
    {0xD31FBD8783511C78, "Move Power: Fire (Lv. 1)"},
    {0xD31FC08783512191, "Move Power: Fire (Lv. 2)"},
    {0xD31FBF8783511FDE, "Move Power: Fire (Lv. 3)"},
    {0x7E35859F64106B6F, "Move Power: Water (Lv. 1)"},
    {0x7E35869F64106D22, "Move Power: Water (Lv. 2)"},
    {0x7E35879F64106ED5, "Move Power: Water (Lv. 3)"},
    {0xC8EDCBC04E527B4A, "Move Power: Electric (Lv. 1)"},
    {0xC8EDCAC04E527997, "Move Power: Electric (Lv. 2)"},
    {0xC8EDC9C04E5277E4, "Move Power: Electric (Lv. 3)"},
    {0x20290FB1FC330641, "Move Power: Grass (Lv. 1)"},
    {0x20290CB1FC330128, "Move Power: Grass (Lv. 2)"},
    {0x20290DB1FC3302DB, "Move Power: Grass (Lv. 3)"},
    {0x51849B43C4EBCD84, "Move Power: Ice (Lv. 1)"},
    {0x51849E43C4EBD29D, "Move Power: Ice (Lv. 2)"},
    {0x51849D43C4EBD0EA, "Move Power: Ice (Lv. 3)"},
    {0xFF0DDF308A9E6B0B, "Move Power: Fighting (Lv. 1)"},
    {0xFF0DE0308A9E6CBE, "Move Power: Fighting (Lv. 2)"},
    {0xFF0DE1308A9E6E71, "Move Power: Fighting (Lv. 3)"},
    {0xC9EDD87FA7D9A829, "Move Power: Poison (Lv. 1)"},
    {0xC9EDD57FA7D9A310, "Move Power: Poison (Lv. 2)"},
    {0xC9EDD67FA7D9A4C3, "Move Power: Poison (Lv. 3)"},
    {0x2F3D34937D29A3F2, "Move Power: Ground (Lv. 1)"},
    {0x2F3D33937D29A23F, "Move Power: Ground (Lv. 2)"},
    {0x2F3D32937D29A08C, "Move Power: Ground (Lv. 3)"},
    {0x2690F089FA95FCF7, "Move Power: Flying (Lv. 1)"},
    {0x2690F189FA95FEAA, "Move Power: Flying (Lv. 2)"},
    {0x2690F289FA96005D, "Move Power: Flying (Lv. 3)"},
    {0x10C36659799A2EE0, "Move Power: Psychic (Lv. 1)"},
    {0x10C36959799A33F9, "Move Power: Psychic (Lv. 2)"},
    {0x10C36859799A3246, "Move Power: Psychic (Lv. 3)"},
    {0xBD946E7AE879D92D, "Move Power: Bug (Lv. 1)"},
    {0xBD946B7AE879D414, "Move Power: Bug (Lv. 2)"},
    {0xBD946C7AE879D5C7, "Move Power: Bug (Lv. 3)"},
    {0xD6D2244BA5767AF6, "Move Power: Rock (Lv. 1)"},
    {0xD6D2234BA5767943, "Move Power: Rock (Lv. 2)"},
    {0xD6D2224BA5767790, "Move Power: Rock (Lv. 3)"},
    {0x21B1603D385CF85B, "Move Power: Ghost (Lv. 1)"},
    {0x21B1613D385CFA0E, "Move Power: Ghost (Lv. 2)"},
    {0x21B1623D385CFBC1, "Move Power: Ghost (Lv. 3)"},
    {0xC5DD1E6872A66694, "Move Power: Dragon (Lv. 1)"},
    {0xC5DD216872A66BAD, "Move Power: Dragon (Lv. 2)"},
    {0xC5DD206872A669FA, "Move Power: Dragon (Lv. 3)"},
    {0x42CC10BEA9F0BA11, "Move Power: Dark (Lv. 1)"},
    {0x42CC0DBEA9F0B4F8, "Move Power: Dark (Lv. 2)"},
    {0x42CC0EBEA9F0B6AB, "Move Power: Dark (Lv. 3)"},
    {0x429AC68EC6515CDA, "Move Power: Steel (Lv. 1)"},
    {0x429AC58EC6515B27, "Move Power: Steel (Lv. 2)"},
    {0x429AC48EC6515974, "Move Power: Steel (Lv. 3)"},
    {0xB2CBA0F9F0DEE7AA, "Move Power: Fairy (Lv. 1)"},
    {0xB2CB9FF9F0DEE5F7, "Move Power: Fairy (Lv. 2)"},
    {0xB2CB9EF9F0DEE444, "Move Power: Fairy (Lv. 3)"},
    {0x633560BB9BE1A5A1, "Speed Power (Lv. 1)"},
    {0x63355DBB9BE1A088, "Speed Power (Lv. 2)"},
    {0x63355EBB9BE1A23B, "Speed Power (Lv. 3)"},
    // Sour — Big Haul Power / Item Power / Mega Power
    {0x99AA421EDED5D2C0, "Big Haul Power (Lv. 1)"},
    {0x99AA451EDED5D7D9, "Big Haul Power (Lv. 2)"},
    {0x99AA441EDED5D626, "Big Haul Power (Lv. 3)"},
    {0x54C2ABEED4759209, "Item Power: Berries (Lv. 1)"},
    {0x54C2A8EED4758CF0, "Item Power: Berries (Lv. 2)"},
    {0x54C2A9EED4758EA3, "Item Power: Berries (Lv. 3)"},
    {0xBACE8C2DC787D8D2, "Item Power: Candies (Lv. 1)"},
    {0xBACE8B2DC787D71F, "Item Power: Candies (Lv. 2)"},
    {0xBACE8A2DC787D56C, "Item Power: Candies (Lv. 3)"},
    {0xAA983C029D989C3B, "Item Power: Treasure (Lv. 1)"},
    {0xAA983D029D989DEE, "Item Power: Treasure (Lv. 2)"},
    {0xAA983E029D989FA1, "Item Power: Treasure (Lv. 3)"},
    {0xAA9D71D2BA27A7F4, "Item Power: Poke Balls (Lv. 1)"},
    {0xAA9D74D2BA27AD0D, "Item Power: Poke Balls (Lv. 2)"},
    {0xAA9D73D2BA27AB5A, "Item Power: Poke Balls (Lv. 3)"},
    {0x9FAAC6104AD9630D, "Item Power: Special (Lv. 1)"},
    {0x9FAAC3104AD95DF4, "Item Power: Special (Lv. 2)"},
    {0x9FAAC4104AD95FA7, "Item Power: Special (Lv. 3)"},
    {0x5EFCFBE5ECF0AD56, "Item Power: Coins (Lv. 1)"},
    {0x5EFCFAE5ECF0ABA3, "Item Power: Coins (Lv. 2)"},
    {0x5EFCF9E5ECF0A9F0, "Item Power: Coins (Lv. 3)"},
    {0x982B8B2119CA2502, "Mega Power Charging (Lv. 1)"},
    {0x982B8A2119CA234F, "Mega Power Charging (Lv. 2)"},
    {0x982B892119CA219C, "Mega Power Charging (Lv. 3)"},
    {0x09D1ECF67D0A80B9, "Mega Power Conservation (Lv. 1)"},
    {0x09D1E9F67D0A7BA0, "Mega Power Conservation (Lv. 2)"},
    {0x09D1EAF67D0A7D53, "Mega Power Conservation (Lv. 3)"},
    // Bitter — Defense Power / Resistance Power
    {0x5738D765D08ED3C5, "Defense Power (Lv. 1)"},
    {0x5738D465D08ECEAC, "Defense Power (Lv. 2)"},
    {0x5738D565D08ED05F, "Defense Power (Lv. 3)"},
    {0xB1F30D967879B7EC, "Sp. Def Power (Lv. 1)"},
    {0xB1F310967879BD05, "Sp. Def Power (Lv. 2)"},
    {0xB1F30F967879BB52, "Sp. Def Power (Lv. 3)"},
    {0xA1392F6F8875E853, "Resistance Power: Normal (Lv. 1)"},
    {0xA139306F8875EA06, "Resistance Power: Normal (Lv. 2)"},
    {0xA139316F8875EBB9, "Resistance Power: Normal (Lv. 3)"},
    {0x054A23C0E5441F4A, "Resistance Power: Fire (Lv. 1)"},
    {0x054A22C0E5441D97, "Resistance Power: Fire (Lv. 2)"},
    {0x054A21C0E5441BE4, "Resistance Power: Fire (Lv. 3)"},
    {0x5C8567B29324AA41, "Resistance Power: Water (Lv. 1)"},
    {0x5C8564B29324A528, "Resistance Power: Water (Lv. 2)"},
    {0x5C8565B29324A6DB, "Resistance Power: Water (Lv. 3)"},
    {0x0F7C15881A42C078, "Resistance Power: Electric (Lv. 1)"},
    {0x0F7C18881A42C591, "Resistance Power: Electric (Lv. 2)"},
    {0x0F7C17881A42C3DE, "Resistance Power: Electric (Lv. 3)"},
    {0xBA91DD9FFB020F6F, "Resistance Power: Grass (Lv. 1)"},
    {0xBA91DE9FFB021122, "Resistance Power: Grass (Lv. 2)"},
    {0xBA91DF9FFB0212D5, "Resistance Power: Grass (Lv. 3)"},
    {0x98C57D52A6A7CBE6, "Resistance Power: Ice (Lv. 1)"},
    {0x98C57C52A6A7CA33, "Resistance Power: Ice (Lv. 2)"},
    {0x98C57B52A6A7C880, "Resistance Power: Ice (Lv. 3)"},
    {0x59E4856B25205D9D, "Resistance Power: Fighting (Lv. 1)"},
    {0x59E4826B25205884, "Resistance Power: Fighting (Lv. 2)"},
    {0x59E4836B25205A37, "Resistance Power: Fighting (Lv. 3)"},
    {0x6EC0AE6433538DE3, "Resistance Power: Poison (Lv. 1)"},
    {0x6EC0AF6433538F96, "Resistance Power: Poison (Lv. 2)"},
    {0x6EC0B06433539149, "Resistance Power: Poison (Lv. 3)"},
    {0xA8844EA1EAC62C7C, "Resistance Power: Ground (Lv. 1)"},
    {0xA88451A1EAC63195, "Resistance Power: Ground (Lv. 2)"},
    {0xA88450A1EAC62FE2, "Resistance Power: Ground (Lv. 3)"},
    {0x2356D877664B3295, "Resistance Power: Flying (Lv. 1)"},
    {0x2356D577664B2D7C, "Resistance Power: Flying (Lv. 2)"},
    {0x2356D677664B2F2F, "Resistance Power: Flying (Lv. 3)"},
    {0x1E3E169DB8C45EBE, "Resistance Power: Psychic (Lv. 1)"},
    {0x1E3E159DB8C45D0B, "Resistance Power: Psychic (Lv. 2)"},
    {0x1E3E149DB8C45B58, "Resistance Power: Psychic (Lv. 3)"},
    {0x5E041EB0583057FF, "Resistance Power: Bug (Lv. 1)"},
    {0x5E041FB0583059B2, "Resistance Power: Bug (Lv. 2)"},
    {0x5E0420B058305B65, "Resistance Power: Bug (Lv. 3)"},
    {0x5DD2D4807490FAC8, "Resistance Power: Rock (Lv. 1)"},
    {0x5DD2D7807490FFE1, "Resistance Power: Rock (Lv. 2)"},
    {0x5DD2D6807490FE2E, "Resistance Power: Rock (Lv. 3)"},
    {0x7F2868BF40E25E11, "Resistance Power: Ghost (Lv. 1)"},
    {0x7F2865BF40E258F8, "Resistance Power: Ghost (Lv. 2)"},
    {0x7F2866BF40E25AAB, "Resistance Power: Ghost (Lv. 3)"},
    {0x7EF71E8F5D4300DA, "Resistance Power: Dragon (Lv. 1)"},
    {0x7EF71D8F5D42FF27, "Resistance Power: Dragon (Lv. 2)"},
    {0x7EF71C8F5D42FD74, "Resistance Power: Dragon (Lv. 3)"},
    {0x5E0DB83DCF4E9C5B, "Resistance Power: Dark (Lv. 1)"},
    {0x5E0DB93DCF4E9E0E, "Resistance Power: Dark (Lv. 2)"},
    {0x5E0DBA3DCF4E9FC1, "Resistance Power: Dark (Lv. 3)"},
    {0x0239766909980A94, "Resistance Power: Steel (Lv. 1)"},
    {0x0239796909980FAD, "Resistance Power: Steel (Lv. 2)"},
    {0x0239786909980DFA, "Resistance Power: Steel (Lv. 3)"},
    {0xC4223D53A3682370, "Resistance Power: Fairy (Lv. 1)"},
    {0xC4224053A3682889, "Resistance Power: Fairy (Lv. 2)"},
    {0xC4223F53A36826D6, "Resistance Power: Fairy (Lv. 3)"},
    // Fresh — Humungo / Teensy / Encounter / Catching Power
    {0xCF24B0DFA2D10011, "Humungo Power (Lv. 1)"},
    {0xCF24ADDFA2D0FAF8, "Humungo Power (Lv. 2)"},
    {0xCF24AEDFA2D0FCAB, "Humungo Power (Lv. 3)"},
    {0xADCF1CA0D67F9CC8, "Teensy Power (Lv. 1)"},
    {0xADCF1FA0D67FA1E1, "Teensy Power (Lv. 2)"},
    {0xADCF1EA0D67FA02E, "Teensy Power (Lv. 3)"},
    {0xAE0066D0BA1EF9FF, "Encounter Power (Lv. 1)"},
    {0xAE0067D0BA1EFBB2, "Encounter Power (Lv. 2)"},
    {0xAE0068D0BA1EFD65, "Encounter Power (Lv. 3)"},
    {0x6E3A5EBE1AB300BE, "Catching Power: Normal (Lv. 1)"},
    {0x6E3A5DBE1AB2FF0B, "Catching Power: Normal (Lv. 2)"},
    {0x6E3A5CBE1AB2FD58, "Catching Power: Normal (Lv. 3)"},
    {0x73532097C839D495, "Catching Power: Fire (Lv. 1)"},
    {0x73531D97C839CF7C, "Catching Power: Fire (Lv. 2)"},
    {0x73531E97C839D12F, "Catching Power: Fire (Lv. 3)"},
    {0xF88096C24CB4CE7C, "Catching Power: Water (Lv. 1)"},
    {0xF88099C24CB4D395, "Catching Power: Water (Lv. 2)"},
    {0xF88098C24CB4D1E2, "Catching Power: Water (Lv. 3)"},
    {0xBEBCF68495422FE3, "Catching Power: Electric (Lv. 1)"},
    {0xBEBCF78495423196, "Catching Power: Electric (Lv. 2)"},
    {0xBEBCF88495423349, "Catching Power: Electric (Lv. 3)"},
    {0xBB95D4B47609E9F2, "Catching Power: Grass (Lv. 1)"},
    {0xBB95D3B47609E83F, "Catching Power: Grass (Lv. 2)"},
    {0xBB95D2B47609E68C, "Catching Power: Grass (Lv. 3)"},
    {0x564678A0A0B9EE29, "Catching Power: Ice (Lv. 1)"},
    {0x564675A0A0B9E910, "Catching Power: Ice (Lv. 2)"},
    {0x564676A0A0B9EAC3, "Catching Power: Ice (Lv. 3)"},
    {0x27AD619F321DDB8F, "Catching Power: Fighting (Lv. 1)"},
    {0x27AD629F321DDD42, "Catching Power: Fighting (Lv. 2)"},
    {0x27AD639F321DDEF5, "Catching Power: Fighting (Lv. 3)"},
    {0xD0721DAD843D5098, "Catching Power: Poison (Lv. 1)"},
    {0xD07220AD843D55B1, "Catching Power: Poison (Lv. 2)"},
    {0xD0721FAD843D53FE, "Catching Power: Poison (Lv. 3)"},
    {0x1A156FD7FA3C1161, "Catching Power: Ground (Lv. 1)"},
    {0x1A156CD7FA3C0C48, "Catching Power: Ground (Lv. 2)"},
    {0x1A156DD7FA3C0DFB, "Catching Power: Ground (Lv. 3)"},
    {0x6EFFA7C0197CC26A, "Catching Power: Flying (Lv. 1)"},
    {0x6EFFA6C0197CC0B7, "Catching Power: Flying (Lv. 2)"},
    {0x6EFFA5C0197CBF04, "Catching Power: Flying (Lv. 3)"},
    {0x5EC93794EF8D4F73, "Catching Power: Psychic (Lv. 1)"},
    {0x5EC93894EF8D5126, "Catching Power: Psychic (Lv. 2)"},
    {0x5EC93994EF8D52D9, "Catching Power: Psychic (Lv. 3)"},
    {0x6F8315BBDF911F0C, "Catching Power: Bug (Lv. 1)"},
    {0x6F8318BBDF912425, "Catching Power: Bug (Lv. 2)"},
    {0x6F8317BBDF912272, "Catching Power: Bug (Lv. 3)"},
    {0xC1A9DF9022880EE5, "Catching Power: Rock (Lv. 1)"},
    {0xC1A9DC90228809CC, "Catching Power: Rock (Lv. 2)"},
    {0xC1A9DD9022880B7F, "Catching Power: Rock (Lv. 3)"},
    {0x64CC1FC98D004ECE, "Catching Power: Ghost (Lv. 1)"},
    {0x64CC1EC98D004D1B, "Catching Power: Ghost (Lv. 2)"},
    {0x64CC1DC98D004B68, "Catching Power: Ghost (Lv. 3)"},
    {0xFDDA4FA34AE15447, "Catching Power: Dragon (Lv. 1)"},
    {0xFDDA50A34AE155FA, "Catching Power: Dragon (Lv. 2)"},
    {0xFDDA51A34AE157AD, "Catching Power: Dragon (Lv. 3)"},
    {0x141E85740556C570, "Catching Power: Dark (Lv. 1)"},
    {0x141E88740556CA89, "Catching Power: Dark (Lv. 2)"},
    {0x141E87740556C8D6, "Catching Power: Dark (Lv. 3)"},
    {0x0F8FD6CC39DD181C, "Catching Power: Steel (Lv. 1)"},
    {0x0F8FD9CC39DD1D35, "Catching Power: Steel (Lv. 2)"},
    {0x0F8FD8CC39DD1B82, "Catching Power: Steel (Lv. 3)"},
    {0xD3223AB99D48C203, "Catching Power: Fairy (Lv. 1)"},
    {0xD3223BB99D48C3B6, "Catching Power: Fairy (Lv. 2)"},
    {0xD3223CB99D48C569, "Catching Power: Fairy (Lv. 3)"},
    {0xDF349EC322BFC85E, "Catching Power: All Types (Lv. 1)"},
    {0xDF349DC322BFC6AB, "Catching Power: All Types (Lv. 2)"},
    {0xDF349CC322BFC4F8, "Catching Power: All Types (Lv. 3)"},
    // Special
    {0x2E27B49C885F70F0, "Pitch-Black Power"},
    {0x2E27B79C885F7609, "Ruby-Red Power"},
    {0x2E27B69C885F7456, "Sapphire-Blue Power"},
    {0x2E27B99C885F796F, "Thunderclap Power"},
    {0x2E27B89C885F77BC, "Emerald-Green Power"},
};

const int DonutInfo::FLAVOR_COUNT = sizeof(DonutInfo::FLAVORS) / sizeof(DonutInfo::FLAVORS[0]);

// Shiny template from PKHeX.Core DonutPocket9a.Template
const uint8_t DonutInfo::SHINY_TEMPLATE[Donut9a::SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // timestamp (set later)
    0x05, 0x54, 0x96, 0x00, 0x10, 0x0E, 0x74, 0x0A, // stars=5, boost=84, sprite=150, cal=3600, berryName=2676
    0x74, 0x0A, 0x74, 0x0A, 0x74, 0x0A, 0x74, 0x0A, // berries 1-4 = 2676
    0x74, 0x0A, 0x74, 0x0A, 0x74, 0x0A, 0x74, 0x0A, // berries 5-8 = 2676
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // datetime1900 (set later)
    0x15, 0xD4, 0xEC, 0xA6, 0x6A, 0x53, 0x37, 0x0D, // flavor0
    0x2E, 0xA0, 0x7F, 0xD6, 0xA0, 0x1E, 0xCF, 0xAD, // flavor1
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // flavor2
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // reserved
};

// --- Lookup functions ---

int DonutInfo::findBerryByItem(uint16_t item) {
    for (int i = 0; i < BERRY_COUNT; i++) {
        if (BERRIES[i].item == item) return i;
    }
    return -1;
}

int DonutInfo::findFlavorByHash(uint64_t hash) {
    for (int i = 0; i < FLAVOR_COUNT; i++) {
        if (FLAVORS[i].hash == hash) return i;
    }
    return -1;
}

const char* DonutInfo::getFlavorName(uint64_t hash) {
    if (hash == 0) return "(none)";
    int idx = findFlavorByHash(hash);
    if (idx >= 0) return FLAVORS[idx].name;
    return "???";
}

const char* DonutInfo::getBerryName(uint16_t item) {
    switch (item) {
        case 0:    return "(none)";
        case 149:  return "Cheri Berry";
        case 150:  return "Chesto Berry";
        case 151:  return "Pecha Berry";
        case 152:  return "Rawst Berry";
        case 153:  return "Aspear Berry";
        case 155:  return "Oran Berry";
        case 156:  return "Persim Berry";
        case 157:  return "Lum Berry";
        case 158:  return "Sitrus Berry";
        case 169:  return "Pomeg Berry";
        case 170:  return "Kelpsy Berry";
        case 171:  return "Qualot Berry";
        case 172:  return "Hondew Berry";
        case 173:  return "Grepa Berry";
        case 174:  return "Tamato Berry";
        case 184:  return "Occa Berry";
        case 185:  return "Passho Berry";
        case 186:  return "Wacan Berry";
        case 187:  return "Rindo Berry";
        case 188:  return "Yache Berry";
        case 189:  return "Chople Berry";
        case 190:  return "Kebia Berry";
        case 191:  return "Shuca Berry";
        case 192:  return "Coba Berry";
        case 193:  return "Payapa Berry";
        case 194:  return "Tanga Berry";
        case 195:  return "Charti Berry";
        case 196:  return "Kasib Berry";
        case 197:  return "Haban Berry";
        case 198:  return "Colbur Berry";
        case 199:  return "Babiri Berry";
        case 200:  return "Chilan Berry";
        case 686:  return "Roseli Berry";
        // Z-A berries (item IDs 2651-2683, names from text_Items_en.txt index N = display line N+1)
        case 2651: return "Hyper Cheri Berry";
        case 2652: return "Hyper Chesto Berry";
        case 2653: return "Hyper Pecha Berry";
        case 2654: return "Hyper Rawst Berry";
        case 2655: return "Hyper Aspear Berry";
        case 2656: return "Hyper Oran Berry";
        case 2657: return "Hyper Persim Berry";
        case 2658: return "Hyper Lum Berry";
        case 2659: return "Hyper Sitrus Berry";
        case 2660: return "Hyper Pomeg Berry";
        case 2661: return "Hyper Kelpsy Berry";
        case 2662: return "Hyper Qualot Berry";
        case 2663: return "Hyper Hondew Berry";
        case 2664: return "Hyper Grepa Berry";
        case 2665: return "Hyper Tamato Berry";
        case 2666: return "Hyper Occa Berry";
        case 2667: return "Hyper Passho Berry";
        case 2668: return "Hyper Wacan Berry";
        case 2669: return "Hyper Rindo Berry";
        case 2670: return "Hyper Yache Berry";
        case 2671: return "Hyper Chople Berry";
        case 2672: return "Hyper Kebia Berry";
        case 2673: return "Hyper Shuca Berry";
        case 2674: return "Hyper Coba Berry";
        case 2675: return "Hyper Payapa Berry";
        case 2676: return "Hyper Tanga Berry";
        case 2677: return "Hyper Charti Berry";
        case 2678: return "Hyper Kasib Berry";
        case 2679: return "Hyper Haban Berry";
        case 2680: return "Hyper Colbur Berry";
        case 2681: return "Hyper Babiri Berry";
        case 2682: return "Hyper Chilan Berry";
        case 2683: return "Hyper Roseli Berry";
        default:   return "???";
    }
}

uint8_t DonutInfo::calcStarRating(int flavorScore) {
    if (flavorScore >= 960) return 5;
    if (flavorScore >= 700) return 4;
    if (flavorScore >= 360) return 3;
    if (flavorScore >= 240) return 2;
    if (flavorScore >= 120) return 1;
    return 0;
}

int DonutInfo::findValidBerryIndex(uint16_t item) {
    for (int i = 0; i < VALID_BERRY_COUNT; i++) {
        if (VALID_BERRY_IDS[i] == item) return i;
    }
    return 0;
}

std::string DonutInfo::starsString(uint8_t count) {
    std::string s;
    for (int i = 0; i < 5; i++) {
        if (i < count)
            s += "\xe2\x98\x85"; // filled star
        else
            s += "\xe2\x98\x86"; // empty star
    }
    return s;
}

// --- Batch operations ---

// Collect all lv3 flavor indices for random fill
static void getLv3FlavorIndices(int* out, int& count) {
    count = 0;
    for (int i = 1; i < DonutInfo::FLAVOR_COUNT; i++) {
        const char* name = DonutInfo::FLAVORS[i].name;
        int len = 0;
        while (name[len]) len++;
        // Check for "(Lv. 3)" at end
        if (len >= 7 &&
            name[len-7] == '(' && name[len-6] == 'L' && name[len-5] == 'v' &&
            name[len-4] == '.' && name[len-3] == ' ' && name[len-2] == '3' &&
            name[len-1] == ')')
            out[count++] = i;
    }
}

void DonutInfo::fillOneShiny(Donut9a& d) {
    std::memcpy(d.data, SHINY_TEMPLATE, Donut9a::SIZE);
    d.applyTimestamp();
    recalcStats(d);
    d.setFlavor(0, 0xD373B22CEF7A33C9ULL); // Sparkling Power: All Types (Lv. 3)
    d.setFlavor(1, 0xCCFCB99681D31E8BULL); // Alpha Power (Lv. 3)
    d.setFlavor(2, 0);
}

void DonutInfo::fillOneShinyRandom(Donut9a& d) {
    seedRand();
    std::memcpy(d.data, SHINY_TEMPLATE, Donut9a::SIZE);
    d.applyTimestamp();
    recalcStats(d);

    int sparkLv3[32]; int sparkCount = 0;
    for (int i = 1; i < FLAVOR_COUNT; i++) {
        const char* n = FLAVORS[i].name;
        if (n[0] == 'S' && n[1] == 'p' && n[2] == 'a' && n[3] == 'r') {
            int len = 0; while (n[len]) len++;
            if (len >= 7 && n[len-2] == '3')
                sparkLv3[sparkCount++] = i;
        }
    }

    int humungoIdx = findFlavorByHash(0xCF24AEDFA2D0FCAB);
    int teensyIdx  = findFlavorByHash(0xADCF1EA0D67FA02E);
    int alphaIdx   = findFlavorByHash(0xCCFCB99681D31E8B);

    int catchLv3[32]; int catchCount = 0;
    for (int i = 1; i < FLAVOR_COUNT; i++) {
        const char* n = FLAVORS[i].name;
        if (n[0] == 'C' && n[1] == 'a' && n[2] == 't' && n[3] == 'c') {
            int len = 0; while (n[len]) len++;
            if (len >= 7 && n[len-2] == '3')
                catchLv3[catchCount++] = i;
        }
    }

    if (sparkCount > 0)
        d.setFlavor(0, FLAVORS[sparkLv3[nextRand() % sparkCount]].hash);
    int roll = nextRand() % 3;
    if (roll == 0 && humungoIdx >= 0)
        d.setFlavor(1, FLAVORS[humungoIdx].hash);
    else if (roll == 1 && teensyIdx >= 0)
        d.setFlavor(1, FLAVORS[teensyIdx].hash);
    else if (alphaIdx >= 0)
        d.setFlavor(1, FLAVORS[alphaIdx].hash);
    if (catchCount > 0)
        d.setFlavor(2, FLAVORS[catchLv3[nextRand() % catchCount]].hash);
}

static bool isSparkling(int flavorIdx) {
    const char* n = DonutInfo::FLAVORS[flavorIdx].name;
    return n[0] == 'S' && n[1] == 'p' && n[2] == 'a' && n[3] == 'r';
}

// Pick 3 distinct Lv3 flavors with at most 1 Sparkling Power
static void pickRandomLv3Flavors(const int* lv3Indices, int lv3Count, uint64_t out[3]) {
    bool hasSparkling = false;
    int picks[3] = {-1, -1, -1};
    for (int p = 0; p < 3; p++) {
        for (;;) {
            int idx = lv3Indices[nextRand() % lv3Count];
            bool dup = false;
            for (int q = 0; q < p; q++) if (picks[q] == idx) { dup = true; break; }
            if (dup) continue;
            if (isSparkling(idx) && hasSparkling) continue;
            picks[p] = idx;
            if (isSparkling(idx)) hasSparkling = true;
            break;
        }
    }
    for (int i = 0; i < 3; i++)
        out[i] = DonutInfo::FLAVORS[picks[i]].hash;
}

void DonutInfo::fillOneRandomLv3(Donut9a& d) {
    seedRand();
    int lv3Indices[512]; int lv3Count = 0;
    getLv3FlavorIndices(lv3Indices, lv3Count);
    if (lv3Count < 3) return;

    d.clear();
    // Randomize 8 berries (skip index 0 = none)
    for (int i = 0; i < Donut9a::MAX_BERRIES; i++)
        d.setBerry(i, VALID_BERRY_IDS[1 + nextRand() % (VALID_BERRY_COUNT - 1)]);
    recalcStats(d);

    uint64_t flavors[3];
    pickRandomLv3Flavors(lv3Indices, lv3Count, flavors);
    d.setFlavor(0, flavors[0]);
    d.setFlavor(1, flavors[1]);
    d.setFlavor(2, flavors[2]);

    d.applyTimestamp();
}

void DonutInfo::fillAllShiny(uint8_t* blockData) {
    seedRand();
    // Collect Sparkling Power lv3 indices for flavor0
    int sparkLv3[32];
    int sparkCount = 0;
    for (int i = 1; i < FLAVOR_COUNT; i++) {
        const char* n = FLAVORS[i].name;
        // Match "Sparkling Power: * (Lv. 3)" but not "Alpha Power"
        if (n[0] == 'S' && n[1] == 'p' && n[2] == 'a' && n[3] == 'r') {
            int len = 0; while (n[len]) len++;
            if (len >= 7 && n[len-2] == '3')
                sparkLv3[sparkCount++] = i;
        }
    }

    // Find Humungo Power (Lv. 3) and Teensy Power (Lv. 3) and Alpha Power (Lv. 3)
    int humungoIdx = findFlavorByHash(0xCF24AEDFA2D0FCAB); // Humungo Power (Lv. 3)
    int teensyIdx  = findFlavorByHash(0xADCF1EA0D67FA02E); // Teensy Power (Lv. 3)
    int alphaIdx   = findFlavorByHash(0xCCFCB99681D31E8B); // Alpha Power (Lv. 3)

    // Collect Catching Power lv3 indices for flavor2
    int catchLv3[32];
    int catchCount = 0;
    for (int i = 1; i < FLAVOR_COUNT; i++) {
        const char* n = FLAVORS[i].name;
        // Match "Catching Power: * (Lv. 3)" but skip Humungo/Teensy/Encounter
        if (n[0] == 'C' && n[1] == 'a' && n[2] == 't' && n[3] == 'c') {
            int len = 0; while (n[len]) len++;
            if (len >= 7 && n[len-2] == '3')
                catchLv3[catchCount++] = i;
        }
    }

    for (int i = 0; i < Donut9a::MAX_COUNT; i++) {
        uint8_t* entry = blockData + i * Donut9a::SIZE;
        std::memcpy(entry, SHINY_TEMPLATE, Donut9a::SIZE);

        Donut9a d{entry};

        recalcStats(d);

        // Randomize flavors like PKHeX's ApplyShinySizeCatch
        if (sparkCount > 0)
            d.setFlavor(0, FLAVORS[sparkLv3[nextRand() % sparkCount]].hash);

        int roll = nextRand() % 3;
        if (roll == 0 && humungoIdx >= 0)
            d.setFlavor(1, FLAVORS[humungoIdx].hash);
        else if (roll == 1 && teensyIdx >= 0)
            d.setFlavor(1, FLAVORS[teensyIdx].hash);
        else if (alphaIdx >= 0)
            d.setFlavor(1, FLAVORS[alphaIdx].hash);

        if (catchCount > 0)
            d.setFlavor(2, FLAVORS[catchLv3[nextRand() % catchCount]].hash);

        d.applyTimestamp(i);
    }
}

void DonutInfo::fillAllRandomLv3(uint8_t* blockData) {
    seedRand();
    int lv3Indices[512];
    int lv3Count = 0;
    getLv3FlavorIndices(lv3Indices, lv3Count);
    if (lv3Count < 3) return;

    for (int i = 0; i < Donut9a::MAX_COUNT; i++) {
        uint8_t* entry = blockData + i * Donut9a::SIZE;
        std::memset(entry, 0, Donut9a::SIZE);

        Donut9a d{entry};
        // Randomize 8 berries (skip index 0 = none)
        for (int j = 0; j < Donut9a::MAX_BERRIES; j++)
            d.setBerry(j, VALID_BERRY_IDS[1 + nextRand() % (VALID_BERRY_COUNT - 1)]);
        recalcStats(d);
        uint64_t flavors[3];
        pickRandomLv3Flavors(lv3Indices, lv3Count, flavors);
        d.setFlavor(0, flavors[0]);
        d.setFlavor(1, flavors[1]);
        d.setFlavor(2, flavors[2]);
        d.applyTimestamp(i);
    }
}

void DonutInfo::compress(uint8_t* blockData) {
    int writePos = 0;
    uint8_t temp[Donut9a::SIZE];
    for (int readPos = 0; readPos < Donut9a::MAX_COUNT; readPos++) {
        uint8_t* src = blockData + readPos * Donut9a::SIZE;
        uint64_t ts;
        std::memcpy(&ts, src, 8);
        if (ts == 0) continue; // empty

        if (writePos != readPos) {
            uint8_t* dst = blockData + writePos * Donut9a::SIZE;
            std::memcpy(temp, src, Donut9a::SIZE);
            std::memcpy(dst, temp, Donut9a::SIZE);
            std::memset(src, 0, Donut9a::SIZE);
        }
        writePos++;
    }
    // Clear remaining slots
    for (int i = writePos; i < Donut9a::MAX_COUNT; i++) {
        std::memset(blockData + i * Donut9a::SIZE, 0, Donut9a::SIZE);
    }
}

void DonutInfo::cloneToAll(uint8_t* blockData, int sourceIndex) {
    if (sourceIndex < 0 || sourceIndex >= Donut9a::MAX_COUNT) return;
    uint8_t* src = blockData + sourceIndex * Donut9a::SIZE;
    for (int i = 0; i < Donut9a::MAX_COUNT; i++) {
        if (i == sourceIndex) continue;
        uint8_t* dst = blockData + i * Donut9a::SIZE;
        std::memcpy(dst, src, Donut9a::SIZE);
        // Give unique timestamps
        Donut9a d{dst};
        d.applyTimestamp(i);
    }
}

void DonutInfo::deleteAll(uint8_t* blockData) {
    std::memset(blockData, 0, Donut9a::MAX_COUNT * Donut9a::SIZE);
}

void DonutInfo::calcFlavorProfile(const Donut9a& d, int flavors[5]) {
    flavors[0] = flavors[1] = flavors[2] = flavors[3] = flavors[4] = 0;
    for (int i = 0; i < 8; i++) {
        uint16_t item = d.berry(i);
        int idx = findBerryByItem(item);
        if (idx < 0) continue;
        const auto& b = BERRIES[idx];
        flavors[0] += b.spicy;
        flavors[1] += b.fresh;
        flavors[2] += b.sweet;
        flavors[3] += b.bitter;
        flavors[4] += b.sour;
    }
}

void DonutInfo::recalcStats(Donut9a& d) {
    int sumBoost = 0;
    int sumCal = 0;
    int flavorScore = 0;
    int flavors[5] = {};
    for (int i = 0; i < 8; i++) {
        uint16_t item = d.berry(i);
        int idx = findBerryByItem(item);
        if (idx < 0) continue;
        const auto& b = BERRIES[idx];
        sumBoost += b.boost;
        sumCal += b.calories;
        flavorScore += b.flavorScore();
        flavors[0] += b.spicy;
        flavors[1] += b.fresh;
        flavors[2] += b.sweet;
        flavors[3] += b.bitter;
        flavors[4] += b.sour;
    }
    uint8_t stars = calcStarRating(flavorScore);
    // Star rating multiplier: (10 + stars) / 10 applied via integer division
    int mult = 10 + stars;
    int totalBoost = sumBoost * mult / 10;
    int totalCal = sumCal * mult / 10;
    d.setCalories(static_cast<uint16_t>(totalCal > 9999 ? 9999 : totalCal));
    d.setLevelBoost(static_cast<uint8_t>(totalBoost));
    d.setStars(stars);
    d.setBerryName(d.berry(0));

    // Auto-calculate sprite: donutIdx * 6 + dominant flavor variant
    // Variant: 0=sweet, 1=spicy, 2=sour, 3=bitter, 4=fresh, 5=mix
    int berryIdx = findBerryByItem(d.berry(0));
    if (berryIdx >= 0) {
        uint8_t donutIdx = BERRIES[berryIdx].donutIdx;
        // Find dominant flavor
        int maxVal = 0;
        for (int i = 0; i < 5; i++)
            if (flavors[i] > maxVal) maxVal = flavors[i];
        int variant;
        if (maxVal == 0) {
            variant = 0;
        } else {
            // Count how many flavors share the max
            int count = 0;
            for (int i = 0; i < 5; i++)
                if (flavors[i] == maxVal) count++;
            if (count > 1) {
                variant = 5; // mix
            } else {
                // Map: [0]=spicy→1, [1]=fresh→4, [2]=sweet→0, [3]=bitter→3, [4]=sour→2
                static const int FLAVOR_TO_VARIANT[] = {1, 4, 0, 3, 2};
                for (int i = 0; i < 5; i++) {
                    if (flavors[i] == maxVal) { variant = FLAVOR_TO_VARIANT[i]; break; }
                }
            }
        }
        d.setDonutSprite(static_cast<uint16_t>(donutIdx * 6 + variant));
    }
}
