// file: src/display/LCD2004.h

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class LCD2004 {
public:
    enum class ScreenMode {
        StartupIntro,
        Intro,
        IntroLoop,
        StaticLogo,
        Clock
    };

    LCD2004(int busNumber = 0, int address = 0x27);
    ~LCD2004();

    bool init();
    void closeDevice();

    bool isInitialized() const;

    bool start();
    void stop();

    bool backlightOn();
    bool backlightOff();
    bool isBacklightOn() const;

    bool showGrowboxByWojtron();

    void setOutputStateProvider(std::function<bool()> provider);

    void setScreenMode(ScreenMode mode);
    void setScreenModeStartupIntro();
    void setScreenModeIntro();
    void setScreenModeIntroLoop();
    void setScreenModeStaticLogo();
    void setScreenModeClock();

    ScreenMode getScreenMode() const;
    const char* getScreenModeName() const;
    bool isIntroRunning() const;

    void setClockBottomText(const std::string& text);
    std::string getClockBottomText() const;

private:
    bool writeRaw(uint8_t value);
    bool pulse(uint8_t data);
    bool sendNibble(uint8_t nibble, uint8_t rs);
    bool sendByte(uint8_t byte, uint8_t rs);
    bool cmd(uint8_t value);
    bool data(uint8_t value);

    bool clear();
    bool home();
    bool setCursor(int row, int col);
    bool print(const std::string& text);
    bool printLine(int row, const std::string& text);
    bool printFourLines(const std::string& line1,
                        const std::string& line2,
                        const std::string& line3,
                        const std::string& line4);

    void workerLoop();
    bool renderClockScreen();
    void playIntroSequence(ScreenMode expectedMode, int finalHoldMs);
    bool sleepInterruptible(int totalMs, ScreenMode expectedMode);

    static std::string centerText(const std::string& text, std::size_t width);
    static std::string rightAlignText(const std::string& text, std::size_t width);
    static std::string fitToWidth(const std::string& text, std::size_t width);
    static const char* screenModeToString(ScreenMode mode);

private:
    static constexpr uint8_t LCD_RS = 0x01;
    static constexpr uint8_t LCD_EN = 0x04;
    static constexpr uint8_t LCD_BL = 0x08;
    static constexpr int LCD_COLS = 20;

    int busNumber_;
    int address_;
    int fd_;
    bool initialized_;
    bool backlightEnabled_;

    std::atomic<bool> running_{false};
    std::thread worker_;

    ScreenMode screenMode_ = ScreenMode::Clock;
    bool introRunning_ = false;
    long long lastClockRenderEpoch_ = -1;
    std::function<bool()> outputStateProvider_;
    std::string clockBottomText_ = "GROWBOX by wojtron";

    mutable std::mutex mtx_;
    std::condition_variable cv_;
};

// end of file: src/display/LCD2004.h
