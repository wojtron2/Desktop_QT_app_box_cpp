#include "mcp23017driver.h"

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

MCP23017Driver::MCP23017Driver(int busNumber, int address)
    : busNumber_(busNumber),
      address_(address),
      fd_(-1),
      initialized_(false),
      gpioA_(0x00),
      gpioB_(0x00)
{
}

MCP23017Driver::~MCP23017Driver()
{
    closeDevice();
}

bool MCP23017Driver::init()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (initialized_) {
        return true;
    }

    std::string devPath = "/dev/i2c-" + std::to_string(busNumber_);

    fd_ = open(devPath.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "[MCP23017] open failed: " << devPath << "\n";
        return false;
    }

    if (ioctl(fd_, I2C_SLAVE, address_) < 0) {
        std::cerr << "[MCP23017] ioctl I2C_SLAVE failed, addr=0x"
                  << std::hex << address_ << std::dec << "\n";
        close(fd_);
        fd_ = -1;
        return false;
    }

    // Wszystkie piny jako wyjscia
    if (!writeRegister(REG_IODIRA, 0x00)) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    if (!writeRegister(REG_IODIRB, 0x00)) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    // Startowo wszystko OFF
    gpioA_ = 0x00;
    gpioB_ = 0x00;

    if (!writeRegister(REG_GPIOA, gpioA_)) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    if (!writeRegister(REG_GPIOB, gpioB_)) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    initialized_ = true;

    std::cout << "[MCP23017] init OK, bus=" << busNumber_
              << " addr=0x" << std::hex << address_ << std::dec << "\n";

    return true;
}

void MCP23017Driver::closeDevice()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }

    initialized_ = false;
}

bool MCP23017Driver::isInitialized() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return initialized_;
}

bool MCP23017Driver::writeRegister(uint8_t reg, uint8_t value)
{
    if (fd_ < 0) {
        return false;
    }

    uint8_t buffer[2] = { reg, value };

    ssize_t written = write(fd_, buffer, 2);
    if (written != 2) {
        std::cerr << "[MCP23017] writeRegister failed reg=0x"
                  << std::hex << static_cast<int>(reg)
                  << " value=0x" << static_cast<int>(value)
                  << std::dec << "\n";
        return false;
    }

    return true;
}

bool MCP23017Driver::readRegisterInternal(uint8_t reg, uint8_t& value)
{
    if (fd_ < 0) {
        return false;
    }

    uint8_t regBuf = reg;
    if (write(fd_, &regBuf, 1) != 1) {
        std::cerr << "[MCP23017] set register for read failed reg=0x"
                  << std::hex << static_cast<int>(reg) << std::dec << "\n";
        return false;
    }

    uint8_t data = 0;
    if (read(fd_, &data, 1) != 1) {
        std::cerr << "[MCP23017] read failed reg=0x"
                  << std::hex << static_cast<int>(reg) << std::dec << "\n";
        return false;
    }

    value = data;
    return true;
}

bool MCP23017Driver::readRegister(uint8_t reg, uint8_t& value)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    return readRegisterInternal(reg, value);
}

bool MCP23017Driver::readIODIR(uint8_t& iodirA, uint8_t& iodirB)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    if (!readRegisterInternal(REG_IODIRA, iodirA)) {
        return false;
    }

    if (!readRegisterInternal(REG_IODIRB, iodirB)) {
        return false;
    }

    return true;
}

bool MCP23017Driver::readGPIO(uint8_t& gpioA, uint8_t& gpioB)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    if (!readRegisterInternal(REG_GPIOA, gpioA)) {
        return false;
    }

    if (!readRegisterInternal(REG_GPIOB, gpioB)) {
        return false;
    }

    return true;
}

bool MCP23017Driver::readOLAT(uint8_t& olatA, uint8_t& olatB)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    if (!readRegisterInternal(REG_OLATA, olatA)) {
        return false;
    }

    if (!readRegisterInternal(REG_OLATB, olatB)) {
        return false;
    }

    return true;
}

bool MCP23017Driver::readINTF(uint8_t& intfA, uint8_t& intfB)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    if (!readRegisterInternal(REG_INTFA, intfA)) {
        return false;
    }

    if (!readRegisterInternal(REG_INTFB, intfB)) {
        return false;
    }

    return true;
}

bool MCP23017Driver::readINTCAP(uint8_t& intcapA, uint8_t& intcapB)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    if (!readRegisterInternal(REG_INTCAPA, intcapA)) {
        return false;
    }

    if (!readRegisterInternal(REG_INTCAPB, intcapB)) {
        return false;
    }

    return true;
}

bool MCP23017Driver::writeGPIOA(uint8_t value)
{
    if (!writeRegister(REG_GPIOA, value)) {
        return false;
    }

    gpioA_ = value;
    return true;
}

bool MCP23017Driver::writeGPIOB(uint8_t value)
{
    if (!writeRegister(REG_GPIOB, value)) {
        return false;
    }

    gpioB_ = value;
    return true;
}

bool MCP23017Driver::updateChannelInCache(int channel, bool on)
{
    if (channel < 1 || channel > 16) {
        return false;
    }

    if (channel <= 8) {
        uint8_t bit = static_cast<uint8_t>(channel - 1);

        if (on) {
            gpioA_ |= static_cast<uint8_t>(1u << bit);
        } else {
            gpioA_ &= static_cast<uint8_t>(~(1u << bit));
        }

        return writeGPIOA(gpioA_);
    }

    uint8_t bit = static_cast<uint8_t>(channel - 9);

    if (on) {
        gpioB_ |= static_cast<uint8_t>(1u << bit);
    } else {
        gpioB_ &= static_cast<uint8_t>(~(1u << bit));
    }

    return writeGPIOB(gpioB_);
}

bool MCP23017Driver::setChannel(int channel, bool on)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    return updateChannelInCache(channel, on);
}

bool MCP23017Driver::setGroup1On()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    return writeGPIOA(0xFF);
}

bool MCP23017Driver::setGroup1Off()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    return writeGPIOA(0x00);
}

bool MCP23017Driver::setGroup2On()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    return writeGPIOB(0xFF);
}

bool MCP23017Driver::setGroup2Off()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    return writeGPIOB(0x00);
}

bool MCP23017Driver::setAllOn()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    bool okA = writeGPIOA(0xFF);
    bool okB = writeGPIOB(0xFF);

    return okA && okB;
}

bool MCP23017Driver::setAllOff()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_) {
        return false;
    }

    bool okA = writeGPIOA(0x00);
    bool okB = writeGPIOB(0x00);

    return okA && okB;
}

uint8_t MCP23017Driver::getCachedGPIOA() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return gpioA_;
}

uint8_t MCP23017Driver::getCachedGPIOB() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return gpioB_;
}