#include "ui.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

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
            case UIState::List:     handleListInput(button, running); break;
            case UIState::Edit:     handleEditInput(button); break;
            case UIState::Batch:    handleBatchInput(button); break;
            case UIState::Import:   handleImportInput(button); break;
            case UIState::ExitMenu: handleExitMenuInput(button, running); break;
        }
    }

    // Handle button repeat
    if (repeatDir_ != 0 && save_.hasDonutBlock()) {
        if (handleRepeat(repeatDir_)) {
            bool dummy = true;
            switch (state_) {
                case UIState::List:     handleListInput(repeatDir_, dummy); break;
                case UIState::Edit:     handleEditInput(repeatDir_); break;
                case UIState::Batch:    handleBatchInput(repeatDir_); break;
                case UIState::Import:   handleImportInput(repeatDir_); break;
                case UIState::ExitMenu: handleExitMenuInput(repeatDir_, dummy); break;
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
                case UIState::Import:
                    if (stickDirY_ < 0)
                        handleImportInput(SDL_CONTROLLER_BUTTON_DPAD_UP);
                    else if (stickDirY_ > 0)
                        handleImportInput(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                    break;
                case UIState::ExitMenu: {
                    bool dm = true;
                    if (stickDirY_ < 0)
                        handleExitMenuInput(SDL_CONTROLLER_BUTTON_DPAD_UP, dm);
                    else if (stickDirY_ > 0)
                        handleExitMenuInput(SDL_CONTROLLER_BUTTON_DPAD_DOWN, dm);
                    break;
                }
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
                DonutInfo::recalcStats(d);
                d.setFlavor(0, 0xD373B22CEF7A33C9ULL); // Sparkling Power: All Types (Lv. 3)
                d.setFlavor(1, 0xCCFCB99681D31E8BULL); // Alpha Power (Lv. 3)
                d.setFlavor(2, 0);
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

        case SDL_CONTROLLER_BUTTON_START: // + = exit menu
            exitCursor_ = 0;
            state_ = UIState::ExitMenu;
            break;

        case SDL_CONTROLLER_BUTTON_BACK: // - = about
            showAbout_ = true;
            break;

        case SDL_CONTROLLER_BUTTON_A: // Switch B = back to profile selector
            if (showConfirm("Go Back?", "Unsaved changes will be lost.")) {
                account_.unmountSave();
                screen_ = AppScreen::ProfileSelector;
            }
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
            batchCursor_ = (batchCursor_ + opCount - 1) % opCount;
            break;

        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            batchCursor_ = (batchCursor_ + 1) % opCount;
            break;

        case SDL_CONTROLLER_BUTTON_B: { // Switch A = confirm
            auto op = static_cast<BatchOp>(batchCursor_);
            uint8_t* bd = save_.donutBlockData();
            if (bd) {
                switch (op) {
                    case BatchOp::OneShiny: {
                        Donut9a d = save_.getDonut(listCursor_);
                        if (d.data) DonutInfo::fillOneShiny(d);
                        break;
                    }
                    case BatchOp::OneShinyRandom: {
                        Donut9a d = save_.getDonut(listCursor_);
                        if (d.data) DonutInfo::fillOneShinyRandom(d);
                        break;
                    }
                    case BatchOp::OneRandomLv3: {
                        Donut9a d = save_.getDonut(listCursor_);
                        if (d.data) DonutInfo::fillOneRandomLv3(d);
                        break;
                    }
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
                    case BatchOp::ExportDonut:
                        exportDonut(listCursor_);
                        break;
                    case BatchOp::ImportDonut:
                        scanDonutFiles();
                        if (importFiles_.empty()) {
                            showMessageAndWait("No Files", "No .donut files found in donuts/ folder.");
                        } else {
                            importCursor_ = 0;
                            importScroll_ = 0;
                            state_ = UIState::Import;
                            return;
                        }
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
        case EditField::Berry1: case EditField::Berry2:
        case EditField::Berry3: case EditField::Berry4:
        case EditField::Berry5: case EditField::Berry6:
        case EditField::Berry7: case EditField::Berry8: {
            int bi = editField_ - static_cast<int>(EditField::Berry1);
            int idx = cycleBerry(d.berry(bi), direction);
            d.setBerry(bi, DonutInfo::VALID_BERRY_IDS[idx]);
            // Auto-recalculate derived stats from berries
            DonutInfo::recalcStats(d);
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

// --- Import Input ---

void UI::handleImportInput(int button) {
    int fileCount = static_cast<int>(importFiles_.size());
    if (fileCount == 0) {
        if (button == SDL_CONTROLLER_BUTTON_A || button == SDL_CONTROLLER_BUTTON_B)
            state_ = UIState::List;
        return;
    }

    int mh = 450;
    int listH = mh - 70;
    int visibleRows = listH / 28;

    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            if (importCursor_ > 0) {
                importCursor_--;
                if (importCursor_ < importScroll_)
                    importScroll_ = importCursor_;
            }
            break;

        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            if (importCursor_ < fileCount - 1) {
                importCursor_++;
                if (importCursor_ >= importScroll_ + visibleRows)
                    importScroll_ = importCursor_ - visibleRows + 1;
            }
            break;

        case SDL_CONTROLLER_BUTTON_B: // Switch A = confirm
            if (importCursor_ >= 0 && importCursor_ < fileCount) {
                importDonut(importFiles_[importCursor_]);
            }
            state_ = UIState::List;
            break;

        case SDL_CONTROLLER_BUTTON_Y: { // Switch X = delete file
            if (importCursor_ >= 0 && importCursor_ < fileCount) {
                std::string display = importFiles_[importCursor_];
                if (display.size() > 6)
                    display = display.substr(0, display.size() - 6);
                if (showConfirm("Delete File?", "This will permanently delete:", display)) {
                    std::string path = basePath_ + "donuts/" + importFiles_[importCursor_];
                    std::remove(path.c_str());
                    scanDonutFiles();
                    if (importFiles_.empty()) {
                        showMessageAndWait("No Files", "No .donut files found in donuts/ folder.");
                        state_ = UIState::List;
                    } else if (importCursor_ >= static_cast<int>(importFiles_.size())) {
                        importCursor_ = static_cast<int>(importFiles_.size()) - 1;
                    }
                }
            }
            break;
        }

        case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
            state_ = UIState::List;
            break;
    }
}

// --- Exit Menu Input ---

void UI::handleExitMenuInput(int button, bool& running) {
    int opCount = static_cast<int>(ExitOp::COUNT);

    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            exitCursor_ = (exitCursor_ + opCount - 1) % opCount;
            break;

        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            exitCursor_ = (exitCursor_ + 1) % opCount;
            break;

        case SDL_CONTROLLER_BUTTON_B: { // Switch A = confirm
            auto op = static_cast<ExitOp>(exitCursor_);
            if (op == ExitOp::SaveAndQuit) {
                saveNow_ = true;
                running = false;
            } else if (op == ExitOp::SaveAndBack) {
                showWorking("Saving...");
                if (save_.isLoaded())
                    save_.save(savePath_);
                account_.commitSave();
                account_.unmountSave();
                screen_ = AppScreen::ProfileSelector;
            } else if (op == ExitOp::QuitWithoutSaving) {
                running = false;
            }
            state_ = UIState::List;
            break;
        }

        case SDL_CONTROLLER_BUTTON_A: // Switch B = cancel
            state_ = UIState::List;
            break;
    }
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
