#include "ui.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>

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

    // Draw colored background image (flavorprofile.png is 628x541)
    auto it = spriteCache_.find("flavorprofile");
    if (it == spriteCache_.end()) {
#ifdef __SWITCH__
        SDL_Surface* surf = IMG_Load("romfs:/sprites/flavorprofile.png");
#else
        SDL_Surface* surf = IMG_Load("romfs/sprites/flavorprofile.png");
#endif
        SDL_Texture* tex = surf ? SDL_CreateTextureFromSurface(renderer_, surf) : nullptr;
        if (surf) SDL_FreeSurface(surf);
        spriteCache_["flavorprofile"] = tex;
        it = spriteCache_.find("flavorprofile");
    }
    if (it->second) {
        // PKHeX renders the 628x541 image in a 276x240 control with pentagon radius 55.
        // In the original image, the dark pentagon has radius ~125px (55 * 628/276).
        // Scale so the dark pentagon matches our 'radius' parameter.
        int imgW = radius * 628 / 125;
        int imgH = radius * 541 / 125;
        SDL_Rect dst = {cx - imgW / 2, cy - imgH / 2, imgW, imgH};
        SDL_RenderCopy(renderer_, it->second, nullptr, &dst);
    }

    // Precompute pentagon vertex positions (unit circle)
    double vx[5], vy[5];
    for (int i = 0; i < 5; i++) {
        double angle = (-PI / 2.0) + i * ANGLE_STEP;
        vx[i] = std::cos(angle);
        vy[i] = std::sin(angle);
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
            msg = "DPad:Move  L/R:Page  A:Edit  X:Delete  Y:Batch  +:Exit  -:About  B:Back";
            break;
        case UIState::Edit:
            msg = "DPad U/D:Field  L/R:Value  L1/R1:x10  A:Confirm  B:Cancel";
            break;
        case UIState::Batch:
            msg = "DPad U/D:Select  A:Confirm  B:Cancel";
            break;
        case UIState::Import:
            msg = "DPad U/D:Select  A:Import  X:Delete  B:Cancel";
            break;
        case UIState::ExitMenu:
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
            SDL_Color emptyCol = selected ? COL_TEXT_DIM : COL_EMPTY;
            char ibuf[8];
            std::snprintf(ibuf, sizeof(ibuf), "%3d", idx + 1);
            drawText(ibuf, LIST_X + 18, ry + 6, emptyCol, fontSmall_);
            drawText("(empty)", LIST_X + 83, ry + 6, emptyCol, fontSmall_);
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
    std::snprintf(buf, sizeof(buf), "%s", DonutInfo::getBerryName(d.berryName()));
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
    int radarCX = px + pw - 190;
    int radarCY = CONTENT_Y + CONTENT_H / 2 + 30;
    drawFlavorRadar(radarCX, radarCY, 70, flavorVals);
}

// --- Donut Editor: Edit Panel ---

static const char* EDIT_LABELS[] = {
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
    y += 32;

    // Auto-calculated stats (read-only)
    std::snprintf(buf, sizeof(buf), "%s  Cal:%d  Boost:+%d",
                  DonutInfo::starsString(d.stars()).c_str(), d.calories(), d.levelBoost());
    drawText(buf, px + 20, y, COL_TEXT_DIM, fontSmall_);
    y += 24;

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
            case EditField::Berry1: case EditField::Berry2:
            case EditField::Berry3: case EditField::Berry4:
            case EditField::Berry5: case EditField::Berry6:
            case EditField::Berry7: case EditField::Berry8: {
                int bi = f - static_cast<int>(EditField::Berry1);
                uint16_t bv = d.berry(bi);
                val = DonutInfo::getBerryName(bv);
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
    "Set: Shiny Power",
    "Set: Shiny Power (Random)",
    "Set: Random Lv3",
    "Fill All: Shiny Power",
    "Fill All: Random Lv3",
    "Clone Selected to All",
    "Delete Selected Donut",
    "Delete ALL Donuts",
    "Compress (remove gaps)",
    "Export Donut to File",
    "Import Donut from File",
    "Cancel",
};

void UI::drawBatchMenu() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 140});

    int mw = 380, mh = 462;
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

// --- Donut Editor: Import Panel ---

void UI::drawImportPanel() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 140});

    int mw = 500, mh = 450;
    int mx = (SCREEN_W - mw) / 2;
    int my = (SCREEN_H - mh) / 2;

    drawRect(mx, my, mw, mh, COL_BATCH_BG);
    drawRectOutline(mx, my, mw, mh, COL_CURSOR, 2);

    drawText("Import Donut", mx + 20, my + 14, COL_CURSOR, fontLarge_);

    int listY = my + 55;
    int listH = mh - 70;
    int visibleRows = listH / 28;
    int fileCount = static_cast<int>(importFiles_.size());

    for (int row = 0; row < visibleRows; row++) {
        int idx = importScroll_ + row;
        if (idx >= fileCount) break;

        int oy = listY + row * 28;
        bool sel = (idx == importCursor_);

        if (sel)
            drawRect(mx + 10, oy - 2, mw - 20, 26, COL_EDIT_FIELD);

        if (sel)
            drawText(">", mx + 14, oy + 2, COL_CURSOR, font_);

        // Show filename without .donut extension
        std::string display = importFiles_[idx];
        if (display.size() > 6)
            display = display.substr(0, display.size() - 6);
        drawText(display, mx + 35, oy + 2, sel ? COL_TEXT : COL_TEXT_DIM, font_);
    }

    // Scroll indicators
    if (importScroll_ > 0)
        drawText("\xe2\x96\xb2", mx + mw - 25, listY, COL_ACCENT, fontSmall_);
    if (importScroll_ + visibleRows < fileCount)
        drawText("\xe2\x96\xbc", mx + mw - 25, listY + listH - 18, COL_ACCENT, fontSmall_);
}

// --- Exit Menu ---

static const char* EXIT_LABELS[] = {
    "Save & Quit",
    "Save & Back to Profiles",
    "Quit without Saving",
    "Cancel",
};

void UI::drawExitMenu() {
    drawRect(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 140});

    int mw = 380, mh = 215;
    int mx = (SCREEN_W - mw) / 2;
    int my = (SCREEN_H - mh) / 2;

    drawRect(mx, my, mw, mh, COL_BATCH_BG);
    drawRectOutline(mx, my, mw, mh, COL_CURSOR, 2);

    drawText("Exit", mx + 20, my + 14, COL_CURSOR, fontLarge_);

    int opCount = static_cast<int>(ExitOp::COUNT);
    for (int i = 0; i < opCount; i++) {
        int oy = my + 55 + i * 34;
        bool sel = (i == exitCursor_);

        if (sel)
            drawRect(mx + 10, oy - 2, mw - 20, 30, COL_EDIT_FIELD);
        if (sel)
            drawText(">", mx + 14, oy + 2, COL_CURSOR, font_);
        drawText(EXIT_LABELS[i], mx + 35, oy + 2, sel ? COL_TEXT : COL_TEXT_DIM, font_);
    }
}
