#include "ui.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>

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

void UI::showMessageAndWait(const std::string& title, const std::string& body, const std::string& body2) {
    if (!renderer_) return;

    bool hasBody2 = !body2.empty();
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

        drawCurrentFrame();

        constexpr int POP_W = 480;
        int popH = hasBody2 ? 200 : 180;
        int px = (SCREEN_W - POP_W) / 2;
        int py = (SCREEN_H - popH) / 2;

        drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 140});
        drawRect(px, py, POP_W, popH, COL_BATCH_BG);
        drawRectOutline(px, py, POP_W, popH, COL_CURSOR, 2);

        drawTextCentered(title, SCREEN_W / 2, py + 35, COL_CURSOR, fontLarge_);
        if (hasBody2) {
            drawTextCentered(body, SCREEN_W / 2, py + 75, COL_TEXT, font_);
            drawTextCentered(body2, SCREEN_W / 2, py + 100, COL_EDIT_VAL, font_);
        } else {
            drawTextCentered(body, SCREEN_W / 2, py + 85, COL_TEXT, font_);
        }
        drawTextCentered("Press B to dismiss", SCREEN_W / 2, py + popH - 30, COL_TEXT_DIM, fontSmall_);

        SDL_RenderPresent(renderer_);
        SDL_Delay(16);
    }
}

bool UI::showConfirm(const std::string& title, const std::string& body, const std::string& body2) {
    if (!renderer_) return false;

    bool hasBody2 = !body2.empty();
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

        drawCurrentFrame();

        constexpr int POP_W = 480;
        int popH = hasBody2 ? 200 : 180;
        int px = (SCREEN_W - POP_W) / 2;
        int py = (SCREEN_H - popH) / 2;

        drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 140});
        drawRect(px, py, POP_W, popH, COL_BATCH_BG);
        drawRectOutline(px, py, POP_W, popH, COL_CURSOR, 2);

        drawTextCentered(title, SCREEN_W / 2, py + 35, COL_CURSOR, fontLarge_);
        if (hasBody2) {
            drawTextCentered(body, SCREEN_W / 2, py + 75, COL_TEXT, font_);
            drawTextCentered(body2, SCREEN_W / 2, py + 100, COL_EDIT_VAL, font_);
        } else {
            drawTextCentered(body, SCREEN_W / 2, py + 85, COL_TEXT, font_);
        }
        drawTextCentered("A: Confirm    B: Cancel", SCREEN_W / 2, py + popH - 30, COL_TEXT_DIM, fontSmall_);

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

// --- Draw Current Frame (for popup backgrounds) ---

void UI::drawCurrentFrame() {
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
        } else {
            drawText("No donut data found in save file.", DETAIL_X + 20, SCREEN_H / 2, COL_TEXT, font_);
            drawText("Make sure you have a Pokemon Legends Z-A save.", DETAIL_X + 20, SCREEN_H / 2 + 30, COL_TEXT_DIM, fontSmall_);
        }
        drawDonutStatusBar();
    }
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
                if (state_ == UIState::Import) drawImportPanel();
                if (state_ == UIState::ExitMenu) drawExitMenu();
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
                if (state_ == UIState::Import) drawImportPanel();
                if (state_ == UIState::ExitMenu) drawExitMenu();
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
