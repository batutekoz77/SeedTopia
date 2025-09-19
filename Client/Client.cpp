#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <iostream>
#include <regex>
#include <string>
#include <functional>
#include <thread>
#include <vector>
#include <cstdint>
#include <optional>

#include <fmt/core.h>
#include <fmt/color.h>

#include <enet/enet.h>
#include "JsonReader.hpp"

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include "UserInterface/Interface.hpp"


// Client Peer Variables
struct ClientPeer {
    ENetAddress address;
    ENetHost* client = nullptr;
    ENetPeer* serverPeer = nullptr;

    std::string error = "";

    std::string UI = "";
    std::string ip = "";
    std::string hwid = "";
};
ClientPeer p;

std::string GetWMIProperty(const std::wstring& wmiClass, const std::wstring& wmiProperty) {
    HRESULT hres;
    std::string result = "";

    // COM başlat
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return "";

    hres = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);
    if (FAILED(hres)) return "";

    IWbemLocator* pLoc = nullptr;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) return "";

    IWbemServices* pSvc = nullptr;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, NULL, 0, NULL, 0, &pSvc);
    if (FAILED(hres)) return "";

    IEnumWbemClassObject* pEnumerator = nullptr;
    std::wstring query = L"SELECT " + wmiProperty + L" FROM " + wmiClass;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t(query.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnumerator);
    if (FAILED(hres)) return "";

    IWbemClassObject* pclsObj = nullptr;
    ULONG uReturn = 0;
    if (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == S_OK) {
        VARIANT vtProp;
        if (pclsObj->Get(wmiProperty.c_str(), 0, &vtProp, 0, 0) == S_OK) {
            _bstr_t bstrVal(vtProp.bstrVal);
            result = (const char*)bstrVal;
        }
        VariantClear(&vtProp);
        pclsObj->Release();
    }

    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return result;
}

// ANSI renkler
void EnableANSIColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut == INVALID_HANDLE_VALUE) return;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

// Basit IP alma
std::string GetLocalMainIP() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) return "0.0.0.0";

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");
    serv.sin_port = htons(53);

    connect(sock, (sockaddr*)&serv, sizeof(serv));

    sockaddr_in name{};
    int namelen = sizeof(name);
    getsockname(sock, (sockaddr*)&name, &namelen);

    char buffer[INET_ADDRSTRLEN];
    InetNtopA(AF_INET, &name.sin_addr, buffer, INET_ADDRSTRLEN);

    closesocket(sock);
    WSACleanup();

    return std::string(buffer);
}

// CLIENT namespace
namespace Client {
    bool activated = false;
    OptimisedValue<std::string> NAME{ "" };
    OptimisedValue<std::string> IP{ "" };
    uint16_t UDP;
    uint16_t TCP;

    inline void LoadConfig(const std::string& file) {
        activated = true;
        *NAME = JsonReader::readJson<std::string>(file, "Client_Name");
        *IP = JsonReader::readJson<std::string>(file, "Server_IP");
        UDP = static_cast<uint16_t>(JsonReader::readJson<unsigned int>(file, "Server_UDP"));
        TCP = static_cast<uint16_t>(JsonReader::readJson<unsigned int>(file, "Server_TCP"));

        SetConsoleTitleA(NAME->c_str());
        fmt::print(fmt::fg(fmt::color::green), "Loaded config file\n");
    }
}

// ENET namespace
namespace Enet {
    bool activated = false;

    bool redirected = false;
    OptimisedValue<std::string> newIP{ "" };
    uint16_t newUDP = 0;

    bool ConnectTo(const std::string& ip, uint16_t port) {
        enet_address_set_host(&p.address, ip.c_str());
        p.address.port = port;

        p.serverPeer = enet_host_connect(p.client, &p.address, 2, 0);
        if (!p.serverPeer) {
            fmt::print(fmt::fg(fmt::color::red), "[ERROR] Could not create peer\n");
            return false;
        }
        //batutekoz77
        ENetEvent event;
        if (enet_host_service(p.client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            fmt::print(fmt::fg(fmt::color::green), "Connected to {}:{}\n", ip, port);

            std::string cpuID = GetWMIProperty(L"Win32_Processor", L"ProcessorId");
            std::string biosSerial = GetWMIProperty(L"Win32_BIOS", L"SerialNumber");
            std::string motherboardSerial = GetWMIProperty(L"Win32_BaseBoard", L"SerialNumber");

            std::string hwid_raw = cpuID + biosSerial + motherboardSerial;
            std::hash<std::string> hasher;
            size_t hwid = hasher(hwid_raw);

            std::string hwid_str = "enter_game\nip|" + GetLocalMainIP() + "\nhwid|" + std::to_string(hwid);
            ENetPacket* packet = enet_packet_create(hwid_str.c_str(), hwid_str.size() + 1, ENET_PACKET_FLAG_RELIABLE);

            enet_peer_send(p.serverPeer, 0, packet);
            enet_host_flush(p.client);

            return true;
        }
        else {
            enet_peer_reset(p.serverPeer);
            fmt::print(fmt::fg(fmt::color::red), "[ERROR] Connection timeout {}:{}\n", ip, port);
            return false;
        }
    }

    void ClientListener() {
        if (!activated) return;

        while (activated) {
            ENetEvent event;
            while (enet_host_service(p.client, &event, 1000) > 0) {
                switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE: {
                    std::string msg(reinterpret_cast<char*>(event.packet->data));
                    auto parsed = ParseMessage(msg);

                    if (event.peer == p.serverPeer && parsed.command == "changesub" && parsed.has("ip") && parsed.has("udp")) {
                        Enet::newIP = parsed.get("ip");
                        Enet::newUDP = static_cast<uint16_t>(std::stoi(parsed.get("udp")));
                        Enet::redirected = true;
                        fmt::print(fmt::fg(fmt::color::yellow), "[CLIENT] Redirecting to {}:{}\n", *Enet::newIP, Enet::newUDP);

                        enet_peer_disconnect_later(event.peer, 0);
                        enet_packet_destroy(event.packet);
                    }
                    if (event.peer == p.serverPeer && parsed.command == "changeUI" && parsed.has("method")) {
                        p.UI = parsed.get("method");
                        fmt::print(fmt::fg(fmt::color::yellow), "[CLIENT] Ui changed to {}\n", p.UI);
                    }
                    else if (event.peer == p.serverPeer && parsed.command == "error" && parsed.has("method")) {
                        p.error = parsed.get("method");
                        fmt::print(fmt::fg(fmt::color::yellow), "[CLIENT] Error changed to {}\n", p.error);
                    }
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT: {
                    fmt::print(fmt::fg(fmt::color::yellow), "[CLIENT] Disconnected from server\n");
                    p.serverPeer = nullptr;

                    if (Enet::redirected) {
                        Enet::redirected = false;
                        enet_host_destroy(p.client);
                        p.client = enet_host_create(nullptr, 1, 2, 0, 0);
                        p.client->usingNewPacket = 1;
                        p.client->checksum = enet_crc32;
                        enet_host_compress_with_range_coder(p.client);
                        if (ConnectTo(*Enet::newIP, Enet::newUDP)) {
                            fmt::print(fmt::fg(fmt::color::green), "[CLIENT] Now connected to subserver {}:{}\n", *Enet::newIP, Enet::newUDP);
                        }
                        else {
                            fmt::print(fmt::fg(fmt::color::red), "[ERROR] Failed to connect to subserver {}:{}\n", *Enet::newIP, Enet::newUDP);
                            activated = false;
                        }
                    }
                    break;
                }

                default:
                    break;
                }
            }
        }

        if (p.client) {
            enet_host_destroy(p.client);
            p.client = nullptr;
        }
    }

    bool ConnectClient() {
        if (!Client::activated) return false;

        if (enet_initialize() != 0) {
            fmt::print(fmt::fg(fmt::color::red), "[ERROR] Failed to initialize ENet\n");
            return false;
        }
        else fmt::print(fmt::fg(fmt::color::green), "Enet Initialized...\n");

        ENetAddress bindAddr{};
        enet_address_set_host(&bindAddr, GetLocalMainIP().c_str());

        p.client = enet_host_create(&bindAddr, 1, 2, 0, 0);
        if (!p.client) {
            fmt::print(fmt::fg(fmt::color::red), "[ERROR] Failed to create ENet client host\n");
            return false;
        }

        p.client->usingNewPacket = 1;
        p.client->checksum = enet_crc32;
        enet_host_compress_with_range_coder(p.client);

        activated = true;
        fmt::print(fmt::fg(fmt::color::green), "Client is connected...\n");
        return ConnectTo(*Client::IP, Client::UDP);
    }
}

int main() {
    p.UI = "MAIN_MENU";

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(1024, 768)), "SFML UI + ENet Example", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    sf::Vector2u windowSize = window.getSize();

    EnableANSIColors();

    Client::LoadConfig("Helper/config.json");


    sf::Font fontRoboto("Fonts/Roboto-Regular.ttf");
    sf::Font fontBoldRoboto("Fonts/Roboto-SemiBold.ttf");

    /* Main Menu */
    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile("Cache/Interface/main_menu_bg_themed.png")) {
        throw std::runtime_error("Failed to load background texture!");
    }
    sf::Texture logoTexture;
    if (!logoTexture.loadFromFile("Cache/Interface/game_title.png")) {
        throw std::runtime_error("Failed to load logo texture!");
    }

    sf::Sprite background(bgTexture);
    sf::Sprite logo(logoTexture);

    background.setScale(sf::Vector2f(
        float(windowSize.x) / bgTexture.getSize().x,
        float(windowSize.y) / bgTexture.getSize().y
    ));

    logo.setScale(sf::Vector2f(0.75, 0.75));

    auto logoBounds = logo.getLocalBounds();
    logo.setPosition(sf::Vector2f(120.f, 40.f));

    sf::Text bottomRight(fontBoldRoboto, "V5.26", 20);
    bottomRight.setFillColor(sf::Color::White);

    auto updateBottomRight = [&](sf::Text& t) {
        auto bounds = t.getLocalBounds();
        float x = window.getSize().x - (bounds.size.x + bounds.position.x) - 16.f;
        float y = window.getSize().y - (bounds.size.y + bounds.position.y) - 16.f;
        t.setPosition(sf::Vector2f(x, y));
    };
    updateBottomRight(bottomRight);

    UIButton playBtn(fontBoldRoboto, "Play Online", 50, { 320.f, 75.f }, 26, 176, 26);
    playBtn.setPosition(sf::Vector2f(370.f, 380.f));

    playBtn.setCallback([&]() {
        if (!Enet::activated && Enet::ConnectClient()) {
            std::thread listenerThread(Enet::ClientListener);
            listenerThread.detach();
        }
    });

    /* Main Menu */



    /* Login Register Menu */
    float panelX = windowSize.x / 2.f;
    float panelY = windowSize.y / 2.f;
    sf::Vector2f inputSize(400.f, 40.f);
    float spacing = 15.f;
    float labelSpacing = 5.f;

    // Checkbox
    sf::RectangleShape checkbox(sf::Vector2f(20.f, 20.f));
    checkbox.setFillColor(sf::Color::White);
    checkbox.setOutlineColor(sf::Color::Black);
    checkbox.setOutlineThickness(2.f);
    checkbox.setPosition(sf::Vector2f(panelX - inputSize.x / 2.f, panelY - 140.f));

    sf::Text checkboxText(fontBoldRoboto, "I have a SeedID", 20);
    checkboxText.setFillColor(sf::Color::White);
    checkboxText.setPosition(sf::Vector2f(checkbox.getPosition().x + 30.f, checkbox.getPosition().y));

    bool hasSeedID = false;

    // Login Fields
    TextInput loginSeedID(fontRoboto, { panelX - inputSize.x / 2.f, panelY - 80.f }, 24);
    TextInput loginPassword(fontRoboto, { panelX - inputSize.x / 2.f, panelY + 10.f }, 24);

    // Login Labels
    sf::Text loginSeedLabel(fontRoboto, "SeedID:", 20);
    loginSeedLabel.setFillColor(sf::Color::White);
    loginSeedLabel.setPosition(sf::Vector2f(loginSeedID.getPosition().x, loginSeedID.getPosition().y - 25 - labelSpacing));

    sf::Text loginPasswordLabel(fontRoboto, "Password:", 20);
    loginPasswordLabel.setFillColor(sf::Color::White);
    loginPasswordLabel.setPosition(sf::Vector2f(loginPassword.getPosition().x, loginPassword.getPosition().y - 25 - labelSpacing));



    // Register Fields
    TextInput regMail(fontRoboto, { panelX - inputSize.x / 2.f, panelY - 80.f }, 24);
    TextInput regSeedID(fontRoboto, { panelX - inputSize.x / 2.f, panelY + 10.f }, 24);
    TextInput regPassword(fontRoboto, { panelX - inputSize.x / 2.f, panelY + 100.f }, 24);

    // Register Labels
    sf::Text regMailLabel(fontRoboto, "Email:", 20);
    regMailLabel.setFillColor(sf::Color::White);
    regMailLabel.setPosition(sf::Vector2f(regMail.getPosition().x, regMail.getPosition().y - 25 - labelSpacing));

    sf::Text regSeedLabel(fontRoboto, "SeedID:", 20);
    regSeedLabel.setFillColor(sf::Color::White);
    regSeedLabel.setPosition(sf::Vector2f(regSeedID.getPosition().x, regSeedID.getPosition().y - 25 - labelSpacing));

    sf::Text regPasswordLabel(fontRoboto, "Password:", 20);
    regPasswordLabel.setFillColor(sf::Color::White);
    regPasswordLabel.setPosition(sf::Vector2f(regPassword.getPosition().x, regPassword.getPosition().y - 25 - labelSpacing));


    sf::Text errorText(fontBoldRoboto, "", 30);
    errorText.setFillColor(sf::Color::Red);
    errorText.setPosition(sf::Vector2f(50.f, 50.f));


    // Connect Button
    sf::Vector2f buttonSize(200.f, 50.f);
    UIButton connectBtn(fontBoldRoboto, "Connect", 30, buttonSize, 26, 176, 26);
    connectBtn.setPosition({ panelX - buttonSize.x / 2.f - 45.f, panelY + 160.f });
    connectBtn.setCallback([&]() {
        if (hasSeedID) {
            if (loginSeedID.getText().length() < 3) errorText.setString("SeedID can't be smaller than 3 characters!");
            else if (loginSeedID.getText().length() > 18) errorText.setString("SeedID can't be bigger than 18 characters!");
            else if (!std::regex_match(loginSeedID.getText(), std::regex("^[a-zA-Z]+$"))) errorText.setString("SeedID can't contain special characters!");

            else if (loginPassword.getText().length() < 3) errorText.setString("Password can't be smaller than 5 characters!");
            else if (loginPassword.getText().length() > 24) errorText.setString("Password can't be bigger than 24 characters!");
            else if (std::regex_search(loginSeedID.getText(), std::regex("\\s"))) errorText.setString("Password can't contain space!");

            else {
                std::string packetMaker = "login\nseedID|" + loginSeedID.getText() + "\npass|" + loginPassword.getText() + "\n";
                ENetPacket* packet = enet_packet_create(packetMaker.c_str(), packetMaker.size() + 1, ENET_PACKET_FLAG_RELIABLE);

                enet_peer_send(p.serverPeer, 0, packet);
                enet_host_flush(p.client);
            }
        }
        else {
            if (regMail.getText().length() < 3) errorText.setString("Mail can't be smaller than 3 characters!");
            else if (regMail.getText().length() > 24) errorText.setString("Mail can't be bigger than 24 characters!");
            else if (!std::regex_match(regMail.getText(), std::regex("^[a-zA-Z0-9.@]+$"))) errorText.setString("Mail can only contain letters, numbers, (.) and (@) chars!");
            else if (regMail.getText().find('@') == std::string::npos || regMail.getText().find('.') == std::string::npos) errorText.setString("This mail address doesn't seem to be valid!");

            else if (regSeedID.getText().length() < 3) errorText.setString("SeedID can't be smaller than 3 characters!");
            else if (regSeedID.getText().length() > 18) errorText.setString("SeedID can't be bigger than 18 characters!");
            else if (!std::regex_match(regSeedID.getText(), std::regex("^[a-zA-Z]+$"))) errorText.setString("SeedID can't contain special characters!");

            else if (regPassword.getText().length() < 3) errorText.setString("Password can't be smaller than 5 characters!");
            else if (regPassword.getText().length() > 24) errorText.setString("Password can't be bigger than 24 characters!");
            else if (std::regex_search(regPassword.getText(), std::regex("\\s"))) errorText.setString("Password can't contain space!");

            else {
                std::string packetMaker = "register\nmail|" + regMail.getText() + "\nseedID|" + regSeedID.getText() + "\npass|" + regPassword.getText() + "\n";
                ENetPacket* packet = enet_packet_create(packetMaker.c_str(), packetMaker.size() + 1, ENET_PACKET_FLAG_RELIABLE);

                enet_peer_send(p.serverPeer, 0, packet);
                enet_host_flush(p.client);
            }
        }
    });

    sf::Vector2f buttonSizeForgot(150.f, 35.f);
    UIButton connectBtnForgot(fontBoldRoboto, "Forgot Pass", 17, buttonSizeForgot, 122, 2, 0);
    connectBtnForgot.setPosition({ panelX - buttonSizeForgot.x / 2.f - 45.f, panelY + 230.f });
    connectBtnForgot.setCallback([&]() {
        if (loginSeedID.getText().length() < 3) errorText.setString("SeedID can't be smaller than 3 characters!");
        else if (loginSeedID.getText().length() > 18) errorText.setString("SeedID can't be bigger than 18 characters!");
        else if (!std::regex_match(loginSeedID.getText(), std::regex("^[a-zA-Z]+$"))) errorText.setString("SeedID can't contain special characters!");

        else {
            std::string packetMaker = "forgot\nseedID|" + loginSeedID.getText() + "\n";
            ENetPacket* packet = enet_packet_create(packetMaker.c_str(), packetMaker.size() + 1, ENET_PACKET_FLAG_RELIABLE);

            enet_peer_send(p.serverPeer, 0, packet);
            enet_host_flush(p.client);
        }
    });


    /* Login Register Menu Draw */


    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (auto* pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (p.UI == "MAIN_MENU") {
                    playBtn.handleEvent(*pressed, window);
                }
                else if (p.UI == "LOGIN_REGISTER") {
                    sf::Vector2f mousePos(pressed->position.x, pressed->position.y);
                    if (checkbox.getGlobalBounds().contains(mousePos)) hasSeedID = !hasSeedID;
                    
                    loginSeedID.handleEvent(*pressed, window);
                    loginPassword.handleEvent(*pressed, window);
                    regMail.handleEvent(*pressed, window);
                    regSeedID.handleEvent(*pressed, window);
                    regPassword.handleEvent(*pressed, window);

                    connectBtn.handleEvent(*pressed, window);
                    connectBtnForgot.handleEvent(*pressed, window);
                }
            }

            if (auto* released = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (p.UI == "MAIN_MENU") {
                    playBtn.handleEvent(*released, window);
                }
                else if (p.UI == "LOGIN_REGISTER") {
                    loginSeedID.handleEvent(*released, window);
                    loginPassword.handleEvent(*released, window);
                    regMail.handleEvent(*released, window);
                    regSeedID.handleEvent(*released, window);
                    regPassword.handleEvent(*released, window);

                    connectBtn.handleEvent(*released, window);
                    connectBtnForgot.handleEvent(*released, window);
                }
            }

            if (auto* textEvent = event->getIf<sf::Event::TextEntered>()) {
                if (p.UI == "MAIN_MENU") {

                }
                else if (p.UI == "LOGIN_REGISTER") {
                    loginSeedID.handleEvent(*textEvent);
                    loginPassword.handleEvent(*textEvent);
                    regMail.handleEvent(*textEvent);
                    regSeedID.handleEvent(*textEvent);
                    regPassword.handleEvent(*textEvent);
                }
            }
        }

        window.clear(sf::Color(60, 60, 60));

        if (p.UI == "MAIN_MENU") {
            window.draw(background);
            window.draw(logo);
            playBtn.draw(window);
            window.draw(bottomRight);
        }
        else if (p.UI == "LOGIN_REGISTER") {
            window.draw(checkbox);
            window.draw(checkboxText);
            if (hasSeedID) {
                sf::RectangleShape line1(sf::Vector2f(6.f, 2.f));
                line1.setRotation(sf::degrees(45.f));
                line1.setPosition(sf::Vector2f(checkbox.getPosition().x + 4.f, checkbox.getPosition().y + 10.f));
                line1.setFillColor(sf::Color::Black);

                sf::RectangleShape line2(sf::Vector2f(10.f, 2.f));
                line2.setRotation(sf::degrees(-45.f));
                line2.setPosition(sf::Vector2f(checkbox.getPosition().x + 7.f, checkbox.getPosition().y + 16.f));
                line2.setFillColor(sf::Color::Black);

                window.draw(line1);
                window.draw(line2);

                window.draw(loginSeedLabel);
                window.draw(loginPasswordLabel);
                loginSeedID.draw(window);
                loginPassword.draw(window);

                if (!p.error.empty()) {
                    errorText.setString(p.error);
                    p.error.clear();
                }
                else if (!errorText.getString().isEmpty()) window.draw(errorText);

                connectBtnForgot.draw(window);
            }
            else {
                window.draw(regMailLabel);
                window.draw(regSeedLabel);
                window.draw(regPasswordLabel);
                regMail.draw(window);
                regSeedID.draw(window);
                regPassword.draw(window);

                if (!p.error.empty()) {
                    errorText.setString(p.error);
                    p.error.clear();
                }
                else if (!errorText.getString().isEmpty()) window.draw(errorText);
            }
            connectBtn.draw(window);
        }

        window.display();
    }

    enet_deinitialize();
    return 0;
}