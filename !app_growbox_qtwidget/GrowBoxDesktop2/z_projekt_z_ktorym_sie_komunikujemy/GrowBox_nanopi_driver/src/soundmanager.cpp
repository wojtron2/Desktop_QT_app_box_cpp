//start of file soundmanager.cpp

#include "soundmanager.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace
{
    const fs::path kSoundBaseDir("/home/w/snd");
    constexpr int kMaxSimultaneousSounds = 3;

    std::string ToLowerCopy(const std::string& s)
    {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    bool HasPathSeparator(const std::string& s)
    {
        return s.find('/') != std::string::npos || s.find('\\') != std::string::npos;
    }

    bool ContainsParentTraversal(const fs::path& p)
    {
        for (const auto& part : p) {
            if (part == "..") {
                return true;
            }
        }
        return false;
    }

    bool IsSupportedAudioExtension(const fs::path& p)
    {
        const std::string ext = ToLowerCopy(p.extension().string());
        return ext == ".wav" || ext == ".wave" || ext == ".mp3";
    }

    bool PathStartsWith(const fs::path& base, const fs::path& value)
    {
        auto itBase = base.begin();
        auto itValue = value.begin();

        for (; itBase != base.end(); ++itBase, ++itValue) {
            if (itValue == value.end() || *itBase != *itValue) {
                return false;
            }
        }

        return true;
    }

    bool ExecutableExists(const std::string& name)
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

    std::string ResolveSoundPath(const std::string& name)
    {
        if (name.empty()) {
            return "";
        }

        std::error_code ec;

        if (!fs::exists(kSoundBaseDir, ec) || !fs::is_directory(kSoundBaseDir, ec)) {
            std::cerr << "[SoundManager] sound base dir not found: "
                      << kSoundBaseDir << std::endl;
            return "";
        }

        fs::path baseCanonical = fs::weakly_canonical(kSoundBaseDir, ec);
        if (ec) {
            std::cerr << "[SoundManager] cannot canonicalize base dir: "
                      << kSoundBaseDir << " err=" << ec.message() << std::endl;
            return "";
        }

        fs::path requested(name);

        if (requested.is_absolute()) {
            fs::path reqCanonical = fs::weakly_canonical(requested, ec);
            if (ec) {
                std::cerr << "[SoundManager] cannot canonicalize requested file: "
                          << requested << " err=" << ec.message() << std::endl;
                return "";
            }

            if (!PathStartsWith(baseCanonical, reqCanonical)) {
                std::cerr << "[SoundManager] rejected path outside sound dir: "
                          << reqCanonical << std::endl;
                return "";
            }

            if (!fs::exists(reqCanonical, ec) ||
                !fs::is_regular_file(reqCanonical, ec) ||
                !IsSupportedAudioExtension(reqCanonical)) {
                return "";
            }

            return reqCanonical.string();
        }

        if (ContainsParentTraversal(requested)) {
            std::cerr << "[SoundManager] rejected relative path with .. : "
                      << requested << std::endl;
            return "";
        }

        fs::path directPath = kSoundBaseDir / requested;
        fs::path directCanonical = fs::weakly_canonical(directPath, ec);

        if (!ec &&
            PathStartsWith(baseCanonical, directCanonical) &&
            fs::exists(directCanonical, ec) &&
            fs::is_regular_file(directCanonical, ec) &&
            IsSupportedAudioExtension(directCanonical)) {
            return directCanonical.string();
        }

        ec.clear();

        if (!HasPathSeparator(name)) {
            for (fs::recursive_directory_iterator it(
                     kSoundBaseDir,
                     fs::directory_options::skip_permission_denied,
                     ec);
                 !ec && it != fs::recursive_directory_iterator();
                 ++it) {

                if (!it->is_regular_file(ec)) {
                    ec.clear();
                    continue;
                }

                const fs::path candidate = it->path();

                if (candidate.filename() == requested.filename() &&
                    IsSupportedAudioExtension(candidate)) {
                    return candidate.string();
                }
            }
        }

        return "";
    }

    std::string VolumeScale(int volume)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2)
           << (static_cast<double>(volume) / 100.0);
        return ss.str();
    }

    std::string PulseVolumeScale(int volume)
    {
        const int pulseScale = (volume * 65536) / 100;
        return std::to_string(pulseScale);
    }

    std::vector<std::string> BuildPlayerArgs(const std::string& resolvedPath,
                                             const std::string& ext,
                                             int effectiveVolume)
    {
        if (ExecutableExists("ffplay")) {
            return {
                "ffplay",
                "-nodisp",
                "-autoexit",
                "-loglevel", "quiet",
                "-volume", std::to_string(effectiveVolume),
                resolvedPath
            };
        }

        if (ExecutableExists("mpv")) {
            return {
                "mpv",
                "--no-video",
                "--really-quiet",
                "--no-terminal",
                "--audio-display=no",
                "--volume=" + std::to_string(effectiveVolume),
                resolvedPath
            };
        }

        if (ext == ".mp3" && ExecutableExists("mpg123")) {
            int mpg123Scale = (effectiveVolume * 32768) / 100;
            if (mpg123Scale < 1) {
                mpg123Scale = 1;
            }

            return {
                "mpg123",
                "-q",
                "-f",
                std::to_string(mpg123Scale),
                resolvedPath
            };
        }

        if ((ext == ".wav" || ext == ".wave") && ExecutableExists("play")) {
            return {
                "play",
                "-q",
                "-v",
                VolumeScale(effectiveVolume),
                resolvedPath
            };
        }

        if (ExecutableExists("paplay")) {
            return {
                "paplay",
                "--volume=" + PulseVolumeScale(effectiveVolume),
                resolvedPath
            };
        }

        if ((ext == ".wav" || ext == ".wave") && ExecutableExists("aplay")) {
            std::cerr << "[SoundManager] WARNING: using aplay fallback; "
                      << "per-file volume is not supported by aplay. "
                      << "Install ffplay, mpv, sox/play or paplay for volume control."
                      << std::endl;
            return {
                "aplay",
                "-q",
                resolvedPath
            };
        }

        return {};
    }

    pid_t StartProcess(const std::vector<std::string>& args)
    {
        if (args.empty()) {
            return -1;
        }

        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "[SoundManager] fork failed: "
                      << std::strerror(errno) << std::endl;
            return -1;
        }

        if (pid == 0) {
            std::vector<char*> argv;
            argv.reserve(args.size() + 1);

            for (const auto& a : args) {
                argv.push_back(const_cast<char*>(a.c_str()));
            }

            argv.push_back(nullptr);
            execvp(argv[0], argv.data());

            std::cerr << "[SoundManager] exec failed: "
                      << args[0]
                      << " err=" << std::strerror(errno)
                      << std::endl;
            _exit(127);
        }

        return pid;
    }
}

SoundManager::SoundManager() {}
SoundManager::~SoundManager() { stop(); }

int SoundManager::clamp100(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

void SoundManager::setGlobalVolume(int volume) {
    const int v = clamp100(volume);
    globalVolume = v;
    std::cout << "[SoundManager] globalVolume=" << v << std::endl;
}

int SoundManager::getGlobalVolume() const {
    return globalVolume.load();
}

void SoundManager::setMultipleMode(bool enabled)
{
    multipleMode = enabled;
    std::lock_guard<std::mutex> lock(mtx);
    pruneFinishedLocked();

    if (!enabled && activeSounds.size() > 1) {
        while (activeSounds.size() > 1) {
            stopActiveLocked(activeSounds.front(), "switch_to_single_mode");
            activeSounds.erase(activeSounds.begin());
        }
    }

    std::cout << "[SoundManager] multipleMode="
              << (enabled ? "on" : "off") << std::endl;
}

bool SoundManager::isMultipleMode() const
{
    return multipleMode.load();
}

int SoundManager::activeCount()
{
    std::lock_guard<std::mutex> lock(mtx);
    pruneFinishedLocked();
    return static_cast<int>(activeSounds.size());
}

std::vector<std::string> SoundManager::listAvailableSounds() const
{
    std::vector<std::string> files;
    std::error_code ec;

    if (!fs::exists(kSoundBaseDir, ec) || !fs::is_directory(kSoundBaseDir, ec)) {
        return files;
    }

    for (fs::recursive_directory_iterator it(kSoundBaseDir,
                                             fs::directory_options::skip_permission_denied,
                                             ec);
         !ec && it != fs::recursive_directory_iterator();
         ++it) {
        if (!it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        const fs::path p = it->path();
        if (!IsSupportedAudioExtension(p)) {
            continue;
        }

        fs::path rel = fs::relative(p, kSoundBaseDir, ec);
        if (ec) {
            ec.clear();
            continue;
        }

        std::string value = rel.generic_string();
        files.push_back(value);
    }

    std::sort(files.begin(), files.end());
    return files;
}

const char* SoundManager::backendName()
{
    return "process-audio";
}

void SoundManager::start() {
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true)) {
        return;
    }

    std::cout << "[SoundManager] backend=" << backendName()
              << " base_dir=" << kSoundBaseDir
              << " ffplay=" << (ExecutableExists("ffplay") ? "yes" : "no")
              << " mpv=" << (ExecutableExists("mpv") ? "yes" : "no")
              << " play=" << (ExecutableExists("play") ? "yes" : "no")
              << " paplay=" << (ExecutableExists("paplay") ? "yes" : "no")
              << " aplay=" << (ExecutableExists("aplay") ? "yes" : "no")
              << " mpg123=" << (ExecutableExists("mpg123") ? "yes" : "no")
              << std::endl;
}

void SoundManager::stop() {
    bool expected = true;
    if (!running.compare_exchange_strong(expected, false)) {
        return;
    }

    std::lock_guard<std::mutex> lock(mtx);
    std::queue<SoundItem> empty;
    std::swap(soundQueue, empty);
    stopAllActiveLocked("sound_manager_stop");
}

void SoundManager::play(const std::string& name) {
    play(name, 100);
}

void SoundManager::play(const std::string& name, int volume) {
    volume = clamp100(volume);

    if (name.empty()) {
        std::cerr << "[SoundManager] play ignored: empty file name" << std::endl;
        return;
    }

    if (!running.load()) {
        std::cerr << "[SoundManager] play ignored: manager is not running" << std::endl;
        return;
    }

    const int gv = globalVolume.load();
    const int effective = (volume * gv) / 100;

    std::cout << "[SoundManager] queue play file=" << name
              << " requestedVolume=" << volume
              << " globalVolume=" << gv
              << " effectiveVolume=" << effective
              << " multipleMode=" << (multipleMode.load() ? "on" : "off")
              << std::endl;

    std::lock_guard<std::mutex> lock(mtx);
    pruneFinishedLocked();

    if (!multipleMode.load()) {
        stopAllActiveLocked("single_mode_new_sound");
    } else {
        while (activeSounds.size() >= kMaxSimultaneousSounds) {
            stopActiveLocked(activeSounds.front(), "multiple_mode_limit");
            activeSounds.erase(activeSounds.begin());
        }
    }

    const pid_t pid = startBackendProcess(name, effective);
    if (pid > 0) {
        activeSounds.push_back({pid, name, effective});
    }
}

pid_t SoundManager::startBackendProcess(const std::string& name, int effectiveVolume)
{
    const std::string resolvedPath = ResolveSoundPath(name);

    if (resolvedPath.empty()) {
        std::cerr << "[SoundManager] file not found in /home/w/snd: "
                  << name << std::endl;
        return -1;
    }

    fs::path p(resolvedPath);
    const std::string ext = ToLowerCopy(p.extension().string());

    std::cout << "[SoundManager] play request file=" << name
              << " resolved=" << resolvedPath
              << " effectiveVolume=" << effectiveVolume
              << " mode=" << (multipleMode.load() ? "multiple" : "single")
              << std::endl;

    if (effectiveVolume <= 0) {
        std::cout << "[SoundManager] muted, skip file: "
                  << resolvedPath << std::endl;
        return -1;
    }

    std::vector<std::string> args = BuildPlayerArgs(resolvedPath, ext, effectiveVolume);
    if (args.empty()) {
        std::cerr << "[SoundManager] no supported player found for "
                  << resolvedPath
                  << ". Install ffplay, mpv, sox/play, paplay, mpg123 or aplay."
                  << std::endl;
        return -1;
    }

    std::cout << "[SoundManager] starting player=" << args[0]
              << " volume=" << effectiveVolume << std::endl;

    const pid_t pid = StartProcess(args);
    if (pid > 0) {
        std::cout << "[SoundManager] started pid=" << pid << std::endl;
    }

    return pid;
}

void SoundManager::stopActiveLocked(ActiveSound& active, const char* reason)
{
    if (active.pid <= 0) {
        return;
    }

    std::cout << "[SoundManager] stopping pid=" << active.pid
              << " file=" << active.name
              << " reason=" << reason << std::endl;

    if (kill(active.pid, SIGTERM) != 0 && errno != ESRCH) {
        std::cerr << "[SoundManager] kill failed pid=" << active.pid
                  << " err=" << std::strerror(errno) << std::endl;
    }

    for (int attempt = 0; attempt < 10; ++attempt) {
        int status = 0;
        const pid_t result = waitpid(active.pid, &status, WNOHANG);
        if (result == active.pid || (result < 0 && errno == ECHILD)) {
            active.pid = -1;
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    std::cerr << "[SoundManager] pid=" << active.pid
              << " did not stop after SIGTERM, sending SIGKILL"
              << std::endl;

    if (kill(active.pid, SIGKILL) != 0 && errno != ESRCH) {
        std::cerr << "[SoundManager] SIGKILL failed pid=" << active.pid
                  << " err=" << std::strerror(errno) << std::endl;
    }

    int status = 0;
    if (waitpid(active.pid, &status, 0) == active.pid || errno == ECHILD) {
        active.pid = -1;
    }
}

void SoundManager::stopAllActiveLocked(const char* reason)
{
    for (auto& active : activeSounds) {
        stopActiveLocked(active, reason);
    }
    pruneFinishedLocked();
    activeSounds.clear();
}

void SoundManager::pruneFinishedLocked()
{
    auto it = activeSounds.begin();
    while (it != activeSounds.end()) {
        if (it->pid <= 0) {
            it = activeSounds.erase(it);
            continue;
        }

        int status = 0;
        const pid_t result = waitpid(it->pid, &status, WNOHANG);

        if (result == 0) {
            ++it;
            continue;
        }

        if (result == it->pid) {
            if (WIFEXITED(status)) {
                std::cout << "[SoundManager] pid=" << it->pid
                          << " finished exit=" << WEXITSTATUS(status)
                          << " file=" << it->name << std::endl;
            } else if (WIFSIGNALED(status)) {
                std::cout << "[SoundManager] pid=" << it->pid
                          << " killed signal=" << WTERMSIG(status)
                          << " file=" << it->name << std::endl;
            }
            it = activeSounds.erase(it);
            continue;
        }

        if (result < 0 && errno == ECHILD) {
            it = activeSounds.erase(it);
            continue;
        }

        ++it;
    }
}

//end of file soundmanager.cpp
