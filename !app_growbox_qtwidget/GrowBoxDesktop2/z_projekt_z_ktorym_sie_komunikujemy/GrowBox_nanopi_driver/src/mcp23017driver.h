#pragma once

#include <cstdint>
#include <mutex>
#include <string>

class MCP23017Driver {
public:
    MCP23017Driver(int busNumber = 0, int address = 0x20);
    ~MCP23017Driver();

    bool init();
    void closeDevice();

    bool isInitialized() const;

    // ================= ZAPIS =================

    bool setChannel(int channel, bool on);

    bool setGroup1On();
    bool setGroup1Off();

    bool setGroup2On();
    bool setGroup2Off();

    bool setAllOn();
    bool setAllOff();

    // ================= ODCZYT PUBLICZNY =================

    bool readRegister(uint8_t reg, uint8_t& value);

    bool readIODIR(uint8_t& iodirA, uint8_t& iodirB);
    bool readGPIO(uint8_t& gpioA, uint8_t& gpioB);
    bool readOLAT(uint8_t& olatA, uint8_t& olatB);
    bool readINTF(uint8_t& intfA, uint8_t& intfB);
    bool readINTCAP(uint8_t& intcapA, uint8_t& intcapB);

    uint8_t getCachedGPIOA() const;
    uint8_t getCachedGPIOB() const;

    // ================= STALE REJESTROW =================

    static constexpr uint8_t REG_IODIRA   = 0x00;
    static constexpr uint8_t REG_IODIRB   = 0x01;
    static constexpr uint8_t REG_IPOLA    = 0x02;
    static constexpr uint8_t REG_IPOLB    = 0x03;
    static constexpr uint8_t REG_GPINTENA = 0x04;
    static constexpr uint8_t REG_GPINTENB = 0x05;
    static constexpr uint8_t REG_DEFVALA  = 0x06;
    static constexpr uint8_t REG_DEFVALB  = 0x07;
    static constexpr uint8_t REG_INTCONA  = 0x08;
    static constexpr uint8_t REG_INTCONB  = 0x09;
    static constexpr uint8_t REG_IOCON    = 0x0A;
    static constexpr uint8_t REG_GPPUA    = 0x0C;
    static constexpr uint8_t REG_GPPUB    = 0x0D;
    static constexpr uint8_t REG_INTFA    = 0x0E;
    static constexpr uint8_t REG_INTFB    = 0x0F;
    static constexpr uint8_t REG_INTCAPA  = 0x10;
    static constexpr uint8_t REG_INTCAPB  = 0x11;
    static constexpr uint8_t REG_GPIOA    = 0x12;
    static constexpr uint8_t REG_GPIOB    = 0x13;
    static constexpr uint8_t REG_OLATA    = 0x14;
    static constexpr uint8_t REG_OLATB    = 0x15;

private:
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegisterInternal(uint8_t reg, uint8_t& value);

    bool writeGPIOA(uint8_t value);
    bool writeGPIOB(uint8_t value);

    bool updateChannelInCache(int channel, bool on);

private:
    int busNumber_;
    int address_;
    int fd_;
    bool initialized_;

    uint8_t gpioA_;
    uint8_t gpioB_;

    mutable std::mutex mtx_;
};