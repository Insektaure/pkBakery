#pragma once
#include "save_file.h"
#include "donut.h"
#include "account.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <vector>
#include <unordered_map>

// App-level screen state
enum class AppScreen { ProfileSelector, MainView };

// Donut editor sub-states
enum class UIState { List, Edit, Batch, Import, ExitMenu };

enum class EditField {
    Berry1, Berry2, Berry3, Berry4, Berry5, Berry6, Berry7, Berry8,
    Flavor0, Flavor1, Flavor2,
    COUNT
};

enum class BatchOp {
    OneShiny, OneShinyRandom, OneRandomLv3,
    FillShiny, FillRandomLv3, CloneToAll, DeleteSelected,
    DeleteAll, Compress, ExportDonut, ImportDonut, Cancel,
    COUNT
};

enum class ExitOp { SaveAndQuit, SaveAndBack, QuitWithoutSaving, Cancel, COUNT };

class UI {
public:
    bool init();
    void shutdown();
    void showSplash();
    void showMessageAndWait(const std::string& title, const std::string& body, const std::string& body2 = "");
    bool showConfirm(const std::string& title, const std::string& body, const std::string& body2 = "");
    void showWorking(const std::string& msg);
    void run(const std::string& basePath, const std::string& savePath);

private:
    SDL_Window*          window_    = nullptr;
    SDL_Renderer*        renderer_  = nullptr;
    SDL_GameController*  pad_       = nullptr;
    TTF_Font*            font_      = nullptr;
    TTF_Font*            fontSmall_ = nullptr;
    TTF_Font*            fontLarge_ = nullptr;

    // Screen dimensions (Switch: 1280x720)
    static constexpr int SCREEN_W = 1280;
    static constexpr int SCREEN_H = 720;

    // Donut editor layout
    static constexpr int HEADER_H    = 44;
    static constexpr int STATUS_H    = 35;
    static constexpr int LIST_X      = 15;
    static constexpr int LIST_W      = 430;
    static constexpr int DETAIL_X    = 460;
    static constexpr int DETAIL_W    = 805;
    static constexpr int CONTENT_Y   = HEADER_H + 4;
    static constexpr int CONTENT_H   = SCREEN_H - HEADER_H - STATUS_H - 8;
    static constexpr int ROW_H       = 32;

    // Colors - warm bakery theme
    static constexpr SDL_Color COLOR_RED         = {220, 60, 60, 255};
    static constexpr SDL_Color COL_BG         = {35, 25, 20, 255};
    static constexpr SDL_Color COL_PANEL      = {50, 40, 35, 255};
    static constexpr SDL_Color COL_HEADER     = {60, 45, 35, 255};
    static constexpr SDL_Color COL_ROW_SEL    = {90, 65, 40, 255};
    static constexpr SDL_Color COL_ROW_HOVER  = {65, 50, 40, 255};
    static constexpr SDL_Color COL_CURSOR     = {220, 170, 50, 255};
    static constexpr SDL_Color COL_TEXT       = {240, 230, 220, 255};
    static constexpr SDL_Color COL_TEXT_DIM   = {160, 145, 130, 255};
    static constexpr SDL_Color COL_STARS      = {255, 200, 50, 255};
    static constexpr SDL_Color COL_ACCENT     = {200, 120, 50, 255};
    static constexpr SDL_Color COL_EMPTY      = {80, 65, 55, 255};
    static constexpr SDL_Color COL_STATUS_BG  = {40, 30, 25, 255};
    static constexpr SDL_Color COL_STATUS     = {180, 160, 140, 255};
    static constexpr SDL_Color COL_BATCH_BG   = {45, 35, 30, 230};
    static constexpr SDL_Color COL_EDIT_FIELD = {70, 55, 45, 255};
    static constexpr SDL_Color COL_EDIT_VAL   = {255, 220, 150, 255};

    // Joystick navigation
    static constexpr int16_t STICK_DEADZONE = 16000;
    static constexpr uint32_t STICK_INITIAL_DELAY = 400;
    static constexpr uint32_t STICK_REPEAT_DELAY  = 200;
    int stickDirX_ = 0;
    int stickDirY_ = 0;
    uint32_t stickMoveTime_ = 0;
    bool stickMoved_ = false;
    void updateStick(int16_t axisX, int16_t axisY);

    // App screen state
    AppScreen screen_ = AppScreen::ProfileSelector;
    std::string basePath_;
    std::string savePath_;

    // Account manager
    AccountManager account_;

    // Profile selector state
    int profileSelCursor_ = 0;
    int selectedProfile_ = -1;

    // Save file
    SaveFile save_;
    bool saveNow_ = false;
    bool showAbout_ = false;

    // Donut editor state
    UIState state_ = UIState::List;
    int listCursor_  = 0;
    int listScroll_  = 0;
    int editField_   = 0;
    int batchCursor_ = 0;

    // Import file picker state
    std::vector<std::string> importFiles_;
    int importCursor_ = 0;
    int importScroll_ = 0;

    // Exit menu state
    int exitCursor_ = 0;

    // Input repeat (donut editor)
    uint32_t repeatDir_   = 0;
    uint32_t repeatStart_ = 0;
    uint32_t repeatLast_  = 0;
    static constexpr uint32_t REPEAT_DELAY = 400;
    static constexpr uint32_t REPEAT_RATE  = 80;

    // Profile selector
    void drawProfileSelectorFrame();
    void handleProfileSelectorInput(bool& running);
    void selectProfile(int index);
    std::string buildBackupDir() const;

    // About popup
    void drawAboutPopup();

    // Sprite cache
    std::unordered_map<std::string, SDL_Texture*> spriteCache_;
    SDL_Texture* getDonutSprite(uint16_t spriteId, uint8_t stars);
    void freeSprites();

    // Redraws the current screen (used as background for popup overlays)
    void drawCurrentFrame();

    // Rendering helpers
    void drawTextCentered(const std::string& text, int cx, int cy, SDL_Color color, TTF_Font* f);
    void drawStatusBar(const std::string& msg);
    void drawRect(int x, int y, int w, int h, SDL_Color c);
    void drawRectOutline(int x, int y, int w, int h, SDL_Color c, int t = 2);
    void drawText(const std::string& text, int x, int y, SDL_Color c, TTF_Font* f = nullptr);
    void drawTextRight(const std::string& text, int x, int y, SDL_Color c, TTF_Font* f = nullptr);

    // Flavor radar chart
    void drawFlavorRadar(int cx, int cy, int radius, const int flavors[5]);

    // Donut editor views
    void drawDonutHeader();
    void drawDonutStatusBar();
    void drawListPanel();
    void drawDetailPanel();
    void drawEditPanel();
    void drawBatchMenu();
    void drawImportPanel();
    void drawExitMenu();

    // Donut editor input
    void handleDonutInput(bool& running);
    void handleListInput(int button, bool& running);
    void handleEditInput(int button);
    void handleBatchInput(int button);
    void handleImportInput(int button);
    void handleExitMenuInput(int button, bool& running);
    bool handleRepeat(uint32_t button);
    void clearRepeat();

    // Edit helpers
    void adjustFieldValue(int direction);
    int cycleBerry(uint16_t current, int direction);
    int cycleFlavor(uint64_t current, int direction);

    // Export / Import
    bool exportDonut(int index);
    bool importDonut(const std::string& filename);
    void scanDonutFiles();
    std::string showKeyboard(const std::string& defaultText);
    std::string sanitizeFilename(const std::string& input);
    std::string buildDefaultExportName(int index);

    // Utility
    int totalPages();
    int currentPage();
};
