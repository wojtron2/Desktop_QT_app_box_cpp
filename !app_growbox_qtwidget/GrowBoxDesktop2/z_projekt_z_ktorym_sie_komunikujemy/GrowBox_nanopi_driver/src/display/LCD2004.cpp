// file: src/display/LCD2004.cpp

#include "display/LCD2004.h"

#include <chrono>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <linux/i2c-dev.h>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

LCD2004::LCD2004(int busNumber, int address)
    : busNumber_(busNumber),
      address_(address),
      fd_(-1),
      initialized_(false),
      backlightEnabled_(true)
{
}

LCD2004::~LCD2004()
{
    stop();
    closeDevice();
}

bool LCD2004::init()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (initialized_) {
        return true;
    }

    std::string devPath = "/dev/i2c-" + std::to_string(busNumber_);

    fd_ = open(devPath.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "[LCD2004] open failed: " << devPath << "\n";
        return false;
    }

    if (ioctl(fd_, I2C_SLAVE, address_) < 0) {
        std::cerr << "[LCD2004] ioctl I2C_SLAVE failed, addr=0x"
                  << std::hex << address_ << std::dec << "\n";
        close(fd_);
        fd_ = -1;
        return false;
    }

    backlightEnabled_ = true;

    if (!writeRaw(0x08)) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (!writeRaw(0x3C)) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    if (!writeRaw(0x38)) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    if (!writeRaw(0x3C)) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    if (!writeRaw(0x38)) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    if (!writeRaw(0x3C)) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    if (!writeRaw(0x38)) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    if (!writeRaw(0x2C)) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    if (!writeRaw(0x28)) {
        close(fd_);
        fd_ = -1;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    if (!cmd(0x28)) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    if (!cmd(0x0C)) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    if (!cmd(0x06)) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    if (!cmd(0x01)) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    initialized_ = true;
    lastClockRenderEpoch_ = -1;

    std::cout << "[LCD2004] init OK, bus=" << busNumber_
              << " addr=0x" << std::hex << address_ << std::dec << "\n";

    return true;
}

void LCD2004::closeDevice()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }

    initialized_ = false;
}

bool LCD2004::isInitialized() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return initialized_;
}

bool LCD2004::start()
{
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_) {
            running_ = false;
            return false;
        }

        lastClockRenderEpoch_ = -1;
    }

    worker_ = std::thread(&LCD2004::workerLoop, this);
    return true;
}

void LCD2004::stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }

    cv_.notify_all();

    if (worker_.joinable()) {
        worker_.join();
    }
}

bool LCD2004::backlightOn()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    backlightEnabled_ = true;
    return writeRaw(LCD_BL);
}

bool LCD2004::backlightOff()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    backlightEnabled_ = false;
    return writeRaw(0x00);
}

bool LCD2004::isBacklightOn() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return backlightEnabled_;
}

bool LCD2004::showGrowboxByWojtron()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    const std::string line2 = centerText("GROWBOX", LCD_COLS);
    const std::string line3 = centerText("by wojtron", LCD_COLS);

    return printFourLines("", line2, line3, "");
}

void LCD2004::setOutputStateProvider(std::function<bool()> provider)
{
    std::lock_guard<std::mutex> lock(mtx_);
    outputStateProvider_ = std::move(provider);
}

void LCD2004::setScreenMode(ScreenMode mode)
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        screenMode_ = mode;
        lastClockRenderEpoch_ = -1;
    }

    cv_.notify_all();
}

void LCD2004::setScreenModeStartupIntro()
{
    setScreenMode(ScreenMode::StartupIntro);
}

void LCD2004::setScreenModeIntro()
{
    setScreenMode(ScreenMode::Intro);
}

void LCD2004::setScreenModeIntroLoop()
{
    setScreenMode(ScreenMode::IntroLoop);
}

void LCD2004::setScreenModeStaticLogo()
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        screenMode_ = ScreenMode::StaticLogo;
        lastClockRenderEpoch_ = -1;

        if (initialized_) {
            printFourLines("",
                           centerText("GROWBOX", LCD_COLS),
                           centerText("by wojtron", LCD_COLS),
                           "");
        }
    }

    cv_.notify_all();
}

void LCD2004::setScreenModeClock()
{
    setScreenMode(ScreenMode::Clock);
}

LCD2004::ScreenMode LCD2004::getScreenMode() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return screenMode_;
}

const char* LCD2004::getScreenModeName() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return screenModeToString(screenMode_);
}

bool LCD2004::isIntroRunning() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return introRunning_;
}

void LCD2004::setClockBottomText(const std::string& text)
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        clockBottomText_ = text;
        lastClockRenderEpoch_ = -1;
    }

    cv_.notify_all();
}

std::string LCD2004::getClockBottomText() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return clockBottomText_;
}

bool LCD2004::writeRaw(uint8_t value)
{
    if (fd_ < 0) {
        return false;
    }

    ssize_t written = write(fd_, &value, 1);
    if (written != 1) {
        std::cerr << "[LCD2004] writeRaw failed value=0x"
                  << std::hex << static_cast<int>(value) << std::dec << "\n";
        return false;
    }

    return true;
}

bool LCD2004::pulse(uint8_t data)
{
    if (!writeRaw(static_cast<uint8_t>(data | LCD_EN))) {
        return false;
    }

    if (!writeRaw(static_cast<uint8_t>(data & ~LCD_EN))) {
        return false;
    }

    return true;
}

bool LCD2004::sendNibble(uint8_t nibble, uint8_t rs)
{
    uint8_t dataByte = static_cast<uint8_t>(((nibble & 0x0F) << 4) | rs);

    if (backlightEnabled_) {
        dataByte = static_cast<uint8_t>(dataByte | LCD_BL);
    }

    return pulse(dataByte);
}

bool LCD2004::sendByte(uint8_t byte, uint8_t rs)
{
    if (!sendNibble(static_cast<uint8_t>((byte >> 4) & 0x0F), rs)) {
        return false;
    }

    if (!sendNibble(static_cast<uint8_t>(byte & 0x0F), rs)) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return true;
}

bool LCD2004::cmd(uint8_t value)
{
    return sendByte(value, 0x00);
}

bool LCD2004::data(uint8_t value)
{
    return sendByte(value, LCD_RS);
}

bool LCD2004::clear()
{
    if (!cmd(0x01)) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
}

bool LCD2004::home()
{
    if (!cmd(0x02)) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
}

bool LCD2004::setCursor(int row, int col)
{
    uint8_t addr = 0x80;

    switch (row) {
    case 0:
        addr = static_cast<uint8_t>(0x80 + col);
        break;
    case 1:
        addr = static_cast<uint8_t>(0xC0 + col);
        break;
    case 2:
        addr = static_cast<uint8_t>(0x94 + col);
        break;
    case 3:
        addr = static_cast<uint8_t>(0xD4 + col);
        break;
    default:
        addr = static_cast<uint8_t>(0x80 + col);
        break;
    }

    return cmd(addr);
}

bool LCD2004::print(const std::string& text)
{
    for (unsigned char ch : text) {
        if (!data(static_cast<uint8_t>(ch))) {
            return false;
        }
    }

    return true;
}

bool LCD2004::printLine(int row, const std::string& text)
{
    if (!setCursor(row, 0)) {
        return false;
    }

    return print(fitToWidth(text, LCD_COLS));
}

bool LCD2004::printFourLines(const std::string& line1,
                             const std::string& line2,
                             const std::string& line3,
                             const std::string& line4)
{
    if (!printLine(0, line1)) {
        return false;
    }

    if (!printLine(1, line2)) {
        return false;
    }

    if (!printLine(2, line3)) {
        return false;
    }

    if (!printLine(3, line4)) {
        return false;
    }

    return true;
}

void LCD2004::workerLoop()
{
    while (running_.load()) {
        ScreenMode mode;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            mode = screenMode_;
        }

        if (mode == ScreenMode::StartupIntro ||
            mode == ScreenMode::Intro ||
            mode == ScreenMode::IntroLoop) {
            {
                std::lock_guard<std::mutex> lock(mtx_);
                introRunning_ = true;
            }

            const int finalHoldMs = (mode == ScreenMode::StartupIntro) ? 5000 : 1200;
            playIntroSequence(mode, finalHoldMs);

            bool switchToClock = false;
            bool switchToStaticLogo = false;

            {
                std::lock_guard<std::mutex> lock(mtx_);
                introRunning_ = false;

                if (screenMode_ == ScreenMode::StartupIntro) {
                    switchToClock = true;
                }
                else if (screenMode_ == ScreenMode::Intro) {
                    switchToStaticLogo = true;
                }
            }

            if (switchToClock) {
                setScreenModeClock();
            }

            if (switchToStaticLogo) {
                setScreenModeStaticLogo();
            }

            if (mode == ScreenMode::IntroLoop) {
                if (!sleepInterruptible(5000, ScreenMode::IntroLoop)) {
                    continue;
                }
            }

            continue;
        }

        if (mode == ScreenMode::StaticLogo) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait_for(lock, std::chrono::milliseconds(200));
            continue;
        }

        renderClockScreen();

        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, std::chrono::milliseconds(200));
    }
}

bool LCD2004::renderClockScreen()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    const long long epoch = static_cast<long long>(t);

    if (lastClockRenderEpoch_ == epoch) {
        return true;
    }

    lastClockRenderEpoch_ = epoch;

    bool anyOutputOn = false;
    if (outputStateProvider_) {
        anyOutputOn = outputStateProvider_();
    }

    std::tm local = *std::localtime(&t);

    std::ostringstream timeSs;
    timeSs << (local.tm_hour < 10 ? "0" : "") << local.tm_hour
           << ":"
           << (local.tm_min < 10 ? "0" : "") << local.tm_min
           << ":"
           << (local.tm_sec < 10 ? "0" : "") << local.tm_sec;

    const std::string line1 = rightAlignText(anyOutputOn ? "OUT: ON" : "OUT: OFF", LCD_COLS);
    const std::string line2 = centerText(timeSs.str(), LCD_COLS);
    const std::string line3 = "";
    const std::string line4 = centerText(clockBottomText_, LCD_COLS);

    return printFourLines(line1, line2, line3, line4);
}

void LCD2004::playIntroSequence(ScreenMode expectedMode, int finalHoldMs)
{
    const char* frames[] = {
        "G",
        "GR",
        "GRO",
        "GROW",
        "GROWB",
        "GROWBO",
        "GROWBOX"
    };

    for (const char* frame : frames) {
        {
            std::lock_guard<std::mutex> lock(mtx_);

            if (!initialized_) {
                return;
            }

            if (!printFourLines("", centerText(frame, LCD_COLS), "", "")) {
                return;
            }
        }

        if (!sleepInterruptible(120, expectedMode)) {
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(mtx_);

        if (!initialized_) {
            return;
        }

        if (!printFourLines("",
                            centerText("GROWBOX", LCD_COLS),
                            centerText("by wojtron", LCD_COLS),
                            "")) {
            return;
        }
    }

    sleepInterruptible(finalHoldMs, expectedMode);
}

bool LCD2004::sleepInterruptible(int totalMs, ScreenMode expectedMode)
{
    int waited = 0;

    while (running_.load() && waited < totalMs) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (screenMode_ != expectedMode) {
                return false;
            }
        }

        const int chunk = ((totalMs - waited) > 50) ? 50 : (totalMs - waited);
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
        waited += chunk;
    }

    return running_.load();
}

std::string LCD2004::centerText(const std::string& text, std::size_t width)
{
    if (text.size() >= width) {
        return text.substr(0, width);
    }

    const std::size_t left = (width - text.size()) / 2;
    const std::size_t right = width - text.size() - left;

    return std::string(left, ' ') + text + std::string(right, ' ');
}

std::string LCD2004::rightAlignText(const std::string& text, std::size_t width)
{
    if (text.size() >= width) {
        return text.substr(0, width);
    }

    return std::string(width - text.size(), ' ') + text;
}

std::string LCD2004::fitToWidth(const std::string& text, std::size_t width)
{
    if (text.size() >= width) {
        return text.substr(0, width);
    }

    return text + std::string(width - text.size(), ' ');
}

const char* LCD2004::screenModeToString(ScreenMode mode)
{
    switch (mode) {
    case ScreenMode::StartupIntro:
        return "startup_intro";
    case ScreenMode::Intro:
        return "intro";
    case ScreenMode::IntroLoop:
        return "intro_loop";
    case ScreenMode::StaticLogo:
        return "static_logo";
    case ScreenMode::Clock:
        return "clock";
    default:
        return "unknown";
    }
}

// end of file: src/display/LCD2004.cpp
