#include "ui.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/statvfs.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace {
std::string formatSize(size_t bytes) {
    if (bytes >= 1024 * 1024)
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / 1024) + " KB";
}
} // namespace

// --- Profile Selector ---

void UI::drawProfileSelectorFrame() {
    SDL_SetRenderDrawColor(renderer_, COL_BG.r, COL_BG.g, COL_BG.b, 255);
    SDL_RenderClear(renderer_);

    drawTextCentered("Select Profile", SCREEN_W / 2, 40, COL_TEXT, font_);

    const auto& profiles = account_.profiles();
    int count = (int)profiles.size();

    constexpr int CARD_W = 160;
    constexpr int CARD_H = 200;
    constexpr int CARD_GAP = 20;
    constexpr int ICON_SIZE = 128;

    int totalW = count * CARD_W + (count - 1) * CARD_GAP;
    int startX = (SCREEN_W - totalW) / 2;
    int startY = (SCREEN_H - CARD_H) / 2;

    for (int i = 0; i < count; i++) {
        int cardX = startX + i * (CARD_W + CARD_GAP);
        int cardY = startY;

        if (i == profileSelCursor_) {
            drawRect(cardX, cardY, CARD_W, CARD_H, COL_ROW_SEL);
            drawRectOutline(cardX, cardY, CARD_W, CARD_H, COL_CURSOR, 3);
        } else {
            drawRect(cardX, cardY, CARD_W, CARD_H, COL_PANEL);
        }

        int iconX = cardX + (CARD_W - ICON_SIZE) / 2;
        int iconY = cardY + 10;

        if (profiles[i].iconTexture) {
            SDL_Rect dst = {iconX, iconY, ICON_SIZE, ICON_SIZE};
            SDL_RenderCopy(renderer_, profiles[i].iconTexture, nullptr, &dst);
        } else {
            drawRect(iconX, iconY, ICON_SIZE, ICON_SIZE, COL_EMPTY);
            if (!profiles[i].nickname.empty()) {
                std::string initial(1, profiles[i].nickname[0]);
                drawTextCentered(initial, iconX + ICON_SIZE / 2, iconY + ICON_SIZE / 2,
                                 COL_TEXT, font_);
            }
        }

        std::string name = profiles[i].nickname;
        if (name.length() > 14) name = name.substr(0, 13) + ".";
        drawTextCentered(name, cardX + CARD_W / 2, cardY + ICON_SIZE + 24, COL_TEXT, fontSmall_);
    }

    drawStatusBar("A:Select  -:About  +:Quit");
}

void UI::handleProfileSelectorInput(bool& running) {
    int count = account_.profileCount();
    if (count == 0) return;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
            return;
        }

        if (event.type == SDL_CONTROLLERAXISMOTION) {
            if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                int16_t lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
                int16_t ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY);
                updateStick(lx, ly);
            }
        }

        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    profileSelCursor_ = (profileSelCursor_ + count - 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    profileSelCursor_ = (profileSelCursor_ + 1) % count;
                    break;
                case SDL_CONTROLLER_BUTTON_B: // Switch A = select
                    selectProfile(profileSelCursor_);
                    break;
                case SDL_CONTROLLER_BUTTON_BACK: // - = about
                    showAbout_ = true;
                    break;
                case SDL_CONTROLLER_BUTTON_START:
                    running = false;
                    break;
            }
        }

        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    profileSelCursor_ = (profileSelCursor_ + count - 1) % count;
                    break;
                case SDLK_RIGHT:
                    profileSelCursor_ = (profileSelCursor_ + 1) % count;
                    break;
                case SDLK_a:
                case SDLK_RETURN:
                    selectProfile(profileSelCursor_);
                    break;
                case SDLK_MINUS:
                    showAbout_ = true;
                    break;
                case SDLK_ESCAPE:
                    running = false;
                    break;
            }
        }
    }

    // Joystick repeat
    if (stickDirX_ != 0 || stickDirY_ != 0) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            if (stickDirX_ < 0)
                profileSelCursor_ = (profileSelCursor_ + count - 1) % count;
            else if (stickDirX_ > 0)
                profileSelCursor_ = (profileSelCursor_ + 1) % count;
            stickMoveTime_ = now;
            stickMoved_ = true;
        }
    }
}

void UI::selectProfile(int index) {
    selectedProfile_ = index;

    // Check if this profile has ZA save data
    if (!account_.hasSaveData(index, GameType::ZA)) {
        showMessageAndWait("No Save Data",
            "No Pokemon Legends: Z-A save data found for this profile.");
        return;
    }

    showWorking("Loading save data...");

    // Mount ZA save
    std::string mountPath = account_.mountSave(index, GameType::ZA);
    if (mountPath.empty()) {
        showMessageAndWait("Mount Error", "Failed to mount save data.");
        return;
    }
    savePath_ = mountPath + saveFileNameOf(GameType::ZA);

    // Check space and backup save files
    bool doBackup = true;

#ifdef __SWITCH__
    size_t saveSize = AccountManager::calculateDirSize(mountPath);
    struct statvfs vfs;
    if (statvfs("sdmc:/", &vfs) == 0) {
        size_t freeSpace = (size_t)vfs.f_bavail * vfs.f_bsize;
        if (freeSpace < saveSize * 2) {
            std::string msg = "Free: " + formatSize(freeSpace) +
                ", Need: " + formatSize(saveSize) +
                ".  Continue without backup?";
            if (!showConfirm("Low Storage", msg)) {
                account_.unmountSave();
                return;
            }
            doBackup = false;
        }
    }
#endif

    if (doBackup) {
        std::string backupDir = buildBackupDir();
        bool ok = AccountManager::backupSaveDir(mountPath, backupDir);
        if (!ok) {
            if (!showConfirm("Backup Failed",
                    "Could not back up save data.\nContinue without backup?")) {
                account_.unmountSave();
                return;
            }
        }
    }

    // Load save
    save_.setGameType(GameType::ZA);
    if (!save_.load(savePath_)) {
        showMessageAndWait("Load Error", "Failed to load save file.");
        account_.unmountSave();
        return;
    }

    // Verify encryption round-trip
    std::string rtResult = save_.verifyRoundTrip();
    if (rtResult != "OK")
        showMessageAndWait("Round-Trip Check", rtResult);

    // Reset donut editor state
    listCursor_ = 0;
    listScroll_ = 0;
    editField_ = 0;
    batchCursor_ = 0;
    state_ = UIState::List;

    screen_ = AppScreen::MainView;
}

std::string UI::buildBackupDir() const {
    std::string profileName = "Unknown";
    if (selectedProfile_ >= 0 && selectedProfile_ < account_.profileCount())
        profileName = account_.profiles()[selectedProfile_].pathSafeName;

    std::string dir = basePath_;
    dir += "backups/";
    dir += profileName;
    dir += "/";

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char timestamp[64];
    std::snprintf(timestamp, sizeof(timestamp), "%s_%04d-%02d-%02d_%02d-%02d-%02d",
                  profileName.c_str(),
                  t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                  t->tm_hour, t->tm_min, t->tm_sec);
    dir += timestamp;
    dir += "/";
    return dir;
}

// --- About Popup ---

void UI::drawAboutPopup() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 140});

    constexpr int POP_W = 700;
    constexpr int POP_H = 490;
    int px = (SCREEN_W - POP_W) / 2;
    int py = (SCREEN_H - POP_H) / 2;

    drawRect(px, py, POP_W, POP_H, COL_BATCH_BG);
    drawRectOutline(px, py, POP_W, POP_H, COL_CURSOR, 2);

    int cx = px + POP_W / 2;
    int y = py + 25;

    // Title
    drawTextCentered("pkBakery - Donut Editor", cx, y, COL_STARS, fontLarge_);
    y += 38;

    // Version / author
    drawTextCentered("v" APP_VERSION " - Developed by " APP_AUTHOR, cx, y, COL_TEXT_DIM, fontSmall_);
    y += 22;
    drawTextCentered("github.com/Insektaure", cx, y, COL_TEXT_DIM, fontSmall_);
    y += 30;

    // Divider
    SDL_SetRenderDrawColor(renderer_, COL_EDIT_FIELD.r, COL_EDIT_FIELD.g, COL_EDIT_FIELD.b, 255);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 20;

    // Description
    drawTextCentered("Pokemon Donut Editor for Nintendo Switch", cx, y, COL_TEXT, font_);
    y += 28;
    drawTextCentered("Edit donuts in Pokemon Legends: Z-A save files.", cx, y, COL_TEXT, font_);
    y += 30;

    // Divider
    SDL_SetRenderDrawColor(renderer_, COL_EDIT_FIELD.r, COL_EDIT_FIELD.g, COL_EDIT_FIELD.b, 255);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 20;

    // Credits
    drawTextCentered("Credits", cx, y, COL_ACCENT, font_);
    y += 24;
    drawTextCentered("Based on PKHeX by kwsch & PokeCrypto research.", cx, y, COL_TEXT_DIM, fontSmall_);
    y += 20;
    drawTextCentered("Save backup & write logic based on JKSV by J-D-K.", cx, y, COL_TEXT_DIM, fontSmall_);
    y += 30;

    // Divider
    SDL_SetRenderDrawColor(renderer_, COL_EDIT_FIELD.r, COL_EDIT_FIELD.g, COL_EDIT_FIELD.b, 255);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 20;

    // Controls
    drawTextCentered("Controls", cx, y, COL_ACCENT, font_);
    y += 28;
    drawText("A: Edit    B: Back    X: Delete    Y: Batch/Export/Import", px + 50, y, COL_TEXT_DIM, fontSmall_);
    y += 20;
    drawText("L/R: Page Up/Down    +: Exit Menu    -: About", px + 50, y, COL_TEXT_DIM, fontSmall_);
    y += 20;
    drawText("Edit: DPad U/D: Field    L/R: Value    L1/R1: x10", px + 50, y, COL_TEXT_DIM, fontSmall_);

    // Footer
    drawTextCentered("Press - or B to close", cx, py + POP_H - 22, COL_TEXT_DIM, fontSmall_);
}
