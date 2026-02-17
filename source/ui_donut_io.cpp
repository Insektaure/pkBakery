#include "ui.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>

#ifdef __SWITCH__
#include <switch.h>
#endif

// --- Export / Import ---

std::string UI::sanitizeFilename(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ' ')
            out += c;
    }
    // Trim leading/trailing spaces
    size_t start = out.find_first_not_of(' ');
    if (start == std::string::npos) return "donut";
    size_t end = out.find_last_not_of(' ');
    out = out.substr(start, end - start + 1);
    // Limit length (40 chars max to fit popup display)
    if (out.size() > 40) out.resize(40);
    if (out.empty()) out = "donut";
    return out;
}

std::string UI::buildDefaultExportName(int index) {
    Donut9a d = save_.getDonut(index);
    if (!d.data || d.isEmpty()) return "donut";

    std::string name = DonutInfo::getBerryName(d.berryName());
    // Replace spaces with underscores
    for (char& c : name) {
        if (c == ' ') c = '_';
    }

    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s_%dstar", name.c_str(), d.stars());
    return buf;
}

std::string UI::showKeyboard(const std::string& defaultText) {
#ifdef __SWITCH__
    SwkbdConfig kbd;
    swkbdCreate(&kbd, 0);
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, "Enter filename");
    swkbdConfigSetInitialText(&kbd, defaultText.c_str());
    swkbdConfigSetStringLenMax(&kbd, 40);

    char result[256] = {};
    Result rc = swkbdShow(&kbd, result, sizeof(result));
    swkbdClose(&kbd);

    if (R_SUCCEEDED(rc) && result[0] != '\0')
        return std::string(result);
    return "";
#else
    // PC fallback: return default name (no interactive keyboard)
    return defaultText;
#endif
}

bool UI::exportDonut(int index) {
    Donut9a d = save_.getDonut(index);
    if (!d.data || d.isEmpty()) {
        showMessageAndWait("Export Error", "Cannot export an empty donut slot.");
        return false;
    }

    // Get filename from user
    std::string defaultName = buildDefaultExportName(index);
    std::string filename = showKeyboard(defaultName);
    if (filename.empty()) return false; // cancelled

    filename = sanitizeFilename(filename);

    // Ensure donuts/ directory exists
    std::string dir = basePath_ + "donuts/";
    mkdir(dir.c_str(), 0755);

    std::string path = dir + filename + ".donut";

    // Check if file exists
    FILE* check = fopen(path.c_str(), "rb");
    if (check) {
        fclose(check);
        if (!showConfirm("Overwrite?", "File already exists:", filename))
            return false;
    }

    // Write the 72 bytes
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        showMessageAndWait("Export Error", "Failed to create file.");
        return false;
    }
    size_t written = fwrite(d.data, 1, Donut9a::SIZE, f);
    fclose(f);

    if (written != Donut9a::SIZE) {
        showMessageAndWait("Export Error", "Failed to write donut data.");
        return false;
    }

    showMessageAndWait("Exported", "Saved as:", filename);
    return true;
}

void UI::scanDonutFiles() {
    importFiles_.clear();

    std::string dir = basePath_ + "donuts/";
    mkdir(dir.c_str(), 0755);

    DIR* dp = opendir(dir.c_str());
    if (!dp) return;

    struct dirent* ep;
    while ((ep = readdir(dp)) != nullptr) {
        std::string name = ep->d_name;
        if (name.size() > 6 && name.substr(name.size() - 6) == ".donut")
            importFiles_.push_back(name);
    }
    closedir(dp);

    std::sort(importFiles_.begin(), importFiles_.end());
}

bool UI::importDonut(const std::string& filename) {
    std::string path = basePath_ + "donuts/" + filename;

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        showMessageAndWait("Import Error", "Failed to open file.");
        return false;
    }

    uint8_t buf[Donut9a::SIZE];
    size_t nread = fread(buf, 1, Donut9a::SIZE, f);
    fclose(f);

    if (nread != Donut9a::SIZE) {
        showMessageAndWait("Import Error", "Invalid file size (expected 72 bytes).");
        return false;
    }

    // Validate donut data
    Donut9a tmp{buf};
    // Check all 8 berries are valid IDs
    for (int i = 0; i < 8; i++) {
        uint16_t b = tmp.berry(i);
        if (b != 0 && DonutInfo::findBerryByItem(b) < 0) {
            showMessageAndWait("Import Error", "Invalid berry ID in slot " + std::to_string(i + 1) + ".");
            return false;
        }
    }
    // Check all 3 flavors are valid hashes
    for (int i = 0; i < 3; i++) {
        uint64_t fh = tmp.flavor(i);
        if (fh != 0 && DonutInfo::findFlavorByHash(fh) < 0) {
            showMessageAndWait("Import Error", "Invalid flavor hash in slot " + std::to_string(i + 1) + ".");
            return false;
        }
    }
    // Check stars in range
    if (tmp.stars() > 5) {
        showMessageAndWait("Import Error", "Invalid star rating.");
        return false;
    }

    Donut9a d = save_.getDonut(listCursor_);
    if (!d.data) {
        showMessageAndWait("Import Error", "Invalid donut slot.");
        return false;
    }

    std::memcpy(d.data, buf, Donut9a::SIZE);
    d.applyTimestamp();
    DonutInfo::recalcStats(d);

    showMessageAndWait("Imported", "Loaded into slot #" + std::to_string(listCursor_ + 1));
    return true;
}
