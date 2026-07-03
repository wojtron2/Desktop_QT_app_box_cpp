//start of file soundmanager.h

#pragma once

#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <sys/types.h>

class SoundManager {
public:
    SoundManager();
    ~SoundManager();

    void start();
    void stop();

    // master volume 0..100
    void setGlobalVolume(int volume);
    int  getGlobalVolume() const;

    void play(const std::string& name);
    void play(const std::string& name, int volume);

    void setMultipleMode(bool enabled);
    bool isMultipleMode() const;
    int  activeCount();

    std::vector<std::string> listAvailableSounds() const;

private:
    struct SoundItem {
        std::string name;
        int volume = 100; // per-play volume 0..100
    };

    struct ActiveSound {
        pid_t pid = -1;
        std::string name;
        int effectiveVolume = 100;
    };

    static int clamp100(int v);

    pid_t startBackendProcess(const std::string& name, int effectiveVolume);
    void stopActiveLocked(ActiveSound& active, const char* reason);
    void stopAllActiveLocked(const char* reason);
    void pruneFinishedLocked();
    static const char* backendName();

    std::queue<SoundItem> soundQueue;
    std::vector<ActiveSound> activeSounds;

    std::thread worker;
    std::mutex mtx;
    std::condition_variable cv;

    std::atomic<bool> running{false};
    std::atomic<int>  globalVolume{100};
    std::atomic<bool> multipleMode{false};
};

//end of file soundmanager.h
