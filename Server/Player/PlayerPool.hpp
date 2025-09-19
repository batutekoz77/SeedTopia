#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>
#include <enet/enet.h>

struct Player {
    uintptr_t id;
    ENetPeer* peer = nullptr;
    uint16_t subserverID = 0;

    std::string ip;
    std::string hwid;

    std::string mail;
    std::string username;
    std::string password;

    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastMailTime;
};

class PlayerPool {
public:
    PlayerPool() = default;

    void AddPlayer(uintptr_t id, ENetPeer* peer = nullptr, uint16_t subserverID = 0, const std::string& ip = "", const std::string& hwid = "", const std::string& mail = "", const std::string& username = "", const std::string& password = "", std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastMailTime = {});

    void RemovePlayer(uintptr_t id);

    Player* GetPlayer(uintptr_t id);

    bool HasPlayer(uintptr_t id);

    std::vector<Player> GetAllPlayers();

    size_t Count();

    template<typename Func>
    void UpdatePlayer(uintptr_t id, Func updater) {
        std::lock_guard<std::mutex> lock(poolMutex);
        auto it = players.find(id);
        if (it != players.end()) {
            updater(it->second);
        }
    }

private:
    std::unordered_map<uintptr_t, Player> players;
    mutable std::mutex poolMutex;
};