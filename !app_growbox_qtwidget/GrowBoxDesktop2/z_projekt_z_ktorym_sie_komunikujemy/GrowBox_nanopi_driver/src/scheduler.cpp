// file: src/scheduler.cpp

#include "scheduler.h"
#include "soundmanager.h"
#include "display/LCD2004.h"
#include "libs/json.hpp"

#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <atomic>

using json = nlohmann::json;

bool Scheduler::validOutputId(int id) { return id >= 1 && id <= OUTPUT_COUNT; }
bool Scheduler::validHour(int hour) { return hour >= 0 && hour <= 23; }
bool Scheduler::validMinute(int minute) { return minute >= 0 && minute <= 59; }
bool Scheduler::validSoundSlot(int slot) { return slot >= 1 && slot <= SOUND_SLOT_COUNT; }
bool Scheduler::validBacklightRuleId(int ruleId) { return ruleId >= 1 && ruleId <= BACKLIGHT_RULE_COUNT; }
bool Scheduler::validBacklightTime(int hour, int minute)
{
    if (hour < 0 || hour > 24) return false;
    if (minute < 0 || minute > 59) return false;
    if (hour == 24 && minute != 0) return false;
    return true;
}

static std::atomic<bool> outputSoundEnabled{false};

int Scheduler::clampVolume(int v)
{
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

Scheduler::Scheduler(const std::string& stateFilePath)
    : stateFile(stateFilePath)
{
    for (int i = 0; i < OUTPUT_COUNT; ++i) {
        outputState[i] = false;

        autoOnHour[i]   = -1;
        autoOnMinute[i] = 0;

        autoOffHour[i]   = -1;
        autoOffMinute[i] = 0;

        manualMode[i]  = false;
    }

    backlightRuleEnabled[0] = false;
    backlightStartHour[0] = 6;
    backlightStartMinute[0] = 0;
    backlightEndHour[0] = 8;
    backlightEndMinute[0] = 0;

    backlightRuleEnabled[1] = false;
    backlightStartHour[1] = 16;
    backlightStartMinute[1] = 0;
    backlightEndHour[1] = 23;
    backlightEndMinute[1] = 0;

    backlightRuleEnabled[2] = false;
    backlightStartHour[2] = 8;
    backlightStartMinute[2] = 0;
    backlightEndHour[2] = 24;
    backlightEndMinute[2] = 0;

    backlightSchedulerActive = false;

    globalAutoMode = true;
    globalSoundVolume = 100;
    globalSoundAutoMode = true;
    soundMultipleMode = false;

    for (int s = 0; s < SOUND_SLOT_COUNT; ++s) {
        soundEnabled[s] = false;
        soundHour[s] = -1;
        soundMinute[s] = 0;
        soundVolume[s] = 100;
        soundFile[s].clear();
    }
}

Scheduler::~Scheduler()
{
    stop();
}

void Scheduler::setSoundManager(SoundManager* sm)
{
    soundManager = sm;
    if (soundManager) {
        soundManager->setGlobalVolume(globalSoundVolume.load());
        soundManager->setMultipleMode(soundMultipleMode.load());
    }
}

void Scheduler::setMcp23017Driver(MCP23017Driver* driver)
{
    mcp23017Driver = driver;
}

void Scheduler::setLcd(LCD2004* lcdPtr)
{
    lcd = lcdPtr;
}

void Scheduler::loadStateFromDisk()
{
    loadState();
    applyAutoStateNow();
    applyBacklightScheduleNow();
}

void Scheduler::start()
{
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true)) {
        return; // juz dziala
    }
    worker = std::thread(&Scheduler::loop, this);
}

void Scheduler::stop()
{
    bool expected = true;
    if (!running.compare_exchange_strong(expected, false)) {
        return; // juz zatrzymany
    }
    if (worker.joinable())
        worker.join();
}

void Scheduler::setOutputOn(int id)
{
    if (!validOutputId(id)) return;

    outputState[id - 1] = true;
    applyOutput(id, true);
    saveState();
}

void Scheduler::setOutputOff(int id)
{
    if (!validOutputId(id)) return;

    outputState[id - 1] = false;
    applyOutput(id, false);
    saveState();
}

void Scheduler::setOutputManualOn(int id)
{
    if (!validOutputId(id)) return;

    manualMode[id - 1] = true;
    outputState[id - 1] = true;
    applyOutput(id, true);
    saveState();
}

void Scheduler::setOutputManualOff(int id)
{
    if (!validOutputId(id)) return;

    manualMode[id - 1] = true;
    outputState[id - 1] = false;
    applyOutput(id, false);
    saveState();
}

bool Scheduler::isOutputOn(int id) const
{
    if (!validOutputId(id)) return false;
    return outputState[id - 1].load();
}

bool Scheduler::isAnyOutputOn() const
{
    for (const auto& state : outputState) {
        if (state.load()) {
            return true;
        }
    }

    return false;
}

void Scheduler::setAllOutputsOn()
{
    for (int i = 0; i < OUTPUT_COUNT; ++i) {
        outputState[i] = true;
    }

    std::cout << "[OUTPUTS] ALL ON\n";

    if (mcp23017Driver) {
        bool ok = mcp23017Driver->setAllOn();
        if (!ok) {
            std::cerr << "[MCP23017] setAllOn failed\n";
        }
    }

    saveState();
}

void Scheduler::setAllOutputsOff()
{
    for (int i = 0; i < OUTPUT_COUNT; ++i) {
        outputState[i] = false;
    }

    std::cout << "[OUTPUTS] ALL OFF\n";

    if (mcp23017Driver) {
        bool ok = mcp23017Driver->setAllOff();
        if (!ok) {
            std::cerr << "[MCP23017] setAllOff failed\n";
        }
    }

    saveState();
}

void Scheduler::setAllOutputsManualOn()
{
    for (int i = 0; i < OUTPUT_COUNT; ++i) {
        manualMode[i] = true;
        outputState[i] = true;
    }

    std::cout << "[OUTPUTS] ALL MANUAL ON\n";

    if (mcp23017Driver) {
        bool ok = mcp23017Driver->setAllOn();
        if (!ok) {
            std::cerr << "[MCP23017] setAllOn failed\n";
        }
    }

    saveState();
}

void Scheduler::setAllOutputsManualOff()
{
    for (int i = 0; i < OUTPUT_COUNT; ++i) {
        manualMode[i] = true;
        outputState[i] = false;
    }

    std::cout << "[OUTPUTS] ALL MANUAL OFF\n";

    if (mcp23017Driver) {
        bool ok = mcp23017Driver->setAllOff();
        if (!ok) {
            std::cerr << "[MCP23017] setAllOff failed\n";
        }
    }

    saveState();
}

void Scheduler::setAutoOnHour(int id, int hour)
{
    if (!validOutputId(id)) return;
    if (!validHour(hour)) return;

    autoOnHour[id - 1] = hour;
    autoOnMinute[id - 1] = 0; // kompatybilnosc wstecz
    saveState();
    applyAutoStateNow();
}

void Scheduler::setAutoOffHour(int id, int hour)
{
    if (!validOutputId(id)) return;
    if (!validHour(hour)) return;

    autoOffHour[id - 1] = hour;
    autoOffMinute[id - 1] = 0; // kompatybilnosc wstecz
    saveState();
    applyAutoStateNow();
}

int Scheduler::getAutoOnHour(int id) const
{
    if (!validOutputId(id)) return -1;
    return autoOnHour[id - 1].load();
}

int Scheduler::getAutoOffHour(int id) const
{
    if (!validOutputId(id)) return -1;
    return autoOffHour[id - 1].load();
}

void Scheduler::setAutoOnTime(int id, int hour, int minute)
{
    if (!validOutputId(id)) return;
    if (!validHour(hour)) return;
    if (!validMinute(minute)) return;

    autoOnHour[id - 1] = hour;
    autoOnMinute[id - 1] = minute;
    saveState();
    applyAutoStateNow();
}

void Scheduler::setAutoOffTime(int id, int hour, int minute)
{
    if (!validOutputId(id)) return;
    if (!validHour(hour)) return;
    if (!validMinute(minute)) return;

    autoOffHour[id - 1] = hour;
    autoOffMinute[id - 1] = minute;
    saveState();
    applyAutoStateNow();
}

void Scheduler::setAllOutputsAutoOnTime(int hour, int minute)
{
    if (!validHour(hour)) return;
    if (!validMinute(minute)) return;

    for (int i = 0; i < OUTPUT_COUNT; ++i) {
        autoOnHour[i] = hour;
        autoOnMinute[i] = minute;
    }

    saveState();
    applyAutoStateNow();
}

void Scheduler::setAllOutputsAutoOffTime(int hour, int minute)
{
    if (!validHour(hour)) return;
    if (!validMinute(minute)) return;

    for (int i = 0; i < OUTPUT_COUNT; ++i) {
        autoOffHour[i] = hour;
        autoOffMinute[i] = minute;
    }

    saveState();
    applyAutoStateNow();
}

int Scheduler::getAutoOnMinute(int id) const
{
    if (!validOutputId(id)) return 0;
    return autoOnMinute[id - 1].load();
}

int Scheduler::getAutoOffMinute(int id) const
{
    if (!validOutputId(id)) return 0;
    return autoOffMinute[id - 1].load();
}

void Scheduler::setManualMode(int id, bool enabled)
{
    if (!validOutputId(id)) return;

    manualMode[id - 1] = enabled;
    saveState();

    if (!enabled) {
        applyAutoStateNow();
    }
}

bool Scheduler::isManualMode(int id) const
{
    if (!validOutputId(id)) return false;
    return manualMode[id - 1].load();
}

void Scheduler::setAllManualMode(bool enabled)
{
    for (int i = 0; i < OUTPUT_COUNT; ++i) {
        manualMode[i] = enabled;
    }

    saveState();

    if (!enabled) {
        applyAutoStateNow();
    }
}

void Scheduler::setGlobalAutoMode(bool enabled)
{
    globalAutoMode = enabled;
    saveState();

    if (enabled) {
        applyAutoStateNow();
    }
}

bool Scheduler::isGlobalAutoMode() const
{
    return globalAutoMode.load();
}

void Scheduler::configureBacklightRule(int ruleId,
                                       bool enabled,
                                       int startHour,
                                       int startMinute,
                                       int endHour,
                                       int endMinute)
{
    if (!validBacklightRuleId(ruleId)) return;
    if (!validBacklightTime(startHour, startMinute)) return;
    if (!validBacklightTime(endHour, endMinute)) return;

    const int idx = ruleId - 1;

    backlightRuleEnabled[idx] = enabled;
    backlightStartHour[idx] = startHour;
    backlightStartMinute[idx] = startMinute;
    backlightEndHour[idx] = endHour;
    backlightEndMinute[idx] = endMinute;

    saveState();
    applyBacklightScheduleNow();
}

void Scheduler::setBacklightRuleEnabled(int ruleId, bool enabled)
{
    if (!validBacklightRuleId(ruleId)) return;

    backlightRuleEnabled[ruleId - 1] = enabled;
    saveState();
    applyBacklightScheduleNow();
}

Scheduler::BacklightRuleSnapshot Scheduler::getBacklightRuleSnapshot(int ruleId) const
{
    BacklightRuleSnapshot s;

    if (!validBacklightRuleId(ruleId))
        return s;

    const int idx = ruleId - 1;

    s.id = ruleId;
    s.enabled = backlightRuleEnabled[idx].load();
    s.workdays = (ruleId == 1 || ruleId == 2);
    s.startHour = backlightStartHour[idx].load();
    s.startMinute = backlightStartMinute[idx].load();
    s.endHour = backlightEndHour[idx].load();
    s.endMinute = backlightEndMinute[idx].load();

    return s;
}

bool Scheduler::isBacklightSchedulerActive() const
{
    return backlightSchedulerActive.load();
}

void Scheduler::setGlobalSoundVolume(int volume)
{
    globalSoundVolume = clampVolume(volume);

    if (soundManager) {
        soundManager->setGlobalVolume(globalSoundVolume.load());
    }

    saveState();
}

int Scheduler::getGlobalSoundVolume() const
{
    return globalSoundVolume.load();
}

void Scheduler::setGlobalSoundAutoMode(bool enabled)
{
    globalSoundAutoMode = enabled;
    saveState();
}

bool Scheduler::isGlobalSoundAutoMode() const
{
    return globalSoundAutoMode.load();
}

void Scheduler::setSoundMultipleMode(bool enabled)
{
    soundMultipleMode = enabled;

    if (soundManager) {
        soundManager->setMultipleMode(enabled);
    }

    saveState();
}

bool Scheduler::isSoundMultipleMode() const
{
    return soundMultipleMode.load();
}

int Scheduler::getActiveSoundCount()
{
    if (!soundManager) {
        return 0;
    }

    return soundManager->activeCount();
}

std::vector<std::string> Scheduler::getAvailableSoundFiles() const
{
    if (!soundManager) {
        return {};
    }

    return soundManager->listAvailableSounds();
}

void Scheduler::playSoundNow(const std::string& file, int volume)
{
    if (!soundManager) return;
    if (file.empty()) return;

    volume = clampVolume(volume);
    soundManager->play(file, volume);
}

void Scheduler::configureSoundAlarm(int slot,
                                    bool enabled,
                                    int hour,
                                    int minute,
                                    const std::string& file,
                                    int volume)
{
    if (!validSoundSlot(slot)) return;
    if (!validHour(hour)) return;
    if (!validMinute(minute)) return;

    int idx = slot - 1;

    soundEnabled[idx] = enabled;
    soundHour[idx] = hour;
    soundMinute[idx] = minute;
    soundVolume[idx] = clampVolume(volume);

    {
        std::lock_guard<std::mutex> lock(soundFileMutex);
        soundFile[idx] = file;
    }

    saveState();
}

void Scheduler::setSoundAlarmEnabled(int slot, bool enabled)
{
    if (!validSoundSlot(slot)) return;

    soundEnabled[slot - 1] = enabled;
    saveState();
}

Scheduler::SoundAlarmSnapshot Scheduler::getSoundAlarmSnapshot(int slot) const
{
    SoundAlarmSnapshot s;

    if (!validSoundSlot(slot))
        return s;

    int idx = slot - 1;

    s.enabled = soundEnabled[idx].load();
    s.hour = soundHour[idx].load();
    s.minute = soundMinute[idx].load();
    s.volume = soundVolume[idx].load();

    {
        std::lock_guard<std::mutex> lock(soundFileMutex);
        s.file = soundFile[idx];
    }

    return s;
}

void Scheduler::applyOutput(int id, bool on)
{
    std::cout << "[OUTPUT " << id << "] "
              << (on ? "ON\n" : "OFF\n");

    if (mcp23017Driver) {
        bool ok = mcp23017Driver->setChannel(id, on);
        if (!ok) {
            std::cerr << "[MCP23017] setChannel failed, id="
                      << id << " state=" << (on ? "ON" : "OFF") << "\n";
        }
    }

    if (outputSoundEnabled.load() && soundManager && on) {
        soundManager->play("click.mp3", 80);
    }
}

void Scheduler::applyAutoStateNow()
{
    if (!globalAutoMode.load())
        return;

    auto isInActiveWindow = [](int nowTotal, int onTotal, int offTotal) -> bool {
        if (onTotal == offTotal)
            return false;

        if (onTotal < offTotal)
            return nowTotal >= onTotal && nowTotal < offTotal;

        return (nowTotal >= onTotal) || (nowTotal < offTotal);
    };

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local = *std::localtime(&t);

    int hour = local.tm_hour;
    int minute = local.tm_min;
    int minuteTotal = hour * 60 + minute;

    for (int i = 0; i < OUTPUT_COUNT; ++i) {

        if (manualMode[i].load())
            continue;

        int onHour  = autoOnHour[i].load();
        int onMin   = autoOnMinute[i].load();

        int offHour = autoOffHour[i].load();
        int offMin  = autoOffMinute[i].load();

        bool state  = outputState[i].load();

        bool hasOn  = (onHour >= 0);
        bool hasOff = (offHour >= 0);

        if (hasOn && hasOff) {
            int onTotal = onHour * 60 + onMin;
            int offTotal = offHour * 60 + offMin;

            bool shouldBeOn = isInActiveWindow(minuteTotal, onTotal, offTotal);

            if (shouldBeOn && !state)
                setOutputOn(i + 1);

            if (!shouldBeOn && state)
                setOutputOff(i + 1);

            continue;
        }

        if (hasOn && hour == onHour && minute == onMin && !state)
            setOutputOn(i + 1);

        if (hasOff && hour == offHour && minute == offMin && state)
            setOutputOff(i + 1);
    }
}

void Scheduler::applyBacklightScheduleNow()
{
    if (!lcd || !lcd->isInitialized()) {
        backlightSchedulerActive = false;
        return;
    }

    bool anyRuleEnabled = false;
    for (int i = 0; i < BACKLIGHT_RULE_COUNT; ++i) {
        if (backlightRuleEnabled[i].load()) {
            anyRuleEnabled = true;
            break;
        }
    }

    if (!anyRuleEnabled) {
        backlightSchedulerActive = false;
        return;
    }

    auto isInActiveWindow = [](int nowTotal, int onTotal, int offTotal) -> bool {
        if (onTotal == offTotal)
            return false;

        if (onTotal < offTotal)
            return nowTotal >= onTotal && nowTotal < offTotal;

        return (nowTotal >= onTotal) || (nowTotal < offTotal);
    };

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local = *std::localtime(&t);

    const bool isWeekend = (local.tm_wday == 0 || local.tm_wday == 6);
    const int minuteTotal = local.tm_hour * 60 + local.tm_min;

    bool shouldBeOn = false;

    for (int i = 0; i < BACKLIGHT_RULE_COUNT; ++i) {
        const bool ruleEnabled = backlightRuleEnabled[i].load();
        if (!ruleEnabled) {
            continue;
        }

        const bool ruleForWorkdays = (i == 0 || i == 1);
        const bool ruleForWeekend = (i == 2);

        if (ruleForWorkdays && isWeekend) {
            continue;
        }

        if (ruleForWeekend && !isWeekend) {
            continue;
        }

        const int onTotal = backlightStartHour[i].load() * 60 + backlightStartMinute[i].load();
        const int offTotal = backlightEndHour[i].load() * 60 + backlightEndMinute[i].load();

        if (isInActiveWindow(minuteTotal, onTotal, offTotal)) {
            shouldBeOn = true;
            break;
        }
    }

    backlightSchedulerActive = shouldBeOn;

    const bool currentState = lcd->isBacklightOn();

    if (shouldBeOn && !currentState) {
        lcd->backlightOn();
    }

    if (!shouldBeOn && currentState) {
        lcd->backlightOff();
    }
}

void Scheduler::loop()
{
    using namespace std::chrono_literals;

    auto isInActiveWindow = [](int nowTotal, int onTotal, int offTotal) -> bool {
        if (onTotal == offTotal)
            return false;

        if (onTotal < offTotal)
            return nowTotal >= onTotal && nowTotal < offTotal;

        return (nowTotal >= onTotal) || (nowTotal < offTotal);
    };

    int lastSoundMinuteTotal = -1;
    int lastOutputMinuteTotal = -1;

    while (running.load()) {

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm local = *std::localtime(&t);

        int hour   = local.tm_hour;
        int minute = local.tm_min;

        // ================= SOUND ALARMS =================
        int minuteTotal = hour * 60 + minute;
        if (minuteTotal != lastSoundMinuteTotal) {
            lastSoundMinuteTotal = minuteTotal;

            for (int s = 0; s < SOUND_SLOT_COUNT; ++s) {

                if (!soundEnabled[s].load())
                    continue;

                int sh = soundHour[s].load();
                int sm = soundMinute[s].load();

                if (sh == hour && sm == minute) {

                    std::string fileCopy;
                    {
                        std::lock_guard<std::mutex> lock(soundFileMutex);
                        fileCopy = soundFile[s];
                    }

                    int vol = soundVolume[s].load();

                    if (!globalSoundAutoMode.load()) {
                        std::cout << "[SOUND AUTO] global sounds off, skipped slot="
                                  << (s + 1) << " file=" << fileCopy << "\n";
                        continue;
                    }

                    if (soundManager && !fileCopy.empty()) {
                        soundManager->play(fileCopy, vol);
                    }
                }
            }
        }

        // ================= OUTPUT AUTO =================
        if (globalAutoMode.load() && minuteTotal != lastOutputMinuteTotal) {

            lastOutputMinuteTotal = minuteTotal;

            for (int i = 0; i < OUTPUT_COUNT; ++i) {

                if (manualMode[i].load())
                    continue;

                int onHour  = autoOnHour[i].load();
                int onMin   = autoOnMinute[i].load();

                int offHour = autoOffHour[i].load();
                int offMin  = autoOffMinute[i].load();

                bool state  = outputState[i].load();

                bool hasOn  = (onHour >= 0);
                bool hasOff = (offHour >= 0);

                if (hasOn && hasOff) {
                    int onTotal = onHour * 60 + onMin;
                    int offTotal = offHour * 60 + offMin;

                    bool shouldBeOn = isInActiveWindow(minuteTotal, onTotal, offTotal);

                    if (shouldBeOn && !state)
                        setOutputOn(i + 1);

                    if (!shouldBeOn && state)
                        setOutputOff(i + 1);

                    continue;
                }

                if (hasOn && hour == onHour && minute == onMin && !state)
                    setOutputOn(i + 1);

                if (hasOff && hour == offHour && minute == offMin && state)
                    setOutputOff(i + 1);
            }
        }

        // ================= LCD BACKLIGHT AUTO =================
        applyBacklightScheduleNow();

        std::this_thread::sleep_for(2s);
    }
}

void Scheduler::loadState()
{
    // Lock tylko dla I/O i parsowania pliku
    std::lock_guard<std::mutex> lock(fileMutex);

    std::ifstream file(stateFile);
    if (!file.is_open())
        return;

    json j = json::parse(file, nullptr, false);
    if (j.is_discarded())
        return;

    globalAutoMode = j.value("globalAutoMode", true);
    globalSoundVolume = clampVolume(j.value("globalSoundVolume", 100));
    globalSoundAutoMode = j.value("globalSoundAutoMode", true);
    soundMultipleMode = j.value("soundMultipleMode", false);

    if (j.contains("outputs")) {
        for (auto& item : j["outputs"]) {

            int id = item.value("id", 0);
            if (!validOutputId(id))
                continue;

            outputState[id - 1] = item.value("state", false);
            autoOnHour[id - 1] = item.value("autoOnHour", -1);
            autoOnMinute[id - 1] = item.value("autoOnMinute", 0);
            autoOffHour[id - 1] = item.value("autoOffHour", -1);
            autoOffMinute[id - 1] = item.value("autoOffMinute", 0);
            manualMode[id - 1] = item.value("manualMode", false);

            applyOutput(id, outputState[id - 1].load());
        }
    }

    if (j.contains("backlightRules")) {
        for (auto& item : j["backlightRules"]) {
            const int id = item.value("id", 0);
            if (!validBacklightRuleId(id))
                continue;

            const int idx = id - 1;

            const int startHour = item.value("startHour", backlightStartHour[idx].load());
            const int startMinute = item.value("startMinute", backlightStartMinute[idx].load());
            const int endHour = item.value("endHour", backlightEndHour[idx].load());
            const int endMinute = item.value("endMinute", backlightEndMinute[idx].load());

            if (!validBacklightTime(startHour, startMinute) ||
                !validBacklightTime(endHour, endMinute)) {
                continue;
            }

            backlightRuleEnabled[idx] = item.value("enabled", false);
            backlightStartHour[idx] = startHour;
            backlightStartMinute[idx] = startMinute;
            backlightEndHour[idx] = endHour;
            backlightEndMinute[idx] = endMinute;
        }
    }

    if (j.contains("soundAlarms")) {
        for (auto& item : j["soundAlarms"]) {

            int slot = item.value("slot", 0);
            if (!validSoundSlot(slot))
                continue;

            int idx = slot - 1;

            soundEnabled[idx] = item.value("enabled", false);
            soundHour[idx] = item.value("hour", -1);
            soundMinute[idx] = item.value("minute", 0);
            soundVolume[idx] = clampVolume(item.value("volume", 100));

            {
                std::lock_guard<std::mutex> sLock(soundFileMutex);
                soundFile[idx] = item.value("file", "");
            }
        }
    }

    if (soundManager) {
        soundManager->setGlobalVolume(globalSoundVolume.load());
        soundManager->setMultipleMode(soundMultipleMode.load());
    }

    std::cout << "[Scheduler] State loaded\n";
}

void Scheduler::saveState()
{
    // Lock tylko dla generacji i zapisu pliku
    std::lock_guard<std::mutex> lock(fileMutex);

    json j;
    j["globalAutoMode"] = globalAutoMode.load();
    j["globalSoundVolume"] = globalSoundVolume.load();
    j["globalSoundAutoMode"] = globalSoundAutoMode.load();
    j["soundMultipleMode"] = soundMultipleMode.load();

    j["outputs"] = json::array();
    for (int i = 0; i < OUTPUT_COUNT; ++i) {
        j["outputs"].push_back({
            {"id", i + 1},
            {"state", outputState[i].load()},
            {"autoOnHour", autoOnHour[i].load()},
            {"autoOffHour", autoOffHour[i].load()},
            {"autoOnMinute", autoOnMinute[i].load()},
            {"autoOffMinute", autoOffMinute[i].load()},
            {"manualMode", manualMode[i].load()}
        });
    }

    j["backlightRules"] = json::array();
    for (int i = 0; i < BACKLIGHT_RULE_COUNT; ++i) {
        j["backlightRules"].push_back({
            {"id", i + 1},
            {"enabled", backlightRuleEnabled[i].load()},
            {"startHour", backlightStartHour[i].load()},
            {"startMinute", backlightStartMinute[i].load()},
            {"endHour", backlightEndHour[i].load()},
            {"endMinute", backlightEndMinute[i].load()}
        });
    }

    j["soundAlarms"] = json::array();
    for (int s = 0; s < SOUND_SLOT_COUNT; ++s) {

        std::string fileCopy;
        {
            std::lock_guard<std::mutex> sLock(soundFileMutex);
            fileCopy = soundFile[s];
        }

        j["soundAlarms"].push_back({
            {"slot", s + 1},
            {"enabled", soundEnabled[s].load()},
            {"hour", soundHour[s].load()},
            {"minute", soundMinute[s].load()},
            {"file", fileCopy},
            {"volume", soundVolume[s].load()}
        });
    }

    std::ofstream out(stateFile);
    if (out.is_open())
        out << j.dump(4);
}

// end of file: src/scheduler.cpp
