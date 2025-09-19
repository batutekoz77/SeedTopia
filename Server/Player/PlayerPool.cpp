#include "PlayerPool.hpp"

void PlayerPool::AddPlayer(uintptr_t id, ENetPeer* peer, uint16_t subserverID, const std::string& ip, const std::string& hwid, const std::string& mail, const std::string& username, const std::string& password, std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastMailTime) {
    std::lock_guard<std::mutex> lock(poolMutex);
    players[id] = Player{ id, peer, subserverID, ip, hwid, mail, username, password, lastMailTime };
}

void PlayerPool::RemovePlayer(uintptr_t id) {
    std::lock_guard<std::mutex> lock(poolMutex);
    players.erase(id);
}

bool PlayerPool::HasPlayer(uintptr_t id) {
    std::lock_guard<std::mutex> lock(poolMutex);
    return players.find(id) != players.end();
}

Player* PlayerPool::GetPlayer(uintptr_t id) {
    std::lock_guard<std::mutex> lock(poolMutex);
    auto it = players.find(id);
    if (it != players.end()) return &it->second;
    return nullptr;
}

std::vector<Player> PlayerPool::GetAllPlayers() {
    std::lock_guard<std::mutex> lock(poolMutex);
    std::vector<Player> result;
    result.reserve(players.size());
    for (auto& kv : players) {
        result.push_back(kv.second);
    }
    return result;
}

size_t PlayerPool::Count() {
    std::lock_guard<std::mutex> lock(poolMutex);
    return players.size();
}