#include "ui.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>
#include <sys/stat.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

// --- Init / Shutdown ---

bool UI::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
        return false;

    if (TTF_Init() < 0) {
        SDL_Quit();
        return false;
    }

    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & IMG_INIT_PNG) == 0) {
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    window_ = SDL_CreateWindow(APP_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window_) {
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        SDL_DestroyWindow(window_);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

#ifdef __SWITCH__
    PlFontData fontData;
    plInitialize(PlServiceType_System);
    plGetSharedFontByType(&fontData, PlSharedFontType_Standard);
    SDL_RWops* rw1 = SDL_RWFromMem(fontData.address, fontData.size);
    SDL_RWops* rw2 = SDL_RWFromMem(fontData.address, fontData.size);
    SDL_RWops* rw3 = SDL_RWFromMem(fontData.address, fontData.size);
    font_      = TTF_OpenFontRW(rw1, 0, 18);
    fontSmall_ = TTF_OpenFontRW(rw2, 0, 14);
    fontLarge_ = TTF_OpenFontRW(rw3, 0, 24);
#else
    font_      = TTF_OpenFont("romfs/fonts/default.ttf", 18);
    fontSmall_ = TTF_OpenFont("romfs/fonts/default.ttf", 14);
    fontLarge_ = TTF_OpenFont("romfs/fonts/default.ttf", 24);
#endif

    // Fallback to romfs font if system font failed
    if (!font_)
        font_ = TTF_OpenFont("romfs:/fonts/default.ttf", 18);
    if (!fontSmall_)
        fontSmall_ = TTF_OpenFont("romfs:/fonts/default.ttf", 14);
    if (!fontLarge_)
        fontLarge_ = TTF_OpenFont("romfs:/fonts/default.ttf", 24);

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            pad_ = SDL_GameControllerOpen(i);
            break;
        }
    }

    return true;
}

void UI::shutdown() {
    freeSprites();
    account_.freeTextures();
    if (fontLarge_) TTF_CloseFont(fontLarge_);
    if (fontSmall_) TTF_CloseFont(fontSmall_);
    if (font_)      TTF_CloseFont(font_);
    if (pad_)       SDL_GameControllerClose(pad_);
    if (renderer_)  SDL_DestroyRenderer(renderer_);
    if (window_)    SDL_DestroyWindow(window_);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
#ifdef __SWITCH__
    plExit();
#endif
}

// --- Splash Screen ---

void UI::showSplash() {
    if (!renderer_) return;

    SDL_Texture* splash = nullptr;

#ifdef __SWITCH__
    SDL_Surface* surf = IMG_Load("romfs:/splash.png");
#else
    SDL_Surface* surf = IMG_Load("romfs/splash.png");
#endif
    if (surf) {
        splash = SDL_CreateTextureFromSurface(renderer_, surf);
        SDL_FreeSurface(surf);
    }

    // Hold for 2.5 seconds
    uint32_t start = SDL_GetTicks();
    while (SDL_GetTicks() - start < 2500) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {}
        SDL_SetRenderDrawColor(renderer_, COL_BG.r, COL_BG.g, COL_BG.b, 255);
        SDL_RenderClear(renderer_);
        if (splash) {
            SDL_RenderCopy(renderer_, splash, nullptr, nullptr);
        } else {
            drawTextCentered(APP_TITLE, SCREEN_W / 2, SCREEN_H / 2 - 20, COL_CURSOR, fontLarge_);
            drawTextCentered("v" APP_VERSION, SCREEN_W / 2, SCREEN_H / 2 + 20, COL_TEXT_DIM, fontSmall_);
        }
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }

    // Fade out over 0.5s
    for (int alpha = 0; alpha <= 255; alpha += 6) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {}
        SDL_SetRenderDrawColor(renderer_, COL_BG.r, COL_BG.g, COL_BG.b, 255);
        SDL_RenderClear(renderer_);
        if (splash) {
            SDL_SetTextureAlphaMod(splash, 255 - alpha);
            SDL_RenderCopy(renderer_, splash, nullptr, nullptr);
        }
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }

    if (splash)
        SDL_DestroyTexture(splash);
}

// --- Message / Working ---

void UI::showMessageAndWait(const std::string& title, const std::string& body) {
    if (!renderer_) return;

    bool waiting = true;
    while (waiting) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                waiting = false;
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                    waiting = false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_b || event.key.keysym.sym == SDLK_ESCAPE)
                    waiting = false;
            }
        }

        SDL_SetRenderDrawColor(renderer_, COL_BG.r, COL_BG.g, COL_BG.b, 255);
        SDL_RenderClear(renderer_);

        drawTextCentered(title, SCREEN_W / 2, SCREEN_H / 2 - 40, COLOR_RED, fontLarge_);
        drawTextCentered(body, SCREEN_W / 2, SCREEN_H / 2 + 15, COL_TEXT_DIM, font_);
        drawTextCentered("Press B to dismiss", SCREEN_W / 2, SCREEN_H / 2 + 65, COL_TEXT_DIM, fontSmall_);

        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
}

bool UI::showConfirm(const std::string& title, const std::string& body) {
    if (!renderer_) return false;

    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                return false;
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B)  // Switch A = confirm
                    return true;
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A)  // Switch B = cancel
                    return false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_RETURN)
                    return true;
                if (event.key.keysym.sym == SDLK_b || event.key.keysym.sym == SDLK_ESCAPE)
                    return false;
            }
        }

        SDL_SetRenderDrawColor(renderer_, COL_BG.r, COL_BG.g, COL_BG.b, 255);
        SDL_RenderClear(renderer_);

        constexpr int POP_W = 480;
        constexpr int POP_H = 180;
        int px = (SCREEN_W - POP_W) / 2;
        int py = (SCREEN_H - POP_H) / 2;
        drawRect(px, py, POP_W, POP_H, COL_BATCH_BG);
        drawRectOutline(px, py, POP_W, POP_H, COL_CURSOR, 2);

        drawTextCentered(title, SCREEN_W / 2, py + 35, COL_CURSOR, fontLarge_);
        drawTextCentered(body, SCREEN_W / 2, py + 85, COL_TEXT, font_);
        drawTextCentered("A: Confirm    B: Cancel", SCREEN_W / 2, py + POP_H - 30, COL_TEXT_DIM, fontSmall_);

        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
}

void UI::showWorking(const std::string& msg) {
    if (!renderer_) return;

    SDL_SetRenderDrawColor(renderer_, COL_BG.r, COL_BG.g, COL_BG.b, 255);
    SDL_RenderClear(renderer_);

    constexpr int POP_W = 400;
    constexpr int POP_H = 160;
    int popX = (SCREEN_W - POP_W) / 2;
    int popY = (SCREEN_H - POP_H) / 2;
    drawRect(popX, popY, POP_W, POP_H, COL_PANEL);
    drawRectOutline(popX, popY, POP_W, POP_H, COL_TEXT_DIM, 2);

    // Gear icon
    int gearCX = SCREEN_W / 2;
    int gearCY = popY + 58;
    constexpr int OUTER_R = 28;
    constexpr int INNER_R = 18;
    constexpr int HOLE_R  = 9;
    constexpr int TEETH    = 8;
    constexpr int TOOTH_W  = 12;

    auto fillCircle = [&](int cx, int cy, int r, SDL_Color c) {
        SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
        for (int dy = -r; dy <= r; dy++) {
            int dx = static_cast<int>(std::sqrt(r * r - dy * dy));
            SDL_RenderDrawLine(renderer_, cx - dx, cy + dy, cx + dx, cy + dy);
        }
    };

    SDL_Color gearColor = COL_ACCENT;
    SDL_SetRenderDrawColor(renderer_, gearColor.r, gearColor.g, gearColor.b, gearColor.a);
    for (int i = 0; i < TEETH; i++) {
        double angle = i * (3.14159265 * 2.0 / TEETH);
        int tx = gearCX + static_cast<int>((INNER_R + 7) * std::cos(angle));
        int ty = gearCY + static_cast<int>((INNER_R + 7) * std::sin(angle));
        SDL_Rect tooth = {tx - TOOTH_W / 2, ty - TOOTH_W / 2, TOOTH_W, TOOTH_W};
        SDL_RenderFillRect(renderer_, &tooth);
    }

    fillCircle(gearCX, gearCY, OUTER_R, gearColor);
    fillCircle(gearCX, gearCY, HOLE_R, COL_PANEL);

    drawTextCentered(msg, SCREEN_W / 2, popY + POP_H - 32, COL_TEXT, font_);

    SDL_RenderPresent(renderer_);
}

// --- Main Run Loop ---

void UI::run(const std::string& basePath, const std::string& savePath) {
    basePath_ = basePath;
    savePath_ = savePath;

#ifdef __SWITCH__
    showWorking("Loading profiles...");
    if (account_.init() && account_.loadProfiles(renderer_)) {
        screen_ = AppScreen::ProfileSelector;
    } else {
        // No account service — load save directly from basePath
        showWorking("Loading save data...");
        save_.setGameType(GameType::ZA);
        savePath_ = basePath_ + "main";
        if (save_.load(savePath_)) {
            std::string rtResult = save_.verifyRoundTrip();
            if (rtResult != "OK")
                showMessageAndWait("Round-Trip Check", rtResult);
        }
        screen_ = AppScreen::MainView;
    }
#else
    // PC: load save directly
    save_.setGameType(GameType::ZA);
    savePath_ = basePath_ + "main";
    if (save_.load(savePath_)) {
        std::string rtResult = save_.verifyRoundTrip();
        if (rtResult != "OK")
            showMessageAndWait("Round-Trip Check", rtResult);
    }
    screen_ = AppScreen::MainView;
#endif

    bool running = true;

    while (running) {
        // About popup intercepts input from any screen
        if (showAbout_) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running = false; break; }
                if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK ||
                        event.cbutton.button == SDL_CONTROLLER_BUTTON_A)
                        showAbout_ = false;
                }
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_MINUS ||
                        event.key.keysym.sym == SDLK_b ||
                        event.key.keysym.sym == SDLK_ESCAPE)
                        showAbout_ = false;
                }
            }
            if (screen_ == AppScreen::ProfileSelector) {
                drawProfileSelectorFrame();
            } else {
                SDL_SetRenderDrawColor(renderer_, COL_BG.r, COL_BG.g, COL_BG.b, 255);
                SDL_RenderClear(renderer_);
                drawDonutHeader();
                if (save_.hasDonutBlock()) {
                    drawListPanel();
                    if (state_ == UIState::Edit) drawEditPanel();
                    else drawDetailPanel();
                }
                if (state_ == UIState::Batch) drawBatchMenu();
                drawDonutStatusBar();
            }
            drawAboutPopup();
            SDL_RenderPresent(renderer_);
            SDL_Delay(16);
            continue;
        }

        AppScreen screenBefore = screen_;
        if (screen_ == AppScreen::ProfileSelector) {
            handleProfileSelectorInput(running);
            if (screen_ == screenBefore) drawProfileSelectorFrame();
        } else {
            handleDonutInput(running);
            if (saveNow_) {
                showWorking("Saving...");
                if (save_.isLoaded())
                    save_.save(savePath_);
                account_.commitSave();
                saveNow_ = false;
            }
            if (screen_ == screenBefore) {
                SDL_SetRenderDrawColor(renderer_, COL_BG.r, COL_BG.g, COL_BG.b, 255);
                SDL_RenderClear(renderer_);
                drawDonutHeader();
                if (save_.hasDonutBlock()) {
                    drawListPanel();
                    if (state_ == UIState::Edit) drawEditPanel();
                    else drawDetailPanel();
                } else {
                    drawText("No donut data found in save file.", DETAIL_X + 20, SCREEN_H / 2, COL_TEXT, font_);
                    drawText("Make sure you have a Pokemon Legends Z-A save.", DETAIL_X + 20, SCREEN_H / 2 + 30, COL_TEXT_DIM, fontSmall_);
                }
                if (state_ == UIState::Batch) drawBatchMenu();
                drawDonutStatusBar();
            }
        }
        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }

    account_.unmountSave();
    account_.shutdown();
}

// --- Joystick ---

void UI::updateStick(int16_t axisX, int16_t axisY) {
    int newDirX = 0, newDirY = 0;
    if (axisX < -STICK_DEADZONE) newDirX = -1;
    else if (axisX > STICK_DEADZONE) newDirX = 1;
    if (axisY < -STICK_DEADZONE) newDirY = -1;
    else if (axisY > STICK_DEADZONE) newDirY = 1;

    if (newDirX != stickDirX_ || newDirY != stickDirY_) {
        stickDirX_ = newDirX;
        stickDirY_ = newDirY;
        stickMoved_ = false;
        stickMoveTime_ = 0;
    }
}

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

    // Backup save files
    std::string backupDir = buildBackupDir();
    AccountManager::backupSaveDir(mountPath, backupDir);

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
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 187});

    constexpr int POP_W = 700;
    constexpr int POP_H = 300;
    int px = (SCREEN_W - POP_W) / 2;
    int py = (SCREEN_H - POP_H) / 2;

    drawRect(px, py, POP_W, POP_H, COL_PANEL);
    drawRectOutline(px, py, POP_W, POP_H, COL_EDIT_FIELD, 2);

    int cx = px + POP_W / 2;
    int y = py + 25;

    drawTextCentered("pkBakery - Donut Editor", cx, y, COL_STARS, fontLarge_);
    y += 38;

    drawTextCentered("v" APP_VERSION " - Developed by " APP_AUTHOR, cx, y, COL_TEXT_DIM, fontSmall_);
    y += 22;
    drawTextCentered("github.com/Insektaure", cx, y, COL_TEXT_DIM, fontSmall_);
    y += 30;

    // Divider
    SDL_SetRenderDrawColor(renderer_, COL_EDIT_FIELD.r, COL_EDIT_FIELD.g, COL_EDIT_FIELD.b, 255);
    SDL_RenderDrawLine(renderer_, px + 30, y, px + POP_W - 30, y);
    y += 20;

    drawTextCentered("Pokemon Donut Editor for Nintendo Switch", cx, y, COL_TEXT, font_);
    y += 28;
    drawTextCentered("Edit donuts in Pokemon Legends: Z-A save files.", cx, y, COL_TEXT, font_);
    y += 35;

    drawTextCentered("Press - or B to close", cx, y, COL_TEXT_DIM, fontSmall_);
}

// --- Sprite Cache ---

SDL_Texture* UI::getDonutSprite(uint16_t spriteId, uint8_t stars) {
    // Build resource name using PKHeX DonutSpriteUtil logic
    std::string name;
    if (spriteId >= 198 && spriteId <= 202) {
        static const char* SPECIAL[] = {
            "donut_uni491",  // 198: Bad Dreams Cruller
            "donut_uni383",  // 199: Omega Old-Fashioned Donut
            "donut_uni382",  // 200: Alpha Old-Fashioned Donut
            "donut_uni384",  // 201: Delta Old-Fashioned Donut
            "donut_uni807",  // 202: Plasma-Glazed Donut
        };
        name = SPECIAL[spriteId - 198];
    } else {
        static const char* FLAVORS[] = {"sweet", "spicy", "sour", "bitter", "fresh", "mix"};
        int variant = spriteId % 6;
        int star = (stars <= 5) ? stars : 0;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "donut_%s%02d", FLAVORS[variant], star);
        name = buf;
    }

    // Check cache
    auto it = spriteCache_.find(name);
    if (it != spriteCache_.end())
        return it->second;

    // Load from romfs
    std::string path;
#ifdef __SWITCH__
    path = "romfs:/sprites/" + name + ".png";
#else
    path = "romfs/sprites/" + name + ".png";
#endif

    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) {
        spriteCache_[name] = nullptr;
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_FreeSurface(surf);
    spriteCache_[name] = tex;
    return tex;
}

void UI::freeSprites() {
    for (auto& [key, tex] : spriteCache_) {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    spriteCache_.clear();
}

// --- Drawing Primitives ---

void UI::drawRect(int x, int y, int w, int h, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &r);
}

void UI::drawRectOutline(int x, int y, int w, int h, SDL_Color c, int t) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    for (int i = 0; i < t; i++) {
        SDL_Rect r = {x + i, y + i, w - 2*i, h - 2*i};
        SDL_RenderDrawRect(renderer_, &r);
    }
}

void UI::drawText(const std::string& text, int x, int y, SDL_Color c, TTF_Font* f) {
    if (!f) f = font_;
    if (!f || text.empty()) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, text.c_str(), c);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void UI::drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color, TTF_Font* f) {
    if (!f || text.empty()) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
    SDL_Rect dst = {cx - surf->w / 2, cy - surf->h / 2, surf->w, surf->h};
    SDL_RenderCopy(renderer_, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void UI::drawTextRight(const std::string& text, int x, int y, SDL_Color c, TTF_Font* f) {
    if (!f) f = font_;
    if (!f || text.empty()) return;
    int tw, th;
    TTF_SizeUTF8(f, text.c_str(), &tw, &th);
    drawText(text, x - tw, y, c, f);
}

void UI::drawStatusBar(const std::string& msg) {
    drawRect(0, SCREEN_H - 35, SCREEN_W, 35, COL_STATUS_BG);
    drawText(msg, 15, SCREEN_H - 30, COL_STATUS, fontSmall_);
}

// --- Flavor Radar Chart ---

void UI::drawFlavorRadar(int cx, int cy, int radius, const int flavors[5]) {
    // Display order (clockwise from top):
    // 0=Spicy(top), 1=Sour(top-right), 2=Fresh(bot-right), 3=Bitter(bot-left), 4=Sweet(top-left)
    // Maps from calc order: [0]=Spicy, [1]=Fresh, [2]=Sweet, [3]=Bitter, [4]=Sour
    const int display[5] = { flavors[0], flavors[4], flavors[1], flavors[3], flavors[2] };
    static const char* LABELS[5] = { "Spicy", "Sour", "Fresh", "Bitter", "Sweet" };
    static const SDL_Color LABEL_COLORS[5] = {
        {255, 100, 100, 255},  // Spicy - red
        {255, 180, 80, 255},   // Sour - orange
        {100, 220, 130, 255},  // Fresh - green
        {100, 150, 255, 255},  // Bitter - blue
        {255, 130, 200, 255},  // Sweet - magenta
    };

    constexpr double PI = 3.14159265358979;
    constexpr double ANGLE_STEP = 2.0 * PI / 5.0;
    constexpr int MAX_STAT = 760;

    // Precompute pentagon vertex positions (unit circle)
    double vx[5], vy[5];
    for (int i = 0; i < 5; i++) {
        double angle = (-PI / 2.0) + i * ANGLE_STEP;
        vx[i] = std::cos(angle);
        vy[i] = std::sin(angle);
    }

    // Draw reference pentagon outline
    SDL_SetRenderDrawColor(renderer_, COL_EDIT_FIELD.r, COL_EDIT_FIELD.g, COL_EDIT_FIELD.b, 255);
    for (int i = 0; i < 5; i++) {
        int j = (i + 1) % 5;
        SDL_RenderDrawLine(renderer_,
            cx + (int)(vx[i] * radius), cy + (int)(vy[i] * radius),
            cx + (int)(vx[j] * radius), cy + (int)(vy[j] * radius));
        // Spokes
        SDL_RenderDrawLine(renderer_, cx, cy,
            cx + (int)(vx[i] * radius), cy + (int)(vy[i] * radius));
    }

    // Compute scaled stat points using PKHeX per-vertex scaling
    int sx[5], sy[5];
    for (int i = 0; i < 5; i++) {
        int stat = display[i];
        int statMax;
        if (stat <= 350)
            statMax = stat + 200;
        else if (stat <= 700)
            statMax = ((stat + 99) / 100) * 100;
        else
            statMax = MAX_STAT;

        float scale = (statMax > 0) ? static_cast<float>(stat) / statMax : 0.0f;
        if (scale > 1.0f) scale = 1.0f;
        if (scale == 0.0f) scale = 0.10f; // baseline

        sx[i] = cx + (int)(vx[i] * radius * scale);
        sy[i] = cy + (int)(vy[i] * radius * scale);
    }

    // Fill stat polygon using scanline (counter-clockwise order: 0,4,3,2,1)
    int ordered[5] = { 0, 4, 3, 2, 1 };
    int polyX[5], polyY[5];
    for (int i = 0; i < 5; i++) {
        polyX[i] = sx[ordered[i]];
        polyY[i] = sy[ordered[i]];
    }

    // Scanline fill for convex polygon
    int minY = polyY[0], maxY = polyY[0];
    for (int i = 1; i < 5; i++) {
        if (polyY[i] < minY) minY = polyY[i];
        if (polyY[i] > maxY) maxY = polyY[i];
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 0, 160); // semi-transparent yellow
    for (int y = minY; y <= maxY; y++) {
        int xMin = 9999, xMax = -9999;
        for (int i = 0; i < 5; i++) {
            int j = (i + 1) % 5;
            int y0 = polyY[i], y1 = polyY[j];
            int x0 = polyX[i], x1 = polyX[j];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                int ix = x0 + (y - y0) * (x1 - x0) / (y1 - y0);
                if (ix < xMin) xMin = ix;
                if (ix > xMax) xMax = ix;
            }
        }
        if (xMin <= xMax)
            SDL_RenderDrawLine(renderer_, xMin, y, xMax, y);
    }

    // Draw stat polygon outline
    SDL_SetRenderDrawColor(renderer_, 255, 255, 0, 255);
    for (int i = 0; i < 5; i++) {
        int j = (i + 1) % 5;
        SDL_RenderDrawLine(renderer_, polyX[i], polyY[i], polyX[j], polyY[j]);
    }

    // Draw labels and values
    for (int i = 0; i < 5; i++) {
        int lx = cx + (int)(vx[i] * (radius + 18));
        int ly = cy + (int)(vy[i] * (radius + 18));
        drawTextCentered(LABELS[i], lx, ly - 8, LABEL_COLORS[i], fontSmall_);
        drawTextCentered(std::to_string(display[i]), lx, ly + 8, COL_TEXT, fontSmall_);
    }
}

// --- Donut Editor: Header ---

void UI::drawDonutHeader() {
    drawRect(0, 0, SCREEN_W, HEADER_H, COL_HEADER);
    drawText(APP_TITLE, 20, 10, COL_CURSOR, fontLarge_);

    if (save_.hasDonutBlock()) {
        int count = save_.donutCount();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d / %d donuts   Page %d/%d",
                      count, Donut9a::MAX_COUNT, currentPage() + 1, totalPages());
        drawTextRight(buf, SCREEN_W - 20, 14, COL_TEXT_DIM, font_);
    }
}

// --- Donut Editor: Status Bar ---

void UI::drawDonutStatusBar() {
    drawRect(0, SCREEN_H - STATUS_H, SCREEN_W, STATUS_H, COL_STATUS_BG);
    const char* msg = "";
    switch (state_) {
        case UIState::List:
            msg = "DPad:Move  L/R:Page  A:Edit  X:Delete  Y:Batch  +:Save&Exit  -:About  B:Back";
            break;
        case UIState::Edit:
            msg = "DPad U/D:Field  L/R:Value  L1/R1:x10  A:Confirm  B:Cancel";
            break;
        case UIState::Batch:
            msg = "DPad U/D:Select  A:Confirm  B:Cancel";
            break;
    }
    drawText(msg, 15, SCREEN_H - STATUS_H + 8, COL_STATUS, fontSmall_);
}

// --- Donut Editor: List Panel ---

void UI::drawListPanel() {
    drawRect(LIST_X, CONTENT_Y, LIST_W, CONTENT_H, COL_PANEL);

    int y = CONTENT_Y + 4;
    drawText("  #", LIST_X + 8, y, COL_TEXT_DIM, fontSmall_);
    drawText("Stars", LIST_X + 83, y, COL_TEXT_DIM, fontSmall_);
    drawText("Cal", LIST_X + 230, y, COL_TEXT_DIM, fontSmall_);
    drawText("Boost", LIST_X + 295, y, COL_TEXT_DIM, fontSmall_);
    drawText("Flavors", LIST_X + 355, y, COL_TEXT_DIM, fontSmall_);

    drawRect(LIST_X + 4, y + 18, LIST_W - 8, 1, COL_TEXT_DIM);

    int startY = CONTENT_Y + 26;
    int startIdx = listScroll_;
    int visibleRows = (CONTENT_H - 28) / ROW_H;

    for (int row = 0; row < visibleRows; row++) {
        int idx = startIdx + row;
        if (idx >= Donut9a::MAX_COUNT) break;

        int ry = startY + row * ROW_H;
        Donut9a d = save_.getDonut(idx);
        bool selected = (idx == listCursor_);

        if (selected)
            drawRect(LIST_X + 2, ry, LIST_W - 4, ROW_H - 1, COL_ROW_SEL);
        else if (row % 2 == 1)
            drawRect(LIST_X + 2, ry, LIST_W - 4, ROW_H - 1, COL_ROW_HOVER);

        if (selected)
            drawText(">", LIST_X + 6, ry + 6, COL_CURSOR, fontSmall_);

        if (d.data && !d.isEmpty()) {
            char ibuf[8];
            std::snprintf(ibuf, sizeof(ibuf), "%3d", idx + 1);
            drawText(ibuf, LIST_X + 18, ry + 6, COL_TEXT, fontSmall_);

            // Donut icon (24x24)
            SDL_Texture* icon = getDonutSprite(d.donutSprite(), d.stars());
            if (icon) {
                SDL_Rect dst = {LIST_X + 55, ry + 3, 24, 24};
                SDL_RenderCopy(renderer_, icon, nullptr, &dst);
            }

            std::string stars = DonutInfo::starsString(d.stars());
            drawText(stars, LIST_X + 83, ry + 6, COL_STARS, fontSmall_);

            char cbuf[8];
            std::snprintf(cbuf, sizeof(cbuf), "%d", d.calories());
            drawText(cbuf, LIST_X + 230, ry + 6, COL_TEXT, fontSmall_);

            char bbuf[8];
            std::snprintf(bbuf, sizeof(bbuf), "+%d", d.levelBoost());
            drawText(bbuf, LIST_X + 295, ry + 6, COL_TEXT, fontSmall_);

            char fbuf[4];
            std::snprintf(fbuf, sizeof(fbuf), "%d", d.flavorCount());
            drawText(fbuf, LIST_X + 370, ry + 6, COL_ACCENT, fontSmall_);
        } else {
            char ibuf[8];
            std::snprintf(ibuf, sizeof(ibuf), "%3d", idx + 1);
            drawText(ibuf, LIST_X + 18, ry + 6, COL_EMPTY, fontSmall_);
            drawText("(empty)", LIST_X + 83, ry + 6, COL_EMPTY, fontSmall_);
        }
    }

    if (listScroll_ > 0)
        drawText("\xe2\x96\xb2", LIST_X + LIST_W - 20, CONTENT_Y + 6, COL_ACCENT, fontSmall_);
    if (listScroll_ + visibleRows < Donut9a::MAX_COUNT)
        drawText("\xe2\x96\xbc", LIST_X + LIST_W - 20, CONTENT_Y + CONTENT_H - 18, COL_ACCENT, fontSmall_);
}

// --- Donut Editor: Detail Panel ---

void UI::drawDetailPanel() {
    int px = DETAIL_X;
    int pw = DETAIL_W;
    drawRect(px, CONTENT_Y, pw, CONTENT_H, COL_PANEL);

    Donut9a d = save_.getDonut(listCursor_);
    if (!d.data || d.isEmpty()) {
        drawText("Empty donut slot", px + 20, CONTENT_Y + 20, COL_EMPTY, font_);
        drawText("Press A to set up a new donut, or Y for batch fill.", px + 20, CONTENT_Y + 50, COL_TEXT_DIM, fontSmall_);
        return;
    }

    int y = CONTENT_Y + 10;
    char buf[128];

    // Donut sprite (80x80) at top-right of detail panel
    constexpr int SPRITE_SZ = 80;
    SDL_Texture* sprite = getDonutSprite(d.donutSprite(), d.stars());
    if (sprite) {
        SDL_Rect dst = {px + pw - SPRITE_SZ - 20, y, SPRITE_SZ, SPRITE_SZ};
        SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
    }

    std::snprintf(buf, sizeof(buf), "Donut #%d", listCursor_ + 1);
    drawText(buf, px + 20, y, COL_CURSOR, fontLarge_);
    y += 30;

    std::snprintf(buf, sizeof(buf), "Sprite %d", d.donutSprite());
    drawText(buf, px + 20, y, COL_TEXT_DIM, fontSmall_);
    y += 24;

    drawText("Stars:", px + 20, y, COL_TEXT_DIM, font_);
    drawText(DonutInfo::starsString(d.stars()), px + 150, y, COL_STARS, font_);
    y += 28;

    std::snprintf(buf, sizeof(buf), "%d", d.calories());
    drawText("Calories:", px + 20, y, COL_TEXT_DIM, font_);
    drawText(buf, px + 150, y, COL_TEXT, font_);
    y += 28;

    std::snprintf(buf, sizeof(buf), "+%d", d.levelBoost());
    drawText("Level Boost:", px + 20, y, COL_TEXT_DIM, font_);
    drawText(buf, px + 150, y, COL_TEXT, font_);
    y += 28;

    drawText("Berry Name:", px + 20, y, COL_TEXT_DIM, font_);
    std::snprintf(buf, sizeof(buf), "%s (%d)", DonutInfo::getBerryName(d.berryName()), d.berryName());
    drawText(buf, px + 150, y, COL_TEXT, font_);
    y += 34;

    drawText("Berries:", px + 20, y, COL_ACCENT, font_);
    y += 24;
    for (int i = 0; i < 4; i++) {
        uint16_t b1 = d.berry(i);
        uint16_t b2 = d.berry(i + 4);
        std::snprintf(buf, sizeof(buf), "%d: %-12s  %d: %s",
                      i + 1, DonutInfo::getBerryName(b1),
                      i + 5, DonutInfo::getBerryName(b2));
        drawText(buf, px + 30, y, COL_TEXT, fontSmall_);
        y += 22;
    }
    y += 8;

    drawText("Flavors:", px + 20, y, COL_ACCENT, font_);
    y += 24;
    for (int i = 0; i < 3; i++) {
        uint64_t fh = d.flavor(i);
        std::snprintf(buf, sizeof(buf), "%d: %s", i + 1, DonutInfo::getFlavorName(fh));
        SDL_Color fc = (fh != 0) ? COL_TEXT : COL_EMPTY;
        drawText(buf, px + 30, y, fc, fontSmall_);
        y += 22;
    }

    y += 12;
    uint64_t ts = d.millisecondsSince1970();
    if (ts > 0) {
        time_t sec = static_cast<time_t>(ts / 1000);
        struct tm* tm = localtime(&sec);
        if (tm) {
            std::snprintf(buf, sizeof(buf), "Created: %04d-%02d-%02d %02d:%02d:%02d",
                          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                          tm->tm_hour, tm->tm_min, tm->tm_sec);
            drawText(buf, px + 20, y, COL_TEXT_DIM, fontSmall_);
        }
    }

    // Flavor radar chart (right side of detail panel)
    int flavorVals[5];
    DonutInfo::calcFlavorProfile(d, flavorVals);
    int radarCX = px + pw - 160;
    int radarCY = CONTENT_Y + CONTENT_H / 2 + 40;
    drawFlavorRadar(radarCX, radarCY, 80, flavorVals);
}

// --- Donut Editor: Edit Panel ---

static const char* EDIT_LABELS[] = {
    "Stars", "Level Boost", "Calories", "Sprite ID", "Berry Name",
    "Berry 1", "Berry 2", "Berry 3", "Berry 4",
    "Berry 5", "Berry 6", "Berry 7", "Berry 8",
    "Flavor 1", "Flavor 2", "Flavor 3",
};

void UI::drawEditPanel() {
    int px = DETAIL_X;
    int pw = DETAIL_W;
    drawRect(px, CONTENT_Y, pw, CONTENT_H, COL_PANEL);

    Donut9a d = save_.getDonut(listCursor_);
    if (!d.data) return;

    char buf[128];
    int y = CONTENT_Y + 10;

    // Donut sprite preview (64x64) at top-right
    constexpr int EDIT_SPRITE_SZ = 64;
    SDL_Texture* sprite = getDonutSprite(d.donutSprite(), d.stars());
    if (sprite) {
        SDL_Rect dst = {px + pw - EDIT_SPRITE_SZ - 20, y, EDIT_SPRITE_SZ, EDIT_SPRITE_SZ};
        SDL_RenderCopy(renderer_, sprite, nullptr, &dst);
    }

    std::snprintf(buf, sizeof(buf), "Editing Donut #%d", listCursor_ + 1);
    drawText(buf, px + 20, y, COL_CURSOR, fontLarge_);
    y += 40;

    int fieldCount = static_cast<int>(EditField::COUNT);
    for (int f = 0; f < fieldCount; f++) {
        bool sel = (f == editField_);
        int fy = y + f * 26;

        if (sel)
            drawRect(px + 10, fy - 2, pw - 20, 24, COL_EDIT_FIELD);

        if (sel)
            drawText(">", px + 14, fy, COL_CURSOR, font_);

        drawText(EDIT_LABELS[f], px + 35, fy, sel ? COL_TEXT : COL_TEXT_DIM, font_);

        std::string val;
        switch (static_cast<EditField>(f)) {
            case EditField::Stars:
                val = DonutInfo::starsString(d.stars()) + " (" + std::to_string(d.stars()) + ")";
                break;
            case EditField::LevelBoost:
                val = "+" + std::to_string(d.levelBoost());
                break;
            case EditField::Calories:
                val = std::to_string(d.calories());
                break;
            case EditField::DonutSprite:
                val = std::to_string(d.donutSprite());
                break;
            case EditField::BerryName:
                std::snprintf(buf, sizeof(buf), "%s (%d)", DonutInfo::getBerryName(d.berryName()), d.berryName());
                val = buf;
                break;
            case EditField::Berry1: case EditField::Berry2:
            case EditField::Berry3: case EditField::Berry4:
            case EditField::Berry5: case EditField::Berry6:
            case EditField::Berry7: case EditField::Berry8: {
                int bi = f - static_cast<int>(EditField::Berry1);
                uint16_t bv = d.berry(bi);
                std::snprintf(buf, sizeof(buf), "%s (%d)", DonutInfo::getBerryName(bv), bv);
                val = buf;
                break;
            }
            case EditField::Flavor0: case EditField::Flavor1:
            case EditField::Flavor2: {
                int fi = f - static_cast<int>(EditField::Flavor0);
                val = DonutInfo::getFlavorName(d.flavor(fi));
                break;
            }
            default: break;
        }

        SDL_Color vc = sel ? COL_EDIT_VAL : COL_TEXT;
        drawText(val, px + 200, fy, vc, font_);

        if (sel) {
            drawText("<", px + 185, fy, COL_ACCENT, font_);
            drawTextRight(">", px + pw - 20, fy, COL_ACCENT, font_);
        }
    }
}

// --- Donut Editor: Batch Menu ---

static const char* BATCH_LABELS[] = {
    "Fill All: Shiny Power",
    "Fill All: Random Lv3",
    "Clone Selected to All",
    "Delete Selected Donut",
    "Delete ALL Donuts",
    "Compress (remove gaps)",
    "Cancel",
};

void UI::drawBatchMenu() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 140});

    int mw = 380, mh = 300;
    int mx = (SCREEN_W - mw) / 2;
    int my = (SCREEN_H - mh) / 2;

    drawRect(mx, my, mw, mh, COL_BATCH_BG);
    drawRectOutline(mx, my, mw, mh, COL_CURSOR, 2);

    drawText("Batch Operations", mx + 20, my + 14, COL_CURSOR, fontLarge_);

    int opCount = static_cast<int>(BatchOp::COUNT);
    for (int i = 0; i < opCount; i++) {
        int oy = my + 55 + i * 32;
        bool sel = (i == batchCursor_);

        if (sel)
            drawRect(mx + 10, oy - 2, mw - 20, 28, COL_EDIT_FIELD);

        if (sel)
            drawText(">", mx + 14, oy + 2, COL_CURSOR, font_);
        drawText(BATCH_LABELS[i], mx + 35, oy + 2, sel ? COL_TEXT : COL_TEXT_DIM, font_);
    }
}

// --- Donut Editor: Input ---

void UI::handleDonutInput(bool& running) {
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
            continue;
        }

        int button = -1;

        if (event.type == SDL_CONTROLLERBUTTONDOWN)
            button = event.cbutton.button;

        if (event.type == SDL_KEYDOWN) {
            // Keyboard mapping matches Switch convention:
            // SDL_CONTROLLER_BUTTON_B = Switch A (confirm)
            // SDL_CONTROLLER_BUTTON_A = Switch B (back)
            switch (event.key.keysym.sym) {
                case SDLK_UP:     button = SDL_CONTROLLER_BUTTON_DPAD_UP; break;
                case SDLK_DOWN:   button = SDL_CONTROLLER_BUTTON_DPAD_DOWN; break;
                case SDLK_LEFT:   button = SDL_CONTROLLER_BUTTON_DPAD_LEFT; break;
                case SDLK_RIGHT:  button = SDL_CONTROLLER_BUTTON_DPAD_RIGHT; break;
                case SDLK_a:
                case SDLK_RETURN: button = SDL_CONTROLLER_BUTTON_B; break;   // confirm
                case SDLK_b:
                case SDLK_ESCAPE: button = SDL_CONTROLLER_BUTTON_A; break;   // back
                case SDLK_x:      button = SDL_CONTROLLER_BUTTON_Y; break;   // Switch X
                case SDLK_y:      button = SDL_CONTROLLER_BUTTON_X; break;   // Switch Y
                case SDLK_q:      button = SDL_CONTROLLER_BUTTON_LEFTSHOULDER; break;
                case SDLK_e:      button = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER; break;
                case SDLK_PLUS:   button = SDL_CONTROLLER_BUTTON_START; break;
                case SDLK_MINUS:  button = SDL_CONTROLLER_BUTTON_BACK; break;
                default: break;
            }
        }

        if (event.type == SDL_CONTROLLERBUTTONUP || event.type == SDL_KEYUP) {
            clearRepeat();
            continue;
        }

        if (button < 0) continue;

        // Setup repeat for directional and shoulder buttons
        if (button == SDL_CONTROLLER_BUTTON_DPAD_UP ||
            button == SDL_CONTROLLER_BUTTON_DPAD_DOWN ||
            button == SDL_CONTROLLER_BUTTON_DPAD_LEFT ||
            button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ||
            button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER ||
            button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
            repeatDir_ = button;
            repeatStart_ = SDL_GetTicks();
            repeatLast_ = repeatStart_;
        }

        if (!save_.hasDonutBlock()) {
            if (button == SDL_CONTROLLER_BUTTON_START) { saveNow_ = true; running = false; }
            if (button == SDL_CONTROLLER_BUTTON_BACK)  { showAbout_ = true; }
            if (button == SDL_CONTROLLER_BUTTON_A) { // Switch B = back
                account_.unmountSave();
                screen_ = AppScreen::ProfileSelector;
            }
            continue;
        }

        switch (state_) {
            case UIState::List:  handleListInput(button, running); break;
            case UIState::Edit:  handleEditInput(button); break;
            case UIState::Batch: handleBatchInput(button); break;
        }
    }

    // Handle button repeat
    if (repeatDir_ != 0 && save_.hasDonutBlock()) {
        if (handleRepeat(repeatDir_)) {
            bool dummy = true;
            switch (state_) {
                case UIState::List:  handleListInput(repeatDir_, dummy); break;
                case UIState::Edit:  handleEditInput(repeatDir_); break;
                case UIState::Batch: handleBatchInput(repeatDir_); break;
            }
        }
    }

    // Handle joystick repeat
    if ((stickDirX_ != 0 || stickDirY_ != 0) && save_.hasDonutBlock()) {
        uint32_t now = SDL_GetTicks();
        uint32_t delay = stickMoved_ ? STICK_REPEAT_DELAY : STICK_INITIAL_DELAY;
        if (now - stickMoveTime_ >= delay) {
            bool dummy = true;
            switch (state_) {
                case UIState::List:
                    if (stickDirY_ < 0)
                        handleListInput(SDL_CONTROLLER_BUTTON_DPAD_UP, dummy);
                    else if (stickDirY_ > 0)
                        handleListInput(SDL_CONTROLLER_BUTTON_DPAD_DOWN, dummy);
                    break;
                case UIState::Edit:
                    if (stickDirY_ < 0)
                        handleEditInput(SDL_CONTROLLER_BUTTON_DPAD_UP);
                    else if (stickDirY_ > 0)
                        handleEditInput(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                    if (stickDirX_ < 0)
                        handleEditInput(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                    else if (stickDirX_ > 0)
                        handleEditInput(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                    break;
                case UIState::Batch:
                    if (stickDirY_ < 0)
                        handleBatchInput(SDL_CONTROLLER_BUTTON_DPAD_UP);
                    else if (stickDirY_ > 0)
                        handleBatchInput(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                    break;
            }
            stickMoveTime_ = now;
            stickMoved_ = true;
        }
    }
}

bool UI::handleRepeat(uint32_t button) {
    if (button != repeatDir_) return false;
    uint32_t now = SDL_GetTicks();
    uint32_t elapsed = now - repeatStart_;
    if (elapsed < REPEAT_DELAY) return false;
    if (now - repeatLast_ < REPEAT_RATE) return false;
    repeatLast_ = now;
    return true;
}

void UI::clearRepeat() {
    repeatDir_ = 0;
}

// --- Donut Editor: List Input ---

void UI::handleListInput(int button, bool& running) {
    int visibleRows = (CONTENT_H - 28) / ROW_H;

    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            if (listCursor_ > 0) {
                listCursor_--;
                if (listCursor_ < listScroll_)
                    listScroll_ = listCursor_;
            }
            break;

        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            if (listCursor_ < Donut9a::MAX_COUNT - 1) {
                listCursor_++;
                if (listCursor_ >= listScroll_ + visibleRows)
                    listScroll_ = listCursor_ - visibleRows + 1;
            }
            break;

        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: {
            listCursor_ -= visibleRows;
            if (listCursor_ < 0) listCursor_ = 0;
            listScroll_ -= visibleRows;
            if (listScroll_ < 0) listScroll_ = 0;
            break;
        }
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: {
            listCursor_ += visibleRows;
            if (listCursor_ >= Donut9a::MAX_COUNT) listCursor_ = Donut9a::MAX_COUNT - 1;
            listScroll_ += visibleRows;
            int maxScroll = Donut9a::MAX_COUNT - visibleRows;
            if (maxScroll < 0) maxScroll = 0;
            if (listScroll_ > maxScroll) listScroll_ = maxScroll;
            break;
        }

        case SDL_CONTROLLER_BUTTON_B: { // Switch A = confirm/edit
            Donut9a d = save_.getDonut(listCursor_);
            if (d.data && d.isEmpty()) {
                std::memcpy(d.data, DonutInfo::SHINY_TEMPLATE, Donut9a::SIZE);
                d.applyTimestamp();
            }
            editField_ = 0;
            state_ = UIState::Edit;
            break;
        }

        case SDL_CONTROLLER_BUTTON_Y: { // Switch X = delete
            Donut9a d = save_.getDonut(listCursor_);
            if (d.data)
                d.clear();
            break;
        }

        case SDL_CONTROLLER_BUTTON_X: // Switch Y = batch
            batchCursor_ = 0;
            state_ = UIState::Batch;
            break;

        case SDL_CONTROLLER_BUTTON_START: // + = save & exit
            if (showConfirm("Save & Exit", "Save changes and exit to home?")) {
                saveNow_ = true;
                running = false;
            }
            break;

        case SDL_CONTROLLER_BUTTON_BACK: // - = about
            showAbout_ = true;
            break;

        case SDL_CONTROLLER_BUTTON_A: // Switch B = back to profile selector
            account_.unmountSave();
            screen_ = AppScreen::ProfileSelector;
            break;
    }
}

// --- Donut Editor: Edit Input ---

void UI::handleEditInput(int button) {
    int fieldCount = static_cast<int>(EditField::COUNT);

    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            if (editField_ > 0) editField_--;
            break;

        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            if (editField_ < fieldCount - 1) editField_++;
            break;

        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            adjustFieldValue(-1);
            break;

        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            adjustFieldValue(+1);
            break;

        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            adjustFieldValue(-10);
            break;

        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            adjustFieldValue(+10);
            break;

        case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
        case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
            state_ = UIState::List;
            break;
    }
}

// --- Donut Editor: Batch Input ---

void UI::handleBatchInput(int button) {
    int opCount = static_cast<int>(BatchOp::COUNT);

    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            if (batchCursor_ > 0) batchCursor_--;
            break;

        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            if (batchCursor_ < opCount - 1) batchCursor_++;
            break;

        case SDL_CONTROLLER_BUTTON_B: { // Switch A = confirm
            auto op = static_cast<BatchOp>(batchCursor_);
            uint8_t* bd = save_.donutBlockData();
            if (bd) {
                switch (op) {
                    case BatchOp::FillShiny:
                        DonutInfo::fillAllShiny(bd);
                        break;
                    case BatchOp::FillRandomLv3:
                        DonutInfo::fillAllRandomLv3(bd);
                        break;
                    case BatchOp::CloneToAll:
                        DonutInfo::cloneToAll(bd, listCursor_);
                        break;
                    case BatchOp::DeleteSelected: {
                        Donut9a d = save_.getDonut(listCursor_);
                        if (d.data) d.clear();
                        break;
                    }
                    case BatchOp::DeleteAll:
                        DonutInfo::deleteAll(bd);
                        break;
                    case BatchOp::Compress:
                        DonutInfo::compress(bd);
                        break;
                    case BatchOp::Cancel:
                        break;
                    default: break;
                }
            }
            state_ = UIState::List;
            break;
        }

        case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
            state_ = UIState::List;
            break;
    }
}

// --- Edit Helpers ---

void UI::adjustFieldValue(int direction) {
    Donut9a d = save_.getDonut(listCursor_);
    if (!d.data) return;

    auto field = static_cast<EditField>(editField_);
    switch (field) {
        case EditField::Stars: {
            int v = d.stars() + direction;
            if (v < 0) v = 5;
            if (v > 5) v = 0;
            d.setStars(static_cast<uint8_t>(v));
            break;
        }
        case EditField::LevelBoost: {
            int v = d.levelBoost() + direction;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            d.setLevelBoost(static_cast<uint8_t>(v));
            break;
        }
        case EditField::Calories: {
            int v = d.calories() + direction;
            if (v < 0) v = 0;
            if (v > 9999) v = 9999;
            d.setCalories(static_cast<uint16_t>(v));
            break;
        }
        case EditField::DonutSprite: {
            int v = d.donutSprite() + direction;
            if (v < 0) v = 202;
            if (v > 202) v = 0;
            d.setDonutSprite(static_cast<uint16_t>(v));
            break;
        }
        case EditField::BerryName: {
            int idx = cycleBerry(d.berryName(), direction);
            d.setBerryName(DonutInfo::VALID_BERRY_IDS[idx]);
            break;
        }
        case EditField::Berry1: case EditField::Berry2:
        case EditField::Berry3: case EditField::Berry4:
        case EditField::Berry5: case EditField::Berry6:
        case EditField::Berry7: case EditField::Berry8: {
            int bi = editField_ - static_cast<int>(EditField::Berry1);
            int idx = cycleBerry(d.berry(bi), direction);
            d.setBerry(bi, DonutInfo::VALID_BERRY_IDS[idx]);
            break;
        }
        case EditField::Flavor0: case EditField::Flavor1:
        case EditField::Flavor2: {
            int fi = editField_ - static_cast<int>(EditField::Flavor0);
            int idx = cycleFlavor(d.flavor(fi), direction);
            d.setFlavor(fi, DonutInfo::FLAVORS[idx].hash);
            break;
        }
        default: break;
    }
}

int UI::cycleBerry(uint16_t current, int direction) {
    int idx = DonutInfo::findValidBerryIndex(current);
    int n = DonutInfo::VALID_BERRY_COUNT;
    idx = ((idx + direction) % n + n) % n;
    return idx;
}

int UI::cycleFlavor(uint64_t current, int direction) {
    int idx = DonutInfo::findFlavorByHash(current);
    if (idx < 0) idx = 0;
    int n = DonutInfo::FLAVOR_COUNT;
    idx = ((idx + direction) % n + n) % n;
    return idx;
}

int UI::totalPages() {
    int visibleRows = (CONTENT_H - 28) / ROW_H;
    if (visibleRows <= 0) return 1;
    return (Donut9a::MAX_COUNT + visibleRows - 1) / visibleRows;
}

int UI::currentPage() {
    int visibleRows = (CONTENT_H - 28) / ROW_H;
    if (visibleRows <= 0) return 0;
    return listScroll_ / visibleRows;
}
