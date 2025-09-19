#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <mutex>
#include <chrono>

#include <fmt/core.h>
#include <fmt/color.h>

#include "JsonReader.hpp"
#include "Player/PlayerPool.hpp"
#include "Database/Database.hpp"
#include "Security/SendMail.h"

PlayerPool gPlayerPool;
Database securityDB("security.db");

std::mutex mailMutex;
const int MAIL_COOLDOWN = 5 * 60;

void EnableANSIColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut == INVALID_HANDLE_VALUE) return;

    if (!GetConsoleMode(hOut, &dwMode)) return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

namespace SubServer {
    struct ServerInfo {
        std::string IP;
        uint16_t UDP;
        size_t playerCount = 0;
    };

    inline std::vector<OptimisedValue<ServerInfo>> servers;
    inline std::mutex serversMutex;

    inline ServerInfo& GetLeastLoaded() {
        std::lock_guard<std::mutex> lock(serversMutex);
        if (servers.size() != 4) throw std::runtime_error("Subserver count must be 4!");

        ServerInfo* least = &*servers[0];
        if (servers[1]->playerCount < least->playerCount) least = &*servers[1];
        if (servers[2]->playerCount < least->playerCount) least = &*servers[2];
        if (servers[3]->playerCount < least->playerCount) least = &*servers[3];

        return *least;
    }
}

struct SubServerHost {
    OptimisedValue<SubServer::ServerInfo> info;
    ENetHost* host = nullptr;

    SubServerHost(const SubServer::ServerInfo& s, ENetHost* h)
        : info(s), host(h) {
    }
};

std::vector<SubServerHost> subserverHosts;

namespace Server {
    bool activated = false;
    std::string NAME;
    std::string IP;
    uint16_t UDP;
    uint16_t TCP;

    inline void LoadConfig(const std::string& file) {
        activated = true;

        NAME = JsonReader::readJson<std::string>(file, "Server_Name");
        IP = JsonReader::readJson<std::string>(file, "Server_IP");
        UDP = static_cast<uint16_t>(JsonReader::readJson<unsigned int>(file, "Server_UDP"));
        TCP = static_cast<uint16_t>(JsonReader::readJson<unsigned int>(file, "Server_TCP"));

        for (int i = 1; i <= 4; i++) {
            std::string key = "Server_Sub" + std::to_string(i);
            std::string subIP = JsonReader::readJson<std::string>(file, key);
            if (!subIP.empty()) {
                SubServer::servers.emplace_back(SubServer::ServerInfo{ subIP, UDP, 0 });
            }
        }

        SetConsoleTitleA(NAME.c_str());
        fmt::print(fmt::fg(fmt::color::green), "[SUCCESS] Loaded config file\n");
    }
}

namespace Enet {
    bool activated = false;
    ENetAddress address;
    ENetHost* server;

    inline void SetupMainServer() {
        if (!Server::activated) return;

        if (enet_initialize() != 0) {
            fmt::print(fmt::fg(fmt::color::red), "[ERROR] Failed to initialize ENet\n");
            activated = false;
            return;
        }

        if (enet_address_set_host(&address, Server::IP.c_str()) != 0) {
            fmt::print(fmt::fg(fmt::color::red), "[ERROR] Failed to set ENet host address\n");
            activated = false;
            return;
        }

        address.port = Server::UDP;
        server = enet_host_create(&address, 1024, 1, 0, 0);
        if (!server) {
            fmt::print(fmt::fg(fmt::color::red), "[ERROR] Failed to create ENet server\n");
            activated = false;
            return;
        }

        server->usingNewPacketForServer = 1;
        server->checksum = enet_crc32;
        enet_host_compress_with_range_coder(server);

        fmt::print(fmt::fg(fmt::color::green), "[SUCCESS] Main ENet server started on {}:{}\n", Server::IP, Server::UDP);
        activated = true;
    }

    inline void MainServerListener() {
        if (!activated) return;

        ENetEvent event;
        while (activated) {
            while (enet_host_service(server, &event, 1000) >= 0) {
                ENetPeer* peer = event.peer;
                switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    uintptr_t id = reinterpret_cast<uintptr_t>(peer);
                    fmt::print(fmt::fg(fmt::color::blue), "[MAIN] Player {} connected\n", id);

                    SubServer::ServerInfo* sub = nullptr;
                    {
                        std::lock_guard<std::mutex> lock(SubServer::serversMutex);
                        size_t minPlayers = SIZE_MAX;
                        for (auto& s : SubServer::servers) {
                            if (s->playerCount < minPlayers) {
                                minPlayers = s->playerCount;
                                sub = &*s;
                            }
                        }
                        if (sub) sub->playerCount++;
                    }

                    if (sub) {
                        std::string msg = "changesub\nip|" + sub->IP + "\nudp|" + std::to_string(sub->UDP) + "\n";
                        ENetPacket* packet = enet_packet_create(msg.c_str(), msg.size(), ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(peer, 0, packet);
                        enet_host_flush(server);
                    }
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT: {
                    uintptr_t id = reinterpret_cast<uintptr_t>(peer);
                    fmt::print(fmt::fg(fmt::color::blue), "[MAIN] Player {} disconnected\n", id);
                    break;
                }
                default: break;
                }
            }
        }
    }

    inline void SubServerListenerAll() {
        ENetEvent event;

        while (true) {
            for (auto& ssh : subserverHosts) {
                if (!ssh.host) continue;

                while (enet_host_service(ssh.host, &event, 1) > 0) {
                    ENetPeer* peer = event.peer;

                    switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT: {
                        uintptr_t id = reinterpret_cast<uintptr_t>(peer);
                        gPlayerPool.AddPlayer(id, peer, ssh.info->UDP);

                        fmt::print(fmt::fg(fmt::color::yellow), "[SUB] Player {} connected to {}:{}\n", id, ssh.info->IP, ssh.info->UDP);
                        break;
                    }
                    case ENET_EVENT_TYPE_RECEIVE: {
                        uintptr_t id = reinterpret_cast<uintptr_t>(peer);

                        if (!gPlayerPool.HasPlayer(id)) {
                            fmt::print(fmt::fg(fmt::color::red), "[SUB] Unknown player {} sent a packet, disconnecting!\n", id);
                            enet_peer_disconnect(peer, 0);
                            enet_packet_destroy(event.packet);
                            break;
                        }

                        std::string msg(reinterpret_cast<char*>(event.packet->data));
                        auto parsed = ParseMessage(msg);

                        if (parsed.command == "enter_game" && parsed.has("ip") && parsed.has("hwid")) {
                            gPlayerPool.UpdatePlayer(id, [&](Player& p) {
                                p.ip = parsed.get("ip");
                                p.hwid = parsed.get("hwid");
                            });

                            std::string msg = "changeUI\nmethod|LOGIN_REGISTER\n";
                            ENetPacket* packet = enet_packet_create(msg.c_str(), msg.size(), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(peer, 0, packet);
                            enet_host_flush(server);
                        }

                        else if (parsed.command == "login" && parsed.has("seedID") && parsed.has("pass")) {
                            std::string get_seedID = parsed.get("seedID");
                            std::string get_pass = parsed.get("pass");

                            std::string msg = "";
                            
                            if (securityDB.check_password(get_seedID, get_pass) == true) {
                                gPlayerPool.UpdatePlayer(id, [&](Player& p) {
                                    p.username = get_seedID;
                                    p.password = get_pass;
                                });
                                msg = "changeUI\nmethod|WORLD_MENU\n";
                            }
                            else msg = "error\nmethod|Username and Password don't match!\n";

                            ENetPacket* packet = enet_packet_create(msg.c_str(), msg.size(), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(peer, 0, packet);
                            enet_host_flush(server);
                        }
                        else if (parsed.command == "register" && parsed.has("mail") && parsed.has("seedID") && parsed.has("pass")) {
                            std::string get_mail = parsed.get("mail");
                            std::string get_seedID = parsed.get("seedID");
                            std::string get_pass = parsed.get("pass");

                            std::string msg = "";

                            if (securityDB.user_exists(get_seedID) == false) {
                                if (securityDB.mail_used(get_mail) == true) msg = "error\nmethod|There is already an account existing with the same mail!\n";
                                else {
                                    gPlayerPool.UpdatePlayer(id, [&](Player& p) {
                                        p.mail = get_mail;
                                        p.username = get_seedID;
                                        p.password = get_pass;
                                    });
                                    
                                    Player* player = gPlayerPool.GetPlayer(id);
                                    if (player) securityDB.add_user(player->mail, player->username, player->password, player->hwid);

                                    msg = "changeUI\nmethod|WORLD_MENU\n";
                                }
                            }
                            else msg = "error\nmethod|There is already an account existing with the same name!\n";

                            ENetPacket* packet = enet_packet_create(msg.c_str(), msg.size(), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(peer, 0, packet);
                            enet_host_flush(server);
                        }
                        else if (parsed.command == "forgot" && parsed.has("seedID")) {
                            std::string get_seedID = parsed.get("seedID");

                            std::string msg = "";

                            if (securityDB.user_exists(get_seedID) == true) {
                                std::string realMail = securityDB.get_mail_by_seedid(get_seedID);
                                if (realMail.length() >= 3) {
                                    using namespace std::chrono;
                                    auto now = steady_clock::now();
                                    bool canSend = false;

                                    {
                                        std::lock_guard<std::mutex> lock(mailMutex);

                                        Player* player = gPlayerPool.GetPlayer(id);
                                        if (player) {
                                            if (player->lastMailTime.find(get_seedID) != player->lastMailTime.end()) {
                                                auto elapsed = duration_cast<seconds>(now - player->lastMailTime[get_seedID]).count();
                                                if (elapsed < MAIL_COOLDOWN) {
                                                    int remaining = MAIL_COOLDOWN - elapsed;
                                                    int min = remaining / 60;
                                                    int sec = remaining % 60;
                                                    msg = "error\nmethod|Please wait " + std::to_string(min) + "m " + std::to_string(sec) + "s before sending again.\n";
                                                }
                                                else {
                                                    canSend = true;
                                                    player->lastMailTime[get_seedID] = now;
                                                }
                                            }
                                            else {
                                                canSend = true;
                                                player->lastMailTime[get_seedID] = now;
                                            }
                                        }
                                        
                                    }

                                    if (canSend) {
                                        std::string userPassword = securityDB.get_pass_by_seedid(get_seedID);

                                        std::thread([](std::string toMail, std::string accountPass) {
                                            sendMail(toMail, accountPass);
                                        }, realMail, userPassword).detach();
                                    }
                                }
                                else msg = "error\nmethod|Couldn't verify the mail of this user!\n";
                            }
                            else msg = "error\nmethod|There is no account with this name!\n";

                            ENetPacket* packet = enet_packet_create(msg.c_str(), msg.size(), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(peer, 0, packet);
                            enet_host_flush(server);
                        }

                        enet_packet_destroy(event.packet);
                        break;
                    }
                    case ENET_EVENT_TYPE_DISCONNECT: {
                        uintptr_t id = reinterpret_cast<uintptr_t>(peer);
                        gPlayerPool.RemovePlayer(id);

                        std::lock_guard<std::mutex> lock(SubServer::serversMutex);
                        if (ssh.info->playerCount > 0) ssh.info->playerCount--;
                        fmt::print(fmt::fg(fmt::color::yellow), "[SUB] Player {} disconnected from {}:{}\n", id, ssh.info->IP, ssh.info->UDP);
                        break;
                    }
                    default: break;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    EnableANSIColors();

    Server::LoadConfig("Helper/config.json");
    Enet::SetupMainServer();

    std::thread mainThread(Enet::MainServerListener);

    // Create SubServer hosts
    uint16_t baseUDP = Server::UDP;
    int subIndex = 1;
    for (auto& sub : SubServer::servers) {
        ENetAddress addr{};
        enet_address_set_host(&addr, sub->IP.c_str());

        sub->UDP = baseUDP + subIndex++;
        addr.port = sub->UDP;

        ENetHost* h = enet_host_create(&addr, 1024, 1, 0, 0);
        if (!h) {
            fmt::print(fmt::fg(fmt::color::red), "[ERROR] Failed to create subserver host for {}:{}\n", sub->IP, sub->UDP);
            continue;
        }
        h->usingNewPacketForServer = 1;
        h->checksum = enet_crc32;
        enet_host_compress_with_range_coder(h);

        subserverHosts.emplace_back(*sub, h);
    }

    std::thread subThread(Enet::SubServerListenerAll);

    mainThread.join();
    subThread.join();

    curl_global_cleanup();

    return 0;
}