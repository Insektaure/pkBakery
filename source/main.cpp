#include "ui.h"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <string>

int main(int argc, char* argv[]) {
#ifdef __SWITCH__
    romfsInit();
#endif

    // Determine base path
    std::string basePath;
#ifdef __SWITCH__
    if (argc > 0 && argv[0]) {
        basePath = argv[0];
        auto lastSlash = basePath.rfind('/');
        if (lastSlash != std::string::npos)
            basePath = basePath.substr(0, lastSlash + 1);
        else
            basePath = "sdmc:/switch/pkBakery/";
    } else {
        basePath = "sdmc:/switch/pkBakery/";
    }
#else
    basePath = "./";
#endif

    std::string savePath = basePath + "main";

    // Initialize UI first so we can show errors
    UI ui;
    if (!ui.init()) {
#ifdef __SWITCH__
        romfsExit();
#endif
        return 1;
    }

    // Show splash screen while loading
    ui.showSplash();

    // Check for applet mode on Switch - require title takeover
#ifdef __SWITCH__
    AppletType at = appletGetAppletType();
    if (at != AppletType_Application && at != AppletType_SystemApplication) {
        ui.showMessageAndWait("Applet Mode Not Supported",
                              "Please run this app in title takeover mode.");
        ui.shutdown();
        romfsExit();
        return 1;
    }
#endif

    // Run main loop - profile selection, game selection, and donut editing all handled inside
    ui.run(basePath, savePath);

    // Cleanup
    ui.shutdown();

#ifdef __SWITCH__
    romfsExit();
#endif
    return 0;
}
