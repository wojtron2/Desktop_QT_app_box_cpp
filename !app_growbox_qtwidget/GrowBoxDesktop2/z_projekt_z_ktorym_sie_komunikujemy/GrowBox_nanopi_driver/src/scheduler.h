// file: src/scheduler.h

#pragma once
#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <string>
#include <vector>

#include "mcp23017driver.h"

class SoundManager;
class LCD2004;

class Scheduler {
public:
    explicit Scheduler(const std::string& stateFilePath);
    ~Scheduler();

    void start();
    void stop();

    void setSoundManager(SoundManager* sm);
    void setMcp23017Driver(MCP23017Driver* driver);
    void setLcd(LCD2004* lcd);
    void loadStateFromDisk();

    // ================= OUTPUTS =================

    void setOutputOn(int id);
    void setOutputOff(int id);
    void setOutputManualOn(int id);
    void setOutputManualOff(int id);
    bool isOutputOn(int id) const;
    bool isAnyOutputOn() const;

    void setAllOutputsOn();
    void setAllOutputsOff();
    void setAllOutputsManualOn();
    void setAllOutputsManualOff();

    void setAutoOnHour(int id, int hour);
    void setAutoOffHour(int id, int hour);
    int  getAutoOnHour(int id) const;
    int  getAutoOffHour(int id) const;

    void setAutoOnTime(int id, int hour, int minute);
    void setAutoOffTime(int id, int hour, int minute);
    void setAllOutputsAutoOnTime(int hour, int minute);
    void setAllOutputsAutoOffTime(int hour, int minute);
    int  getAutoOnMinute(int id) const;
    int  getAutoOffMinute(int id) const;

    void setManualMode(int id, bool enabled);
    bool isManualMode(int id) const;
    void setAllManualMode(bool enabled);

    void setGlobalAutoMode(bool enabled);
    bool isGlobalAutoMode() const;

    // ================= LCD BACKLIGHT =================

    static constexpr int BACKLIGHT_RULE_COUNT = 3;

    struct BacklightRuleSnapshot {
        int id = 0;
        bool enabled = false;
        bool workdays = false;
        int startHour = 0;
        int startMinute = 0;
        int endHour = 0;
        int endMinute = 0;
    };

    void configureBacklightRule(int ruleId,
                                bool enabled,
                                int startHour,
                                int startMinute,
                                int endHour,
                                int endMinute);

    void setBacklightRuleEnabled(int ruleId, bool enabled);
    BacklightRuleSnapshot getBacklightRuleSnapshot(int ruleId) const;
    bool isBacklightSchedulerActive() const;

    // ================= SOUND =================

    static constexpr int SOUND_SLOT_COUNT = 10;

    void setGlobalSoundVolume(int volume);
    int  getGlobalSoundVolume() const;

    void setGlobalSoundAutoMode(bool enabled);
    bool isGlobalSoundAutoMode() const;

    void setSoundMultipleMode(bool enabled);
    bool isSoundMultipleMode() const;
    int  getActiveSoundCount();
    std::vector<std::string> getAvailableSoundFiles() const;

    void playSoundNow(const std::string& file, int volume);

    struct SoundAlarmSnapshot {
        bool enabled = false;
        int hour = -1;
        int minute = 0;
        int volume = 100;
        std::string file;
    };

    void configureSoundAlarm(int slot,
                             bool enabled,
                             int hour,
                             int minute,
                             const std::string& file,
                             int volume);

    void setSoundAlarmEnabled(int slot, bool enabled);

    SoundAlarmSnapshot getSoundAlarmSnapshot(int slot) const;

private:
    void loop();
    void loadState();
    void saveState();
    void applyOutput(int id, bool on);
    void applyAutoStateNow();
    void applyBacklightScheduleNow();

    static constexpr int OUTPUT_COUNT = 16;

    static bool validOutputId(int id);
    static bool validHour(int hour);
    static bool validMinute(int minute);
    static bool validSoundSlot(int slot);
    static bool validBacklightRuleId(int ruleId);
    static bool validBacklightTime(int hour, int minute);
    static int  clampVolume(int v);

    std::atomic<bool> running{false};
    std::atomic<bool> globalAutoMode{true};

    std::array<std::atomic<bool>, OUTPUT_COUNT> outputState;
    std::array<std::atomic<int>,  OUTPUT_COUNT> autoOnHour;
    std::array<std::atomic<int>,  OUTPUT_COUNT> autoOffHour;
    std::array<std::atomic<int>,  OUTPUT_COUNT> autoOnMinute;
    std::array<std::atomic<int>,  OUTPUT_COUNT> autoOffMinute;
    std::array<std::atomic<bool>, OUTPUT_COUNT> manualMode;

    std::array<std::atomic<bool>, BACKLIGHT_RULE_COUNT> backlightRuleEnabled;
    std::array<std::atomic<int>,  BACKLIGHT_RULE_COUNT> backlightStartHour;
    std::array<std::atomic<int>,  BACKLIGHT_RULE_COUNT> backlightStartMinute;
    std::array<std::atomic<int>,  BACKLIGHT_RULE_COUNT> backlightEndHour;
    std::array<std::atomic<int>,  BACKLIGHT_RULE_COUNT> backlightEndMinute;
    std::atomic<bool> backlightSchedulerActive{false};

    std::atomic<int> globalSoundVolume{100};
    std::atomic<bool> globalSoundAutoMode{true};
    std::atomic<bool> soundMultipleMode{false};

    std::array<std::atomic<bool>, SOUND_SLOT_COUNT> soundEnabled;
    std::array<std::atomic<int>,  SOUND_SLOT_COUNT> soundHour;
    std::array<std::atomic<int>,  SOUND_SLOT_COUNT> soundMinute;
    std::array<std::atomic<int>,  SOUND_SLOT_COUNT> soundVolume;
    std::array<std::string,        SOUND_SLOT_COUNT> soundFile;

    SoundManager* soundManager = nullptr;
    MCP23017Driver* mcp23017Driver = nullptr;
    LCD2004* lcd = nullptr;

    std::thread worker;

    // Mutex tylko dla pliku stanu
    mutable std::mutex fileMutex;

    // Mutex tylko dla soundFile[]
    mutable std::mutex soundFileMutex;

    std::string stateFile;
};

// end of file: src/scheduler.h
