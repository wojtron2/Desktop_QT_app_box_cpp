// file: src/httpserver.cpp

#include "httpserver.h"
#include "display/LCD2004.h"
#include "libs/httplib.h"
#include "libs/json.hpp"

#include <iostream>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <unistd.h>

using namespace httplib;
using json = nlohmann::json;
namespace fs = std::filesystem;

static constexpr int OUTPUT_COUNT = 16;
static constexpr int SOUND_SLOT_COUNT = Scheduler::SOUND_SLOT_COUNT;
static const fs::path SOUND_BASE_DIR("/home/w/snd");

// ================= Helpers =================

static bool parseInt(const std::string& s, int& out)
{
    try {
        size_t pos = 0;
        int value = std::stoi(s, &pos);
        if (pos != s.size())
            return false;
        out = value;
        return true;
    }
    catch (...) {
        return false;
    }
}

static bool validateOutputId(int id, Response& res, json& j)
{
    if (id < 1 || id > OUTPUT_COUNT) {
        res.status = 400;
        j["error"] = "invalid_output_id";
        return false;
    }
    return true;
}

static bool validateSoundSlot(int slot, Response& res, json& j)
{
    if (slot < 1 || slot > SOUND_SLOT_COUNT) {
        res.status = 400;
        j["error"] = "invalid_sound_slot";
        return false;
    }
    return true;
}

static bool validateHourMinute(int hour, int minute, Response& res, json& j)
{
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        res.status = 400;
        j["error"] = "time_out_of_range";
        return false;
    }
    return true;
}

static bool validateBacklightHourMinute(int hour, int minute, Response& res, json& j)
{
    if (hour < 0 || hour > 24 || minute < 0 || minute > 59 || (hour == 24 && minute != 0)) {
        res.status = 400;
        j["error"] = "backlight_time_out_of_range";
        return false;
    }
    return true;
}

static bool validateVolume(int volume, Response& res, json& j)
{
    if (volume < 0 || volume > 100) {
        res.status = 400;
        j["error"] = "volume_out_of_range";
        return false;
    }
    return true;
}

static bool executableExists(const std::string& name)
{
    if (name.empty()) {
        return false;
    }

    if (name.find('/') != std::string::npos) {
        return access(name.c_str(), X_OK) == 0;
    }

    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) {
        return false;
    }

    std::stringstream ss(pathEnv);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) {
            continue;
        }

        fs::path candidate = fs::path(dir) / name;
        if (access(candidate.c_str(), X_OK) == 0) {
            return true;
        }
    }

    return false;
}

// =================================================

HttpServer::HttpServer(const std::string& bind_address,
                       int port,
                       const std::string& www_dir,
                       Scheduler& scheduler,
                       LCD2004& lcd)
    : bind_address_(bind_address),
      port_(port),
      www_dir_(www_dir),
      scheduler_(scheduler),
      lcd_(lcd) {}

void HttpServer::run()
{
    Server svr;

    // ================= STATUS =================
    // includes: deviceLocalTime, deviceUtcTime, globalAutoMode, outputs[], soundAlarms[], globalSoundVolume

    svr.Get("/api/status", [&](const Request&, Response& res) {

        json j;

        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);

        std::tm localTm = *std::localtime(&tt);
        std::tm utcTm = *std::gmtime(&tt);

        std::ostringstream localSs;
        localSs << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S");

        std::ostringstream utcSs;
        utcSs << std::put_time(&utcTm, "%Y-%m-%d %H:%M:%S");

        j["deviceLocalTime"] = localSs.str();
        j["deviceUtcTime"] = utcSs.str();
        j["deviceEpoch"] = static_cast<long long>(tt);

        j["globalAutoMode"] = scheduler_.isGlobalAutoMode();
        j["globalSoundVolume"] = scheduler_.getGlobalSoundVolume();
        j["globalSoundAutoMode"] = scheduler_.isGlobalSoundAutoMode();
        j["soundMultipleMode"] = scheduler_.isSoundMultipleMode();
        j["activeSoundCount"] = scheduler_.getActiveSoundCount();

        j["outputs"] = json::array();
        for (int i = 1; i <= OUTPUT_COUNT; ++i) {
            j["outputs"].push_back({
                {"id", i},
                {"state", scheduler_.isOutputOn(i)},
                {"autoOnHour", scheduler_.getAutoOnHour(i)},
                {"autoOnMinute", scheduler_.getAutoOnMinute(i)},
                {"autoOffHour", scheduler_.getAutoOffHour(i)},
                {"autoOffMinute", scheduler_.getAutoOffMinute(i)},
                {"manualMode", scheduler_.isManualMode(i)}
            });
        }

        j["soundAlarms"] = json::array();
        for (int s = 1; s <= SOUND_SLOT_COUNT; ++s) {
            auto snap = scheduler_.getSoundAlarmSnapshot(s);
            j["soundAlarms"].push_back({
                {"slot", s},
                {"enabled", snap.enabled},
                {"hour", snap.hour},
                {"minute", snap.minute},
                {"file", snap.file},
                {"volume", snap.volume}
            });
        }

        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/display/status", [&](const Request&, Response& res) {

        json j;

        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);

        std::tm localTm = *std::localtime(&tt);

        std::ostringstream localSs;
        localSs << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S");

        j["lcdInitialized"] = lcd_.isInitialized();
        j["mode"] = lcd_.getScreenModeName();
        j["introRunning"] = lcd_.isIntroRunning();
        j["backlightOn"] = lcd_.isBacklightOn();
        j["backlightSchedulerActive"] = scheduler_.isBacklightSchedulerActive();
        j["clockBottomText"] = lcd_.getClockBottomText();
        j["outputState"] = scheduler_.isAnyOutputOn();
        j["out1State"] = scheduler_.isAnyOutputOn();
        j["deviceLocalTime"] = localSs.str();
        j["deviceEpoch"] = static_cast<long long>(tt);

        j["backlightRules"] = json::array();
        for (int i = 1; i <= Scheduler::BACKLIGHT_RULE_COUNT; ++i) {
            auto snap = scheduler_.getBacklightRuleSnapshot(i);
            j["backlightRules"].push_back({
                {"id", snap.id},
                {"enabled", snap.enabled},
                {"workdays", snap.workdays},
                {"startHour", snap.startHour},
                {"startMinute", snap.startMinute},
                {"endHour", snap.endHour},
                {"endMinute", snap.endMinute}
            });
        }

        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/display/mode/intro", [&](const Request&, Response& res) {

        json j;

        if (!lcd_.isInitialized()) {
            res.status = 500;
            j["error"] = "lcd_not_initialized";
            res.set_content(j.dump(), "application/json");
            return;
        }

        lcd_.setScreenModeIntro();

        j["result"] = "display_intro_started";
        j["mode"] = "intro";
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/display/mode/intro_loop", [&](const Request&, Response& res) {

        json j;

        if (!lcd_.isInitialized()) {
            res.status = 500;
            j["error"] = "lcd_not_initialized";
            res.set_content(j.dump(), "application/json");
            return;
        }

        lcd_.setScreenModeIntroLoop();

        j["result"] = "display_intro_loop_started";
        j["mode"] = "intro_loop";
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/display/mode/clock", [&](const Request&, Response& res) {

        json j;

        if (!lcd_.isInitialized()) {
            res.status = 500;
            j["error"] = "lcd_not_initialized";
            res.set_content(j.dump(), "application/json");
            return;
        }

        lcd_.setScreenModeClock();

        j["result"] = "display_clock_enabled";
        j["mode"] = "clock";
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/display/backlight/on", [&](const Request&, Response& res) {

        json j;

        if (!lcd_.isInitialized()) {
            res.status = 500;
            j["error"] = "lcd_not_initialized";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!lcd_.backlightOn()) {
            res.status = 500;
            j["error"] = "lcd_backlight_on_failed";
            res.set_content(j.dump(), "application/json");
            return;
        }

        j["result"] = "display_backlight_on";
        j["backlightOn"] = true;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/display/backlight/off", [&](const Request&, Response& res) {

        json j;

        if (!lcd_.isInitialized()) {
            res.status = 500;
            j["error"] = "lcd_not_initialized";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!lcd_.backlightOff()) {
            res.status = 500;
            j["error"] = "lcd_backlight_off_failed";
            res.set_content(j.dump(), "application/json");
            return;
        }

        j["result"] = "display_backlight_off";
        j["backlightOn"] = false;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get(R"(/api/display/backlight/rule/(\d+)/set)", [&](const Request& req, Response& res) {

        json j;
        int ruleId = 0;

        if (!parseInt(req.matches[1], ruleId)) {
            res.status = 400;
            j["error"] = "invalid_backlight_rule_id";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (ruleId < 1 || ruleId > Scheduler::BACKLIGHT_RULE_COUNT) {
            res.status = 400;
            j["error"] = "invalid_backlight_rule_id";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!req.has_param("enabled") ||
            !req.has_param("start_hour") ||
            !req.has_param("start_minute") ||
            !req.has_param("end_hour") ||
            !req.has_param("end_minute")) {
            res.status = 400;
            j["error"] = "missing_params";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int enabled = 0;
        int startHour = 0;
        int startMinute = 0;
        int endHour = 0;
        int endMinute = 0;

        if (!parseInt(req.get_param_value("enabled"), enabled) ||
            !parseInt(req.get_param_value("start_hour"), startHour) ||
            !parseInt(req.get_param_value("start_minute"), startMinute) ||
            !parseInt(req.get_param_value("end_hour"), endHour) ||
            !parseInt(req.get_param_value("end_minute"), endMinute)) {
            res.status = 400;
            j["error"] = "invalid_params";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!validateBacklightHourMinute(startHour, startMinute, res, j) ||
            !validateBacklightHourMinute(endHour, endMinute, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.configureBacklightRule(ruleId,
                                          enabled != 0,
                                          startHour,
                                          startMinute,
                                          endHour,
                                          endMinute);

        j["result"] = "display_backlight_rule_set";
        j["id"] = ruleId;
        j["enabled"] = (enabled != 0);
        j["startHour"] = startHour;
        j["startMinute"] = startMinute;
        j["endHour"] = endHour;
        j["endMinute"] = endMinute;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/display/clock_text/set", [&](const Request& req, Response& res) {

        json j;

        if (!lcd_.isInitialized()) {
            res.status = 500;
            j["error"] = "lcd_not_initialized";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!req.has_param("text")) {
            res.status = 400;
            j["error"] = "missing_text";
            res.set_content(j.dump(), "application/json");
            return;
        }

        const std::string text = req.get_param_value("text");

        lcd_.setClockBottomText(text);

        j["result"] = "display_clock_text_set";
        j["clockBottomText"] = text;
        res.set_content(j.dump(), "application/json");
    });

    // ================= OUTPUT ON =================

    svr.Get(R"(/api/output/(\d+)/on)",
        [&](const Request& req, Response& res) {

        json j;
        int id = 0;

        if (!parseInt(req.matches[1], id)) {
            res.status = 400;
            j["error"] = "invalid_output_id";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!validateOutputId(id, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setOutputManualOn(id);

        j["result"] = "on";
        j["id"] = id;
        res.set_content(j.dump(), "application/json");
    });

    // ================= OUTPUT OFF =================

    svr.Get(R"(/api/output/(\d+)/off)",
        [&](const Request& req, Response& res) {

        json j;
        int id = 0;

        if (!parseInt(req.matches[1], id)) {
            res.status = 400;
            j["error"] = "invalid_output_id";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!validateOutputId(id, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setOutputManualOff(id);

        j["result"] = "off";
        j["id"] = id;
        res.set_content(j.dump(), "application/json");
    });

    // ================= OUTPUTS SET ALL ON =================

    svr.Get("/api/outputs/on_all",
        [&](const Request&, Response& res) {

        scheduler_.setAllOutputsManualOn();

        json j;
        j["result"] = "on_all";
        j["count"] = OUTPUT_COUNT;
        res.set_content(j.dump(), "application/json");
    });

    // ================= OUTPUTS SET ALL OFF =================

    svr.Get("/api/outputs/off_all",
        [&](const Request&, Response& res) {

        scheduler_.setAllOutputsManualOff();

        json j;
        j["result"] = "off_all";
        j["count"] = OUTPUT_COUNT;
        res.set_content(j.dump(), "application/json");
    });

    // ================= OUTPUTS AUTO ON/OFF ALL =================

    svr.Get("/api/outputs/autoon_all",
        [&](const Request& req, Response& res) {

        json j;

        if (!req.has_param("hour")) {
            res.status = 400;
            j["error"] = "missing_hour";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int hour = 0;
        if (!parseInt(req.get_param_value("hour"), hour)) {
            res.status = 400;
            j["error"] = "invalid_hour";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int minute = 0;
        if (req.has_param("minute")) {
            if (!parseInt(req.get_param_value("minute"), minute)) {
                res.status = 400;
                j["error"] = "invalid_minute";
                res.set_content(j.dump(), "application/json");
                return;
            }
        }

        if (!validateHourMinute(hour, minute, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setAllOutputsAutoOnTime(hour, minute);

        j["result"] = "autoon_all_set";
        j["count"] = OUTPUT_COUNT;
        j["hour"] = hour;
        j["minute"] = minute;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/outputs/autooff_all",
        [&](const Request& req, Response& res) {

        json j;

        if (!req.has_param("hour")) {
            res.status = 400;
            j["error"] = "missing_hour";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int hour = 0;
        if (!parseInt(req.get_param_value("hour"), hour)) {
            res.status = 400;
            j["error"] = "invalid_hour";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int minute = 0;
        if (req.has_param("minute")) {
            if (!parseInt(req.get_param_value("minute"), minute)) {
                res.status = 400;
                j["error"] = "invalid_minute";
                res.set_content(j.dump(), "application/json");
                return;
            }
        }

        if (!validateHourMinute(hour, minute, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setAllOutputsAutoOffTime(hour, minute);

        j["result"] = "autooff_all_set";
        j["count"] = OUTPUT_COUNT;
        j["hour"] = hour;
        j["minute"] = minute;
        res.set_content(j.dump(), "application/json");
    });

    // ================= AUTO ON TIME =================
    // /api/output/1/autoon?hour=18
    // /api/output/1/autoon?hour=18&minute=5

    svr.Get(R"(/api/output/(\d+)/autoon)",
        [&](const Request& req, Response& res) {

        json j;
        int id = 0;

        if (!parseInt(req.matches[1], id)) {
            res.status = 400;
            j["error"] = "invalid_output_id";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!validateOutputId(id, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!req.has_param("hour")) {
            res.status = 400;
            j["error"] = "missing_hour";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int hour = 0;
        if (!parseInt(req.get_param_value("hour"), hour)) {
            res.status = 400;
            j["error"] = "invalid_hour";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int minute = 0;
        if (req.has_param("minute")) {
            if (!parseInt(req.get_param_value("minute"), minute)) {
                res.status = 400;
                j["error"] = "invalid_minute";
                res.set_content(j.dump(), "application/json");
                return;
            }
        }

        if (!validateHourMinute(hour, minute, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setAutoOnTime(id, hour, minute);

        j["result"] = "autoon_set";
        j["id"] = id;
        j["hour"] = hour;
        j["minute"] = minute;
        res.set_content(j.dump(), "application/json");
    });

    // ================= AUTO OFF TIME =================
    // /api/output/1/autooff?hour=22
    // /api/output/1/autooff?hour=22&minute=15

    svr.Get(R"(/api/output/(\d+)/autooff)",
        [&](const Request& req, Response& res) {

        json j;
        int id = 0;

        if (!parseInt(req.matches[1], id) || !validateOutputId(id, res, j)) {
            res.status = 400;
            j["error"] = "invalid_output_id";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!req.has_param("hour")) {
            res.status = 400;
            j["error"] = "missing_hour";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int hour = 0;
        if (!parseInt(req.get_param_value("hour"), hour)) {
            res.status = 400;
            j["error"] = "invalid_hour";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int minute = 0;
        if (req.has_param("minute")) {
            if (!parseInt(req.get_param_value("minute"), minute)) {
                res.status = 400;
                j["error"] = "invalid_minute";
                res.set_content(j.dump(), "application/json");
                return;
            }
        }

        if (!validateHourMinute(hour, minute, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setAutoOffTime(id, hour, minute);

        j["result"] = "autooff_set";
        j["id"] = id;
        j["hour"] = hour;
        j["minute"] = minute;
        res.set_content(j.dump(), "application/json");
    });

    // ================= MANUAL MODE ON/OFF =================

    svr.Get(R"(/api/output/(\d+)/manual/on)",
        [&](const Request& req, Response& res) {

        json j;
        int id = 0;

        if (!parseInt(req.matches[1], id) || !validateOutputId(id, res, j)) {
            res.status = 400;
            j["error"] = "invalid_output_id";
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setManualMode(id, true);

        j["result"] = "manual_enabled";
        j["id"] = id;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get(R"(/api/output/(\d+)/manual/off)",
        [&](const Request& req, Response& res) {

        json j;
        int id = 0;

        if (!parseInt(req.matches[1], id) || !validateOutputId(id, res, j)) {
            res.status = 400;
            j["error"] = "invalid_output_id";
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setManualMode(id, false);

        j["result"] = "manual_disabled";
        j["id"] = id;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/outputs/manual/on_all",
        [&](const Request&, Response& res) {

        scheduler_.setAllManualMode(true);

        json j;
        j["result"] = "manual_enabled_all";
        j["count"] = OUTPUT_COUNT;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/outputs/manual/off_all",
        [&](const Request&, Response& res) {

        scheduler_.setAllManualMode(false);

        json j;
        j["result"] = "manual_disabled_all";
        j["count"] = OUTPUT_COUNT;
        res.set_content(j.dump(), "application/json");
    });

    // ================= GLOBAL AUTO ON/OFF =================

    svr.Get("/api/system/auto/on",
        [&](const Request&, Response& res) {

        scheduler_.setGlobalAutoMode(true);

        json j;
        j["result"] = "global_auto_enabled";
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/system/auto/off",
        [&](const Request&, Response& res) {

        scheduler_.setGlobalAutoMode(false);

        json j;
        j["result"] = "global_auto_disabled";
        res.set_content(j.dump(), "application/json");
    });

    // ================= SOUND AUTO ON/OFF =================

    svr.Get("/api/sound/auto/on",
        [&](const Request&, Response& res) {

        scheduler_.setGlobalSoundAutoMode(true);

        json j;
        j["result"] = "global_sound_auto_enabled";
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/sound/auto/off",
        [&](const Request&, Response& res) {

        scheduler_.setGlobalSoundAutoMode(false);

        json j;
        j["result"] = "global_sound_auto_disabled";
        res.set_content(j.dump(), "application/json");
    });

    // ================= SOUND PLAYBACK MODE =================

    svr.Get("/api/sound/mode/multiple/on",
        [&](const Request&, Response& res) {

        scheduler_.setSoundMultipleMode(true);

        json j;
        j["result"] = "sound_multiple_enabled";
        j["soundMultipleMode"] = true;
        j["activeSoundCount"] = scheduler_.getActiveSoundCount();
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/sound/mode/multiple/off",
        [&](const Request&, Response& res) {

        scheduler_.setSoundMultipleMode(false);

        json j;
        j["result"] = "sound_multiple_disabled";
        j["soundMultipleMode"] = false;
        j["activeSoundCount"] = scheduler_.getActiveSoundCount();
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/sound/mode/set",
        [&](const Request& req, Response& res) {

        json j;

        if (!req.has_param("multiple")) {
            res.status = 400;
            j["error"] = "missing_multiple";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int multiple = 0;
        if (!parseInt(req.get_param_value("multiple"), multiple)) {
            res.status = 400;
            j["error"] = "invalid_multiple";
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setSoundMultipleMode(multiple != 0);

        j["result"] = "sound_mode_set";
        j["soundMultipleMode"] = (multiple != 0);
        j["activeSoundCount"] = scheduler_.getActiveSoundCount();
        res.set_content(j.dump(), "application/json");
    });

    // ================= SOUND: GLOBAL VOLUME =================
    // GET /api/sound/volume/get
    // GET /api/sound/volume/set?value=70

    svr.Get("/api/sound/volume/get",
        [&](const Request&, Response& res) {

        json j;
        j["globalSoundVolume"] = scheduler_.getGlobalSoundVolume();
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/sound/volume/set",
        [&](const Request& req, Response& res) {

        json j;

        if (!req.has_param("value")) {
            res.status = 400;
            j["error"] = "missing_value";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int v = 0;
        if (!parseInt(req.get_param_value("value"), v)) {
            res.status = 400;
            j["error"] = "invalid_value";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!validateVolume(v, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        std::cout << "[HTTP] /api/sound/volume/set value=" << v << std::endl;
        scheduler_.setGlobalSoundVolume(v);

        j["result"] = "global_volume_set";
        j["value"] = v;
        res.set_content(j.dump(), "application/json");
    });

    // ================= SOUND FILES / DIAGNOSTICS =================

    svr.Get("/api/sound/files",
        [&](const Request&, Response& res) {

        json j;
        j["files"] = json::array();

        for (const auto& file : scheduler_.getAvailableSoundFiles()) {
            j["files"].push_back(file);
        }

        j["count"] = j["files"].size();
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/sound/diag",
        [&](const Request&, Response& res) {

        std::error_code ec;
        json j;

        j["soundBaseDir"] = SOUND_BASE_DIR.string();
        j["baseDirExists"] = fs::exists(SOUND_BASE_DIR, ec);
        ec.clear();
        j["baseDirIsDirectory"] = fs::is_directory(SOUND_BASE_DIR, ec);
        ec.clear();
        j["baseDirReadable"] = (access(SOUND_BASE_DIR.c_str(), R_OK | X_OK) == 0);
        j["uid"] = static_cast<int>(getuid());
        j["euid"] = static_cast<int>(geteuid());
        j["gid"] = static_cast<int>(getgid());
        j["egid"] = static_cast<int>(getegid());
        j["PATH"] = std::getenv("PATH") ? std::getenv("PATH") : "";
        j["players"] = {
            {"ffplay", executableExists("ffplay")},
            {"mpv", executableExists("mpv")},
            {"play", executableExists("play")},
            {"paplay", executableExists("paplay")},
            {"aplay", executableExists("aplay")},
            {"mpg123", executableExists("mpg123")}
        };

        auto files = scheduler_.getAvailableSoundFiles();
        j["availableFileCount"] = files.size();
        j["activeSoundCount"] = scheduler_.getActiveSoundCount();
        j["soundMultipleMode"] = scheduler_.isSoundMultipleMode();
        j["globalSoundVolume"] = scheduler_.getGlobalSoundVolume();

        res.set_content(j.dump(), "application/json");
    });

    // ================= SOUND: PLAY NOW =================
    // Variant A: with volume
    // Variant B: without volume uses current global volume

    svr.Get("/api/sound/play",
        [&](const Request& req, Response& res) {

        json j;

        if (!req.has_param("file")) {
            res.status = 400;
            j["error"] = "missing_file";
            res.set_content(j.dump(), "application/json");
            return;
        }

        std::string file = req.get_param_value("file");

        if (!req.has_param("volume")) {
            res.status = 400;
            j["error"] = "missing_volume";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int v = 0;
        if (!parseInt(req.get_param_value("volume"), v)) {
            res.status = 400;
            j["error"] = "invalid_volume";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!validateVolume(v, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.playSoundNow(file, v);

        j["result"] = "play";
        j["file"] = file;
        j["volume"] = v;
        j["soundMultipleMode"] = scheduler_.isSoundMultipleMode();
        j["activeSoundCount"] = scheduler_.getActiveSoundCount();
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/sound/play_current",
        [&](const Request& req, Response& res) {

        json j;

        if (!req.has_param("file")) {
            res.status = 400;
            j["error"] = "missing_file";
            res.set_content(j.dump(), "application/json");
            return;
        }

        std::string file = req.get_param_value("file");

        scheduler_.playSoundNow(file, 100);

        j["result"] = "play_current";
        j["file"] = file;
        j["volume"] = scheduler_.getGlobalSoundVolume();
        j["soundMultipleMode"] = scheduler_.isSoundMultipleMode();
        j["activeSoundCount"] = scheduler_.getActiveSoundCount();
        res.set_content(j.dump(), "application/json");
    });

    // ================= SOUND STATUS =================

    svr.Get("/api/sound/status",
        [&](const Request&, Response& res) {

        json j;
        j["globalSoundVolume"] = scheduler_.getGlobalSoundVolume();
        j["globalSoundAutoMode"] = scheduler_.isGlobalSoundAutoMode();
        j["soundMultipleMode"] = scheduler_.isSoundMultipleMode();
        j["activeSoundCount"] = scheduler_.getActiveSoundCount();
        j["availableFiles"] = json::array();
        for (const auto& file : scheduler_.getAvailableSoundFiles()) {
            j["availableFiles"].push_back(file);
        }
        j["soundAlarms"] = json::array();

        for (int s = 1; s <= SOUND_SLOT_COUNT; ++s) {
            auto snap = scheduler_.getSoundAlarmSnapshot(s);

            j["soundAlarms"].push_back({
                {"slot", s},
                {"enabled", snap.enabled},
                {"hour", snap.hour},
                {"minute", snap.minute},
                {"file", snap.file},
                {"volume", snap.volume}
            });
        }

        res.set_content(j.dump(), "application/json");
    });

    // ================= SOUND ALARM SET =================

    svr.Get(R"(/api/sound/alarm/(\d+)/set)",
        [&](const Request& req, Response& res) {

        json j;
        int slot = 0;

        if (!parseInt(req.matches[1], slot)) {
            res.status = 400;
            j["error"] = "invalid_sound_slot";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!validateSoundSlot(slot, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!req.has_param("hour") || !req.has_param("minute") || !req.has_param("file")) {
            res.status = 400;
            j["error"] = "missing_params";
            res.set_content(j.dump(), "application/json");
            return;
        }

        int hour = 0;
        int minute = 0;

        if (!parseInt(req.get_param_value("hour"), hour) ||
            !parseInt(req.get_param_value("minute"), minute)) {
            res.status = 400;
            j["error"] = "invalid_time";
            res.set_content(j.dump(), "application/json");
            return;
        }

        if (!validateHourMinute(hour, minute, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        std::string file = req.get_param_value("file");

        int volume = 100;
        if (req.has_param("volume")) {
            if (!parseInt(req.get_param_value("volume"), volume)) {
                res.status = 400;
                j["error"] = "invalid_volume";
                res.set_content(j.dump(), "application/json");
                return;
            }
        }

        if (!validateVolume(volume, res, j)) {
            res.set_content(j.dump(), "application/json");
            return;
        }

        bool enabled = true;
        if (req.has_param("enabled")) {
            int en = 1;
            if (!parseInt(req.get_param_value("enabled"), en)) {
                res.status = 400;
                j["error"] = "invalid_enabled";
                res.set_content(j.dump(), "application/json");
                return;
            }
            enabled = (en != 0);
        }

        scheduler_.configureSoundAlarm(slot, enabled, hour, minute, file, volume);

        j["result"] = "sound_alarm_set";
        j["slot"] = slot;
        j["enabled"] = enabled;
        j["hour"] = hour;
        j["minute"] = minute;
        j["file"] = file;
        j["volume"] = volume;

        res.set_content(j.dump(), "application/json");
    });

    // ================= SOUND ALARM ENABLE/DISABLE =================

    svr.Get(R"(/api/sound/alarm/(\d+)/enable)",
        [&](const Request& req, Response& res) {

        json j;
        int slot = 0;

        if (!parseInt(req.matches[1], slot) || !validateSoundSlot(slot, res, j)) {
            res.status = 400;
            j["error"] = "invalid_sound_slot";
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setSoundAlarmEnabled(slot, true);

        j["result"] = "sound_alarm_enabled";
        j["slot"] = slot;
        res.set_content(j.dump(), "application/json");
    });

    svr.Get(R"(/api/sound/alarm/(\d+)/disable)",
        [&](const Request& req, Response& res) {

        json j;
        int slot = 0;

        if (!parseInt(req.matches[1], slot) || !validateSoundSlot(slot, res, j)) {
            res.status = 400;
            j["error"] = "invalid_sound_slot";
            res.set_content(j.dump(), "application/json");
            return;
        }

        scheduler_.setSoundAlarmEnabled(slot, false);

        j["result"] = "sound_alarm_disabled";
        j["slot"] = slot;
        res.set_content(j.dump(), "application/json");
    });

    // ================= STATIC =================

    if (!svr.set_mount_point("/", www_dir_)) {
        std::cerr << "ERROR: www directory not found: "
                  << www_dir_ << "\n";
        return;
    }

    std::cout << "HTTP server listening on "
              << bind_address_ << ":" << port_ << "\n";

    svr.listen(bind_address_.c_str(), port_);
}

// end of file: src/httpserver.cpp
