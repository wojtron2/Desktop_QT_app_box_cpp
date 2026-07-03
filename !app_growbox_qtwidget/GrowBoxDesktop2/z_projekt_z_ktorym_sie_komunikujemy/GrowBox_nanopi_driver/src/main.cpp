// file: src/main.cpp

#include "httpserver.h"
#include "scheduler.h"
#include "soundmanager.h"
#include "mcp23017driver.h"
#include "display/LCD2004.h"

#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <system_error>

namespace fs = std::filesystem;

int main() {

    const std::string bind_address = "0.0.0.0";
    const int port = 8080;

    char exePath[1024];

    ssize_t len = readlink("/proc/self/exe",
                           exePath,
                           sizeof(exePath) - 1);

    if (len == -1) {
        std::cerr << "Cannot determine executable path\n";
        return 1;
    }

    exePath[len] = '\0';

    fs::path exeFullPath(exePath);
    fs::path exeDir = exeFullPath.parent_path();
    fs::path projectDir = exeDir.parent_path();
    fs::path wwwPath = projectDir / "www";

    fs::path configDir = projectDir / "config";
    fs::path stateFile = configDir / "scheduler_state.json";

    std::cout << "Project dir: " << projectDir << "\n";
    std::cout << "WWW dir:     " << wwwPath << "\n";
    std::cout << "Config dir:  " << configDir << "\n";
    std::cout << "State file:  " << stateFile << "\n";

    if (!fs::exists(wwwPath)) {
        std::cerr << "WWW directory does not exist!\n";
        return 1;
    }

    std::error_code ec;
    fs::create_directories(configDir, ec);
    if (ec) {
        std::cerr << "Cannot create config directory: "
                  << configDir << " error=" << ec.message() << "\n";
        return 1;
    }

    SoundManager soundManager;
    soundManager.start();

    LCD2004 lcd(0, 0x27);
    if (!lcd.init()) {
        std::cerr << "[MAIN] LCD2004 init failed\n";
    }
    else {
        lcd.backlightOn();

        if (!lcd.start()) {
            std::cerr << "[MAIN] LCD2004 worker start failed\n";
        }
        else {
            lcd.setScreenModeStartupIntro();
        }
    }

    MCP23017Driver mcp23017(0, 0x20);
    if (!mcp23017.init()) {
        std::cerr << "[MAIN] MCP23017 init failed\n";
    }

    Scheduler scheduler(stateFile.string());
    scheduler.setSoundManager(&soundManager);
    scheduler.setMcp23017Driver(&mcp23017);
    scheduler.setLcd(&lcd);
    scheduler.loadStateFromDisk();
    scheduler.start();

    lcd.setOutputStateProvider([&scheduler]() {
        return scheduler.isAnyOutputOn();
    });

    HttpServer server(bind_address,
                      port,
                      wwwPath.string(),
                      scheduler,
                      lcd);

    std::cout << "Server starting on "
              << bind_address << ":" << port << "\n";

    try {
        server.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }

    scheduler.stop();
    lcd.stop();
    soundManager.stop();

    return 0;
}

// end of file: src/main.cpp
