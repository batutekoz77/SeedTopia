#pragma once

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <sstream>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <type_traits>

struct ParsedMessage {
    std::string command;
    std::unordered_map<std::string, std::string> params;

    bool has(const std::string& key) const {
        return params.find(key) != params.end();
    }

    std::string get(const std::string& key) const {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : "";
    }
};

ParsedMessage ParseMessage(const std::string& msg) {
    ParsedMessage result;
    std::istringstream iss(msg);
    std::string line;

    bool firstLine = true;
    while (std::getline(iss, line)) {
        if (firstLine) {
            result.command = line;
            firstLine = false;
        }
        else {
            auto pos = line.find('|');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                result.params[key] = value;
            }
        }
    }
    return result;
}

template<typename T, size_t SMALL_SIZE = 32>
class OptimisedValue {
    bool useSmall;
    alignas(T) char buffer[SMALL_SIZE];
    T* ptr;
public:
    template<typename... Args>
    OptimisedValue(Args&&... args) {
        if (sizeof(T) <= SMALL_SIZE) {
            ptr = new(buffer) T(std::forward<Args>(args)...);
            useSmall = true;
        }
        else {
            ptr = new T(std::forward<Args>(args)...);
            useSmall = false;
        }
    }

    ~OptimisedValue() {
        if (useSmall) ptr->~T();
        else delete ptr;
    }

    OptimisedValue(const OptimisedValue&) = delete;
    OptimisedValue& operator=(const OptimisedValue&) = delete;

    OptimisedValue(OptimisedValue&& other) noexcept {
        useSmall = other.useSmall;
        if (useSmall) {
            ptr = new(buffer) T(std::move(*other.ptr));
        }
        else {
            ptr = other.ptr;
            other.ptr = nullptr;
        }
    }

    OptimisedValue& operator=(OptimisedValue&& other) noexcept {
        if (this != &other) {
            this->~OptimisedValue();
            new(this) OptimisedValue(std::move(other));
        }
        return *this;
    }

    T* operator->() { return ptr; }
    const T* operator->() const { return ptr; }
    T& operator*() { return *ptr; }
    const T& operator*() const { return *ptr; }

    bool isSmall() const { return useSmall; }
};

namespace JsonReader {

    inline std::string getRawJsonValue(const std::string& filename, const std::string& key) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Could not open JSON file: " << filename << "\n";
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();

        json.erase(std::remove_if(json.begin(), json.end(), ::isspace), json.end());

        std::string pattern = "\"" + key + "\":";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) {
            std::cerr << "[ERROR] Key not found in JSON: " << key << "\n";
            return "";
        }

        pos += pattern.size();

        if (json[pos] == '"') {
            size_t end = json.find('"', pos + 1);
            if (end == std::string::npos) return "";
            return json.substr(pos + 1, end - (pos + 1));
        }
        else {
            size_t end = json.find_first_of(",}", pos);
            if (end == std::string::npos) end = json.size();
            return json.substr(pos, end - pos);
        }
    }

    template<typename T>
    T readJson(const std::string& filename, const std::string& key);

    // string
    template<>
    inline std::string readJson<std::string>(const std::string& filename, const std::string& key) {
        return getRawJsonValue(filename, key);
    }

    // bool
    template<>
    inline bool readJson<bool>(const std::string& filename, const std::string& key) {
        std::string val = getRawJsonValue(filename, key);
        return val == "true" || val == "1";
    }

    // int
    template<>
    inline int readJson<int>(const std::string& filename, const std::string& key) {
        std::string val = getRawJsonValue(filename, key);
        try {
            return val.empty() ? 0 : std::stoi(val);
        }
        catch (...) {
            std::cerr << "[ERROR] Invalid int value for key: " << key << "\n";
            return 0;
        }
    }

    // long
    template<>
    inline long readJson<long>(const std::string& filename, const std::string& key) {
        std::string val = getRawJsonValue(filename, key);
        try {
            return val.empty() ? 0 : std::stol(val);
        }
        catch (...) {
            std::cerr << "[ERROR] Invalid long value for key: " << key << "\n";
            return 0;
        }
    }

    // long long
    template<>
    inline long long readJson<long long>(const std::string& filename, const std::string& key) {
        std::string val = getRawJsonValue(filename, key);
        try {
            return val.empty() ? 0 : std::stoll(val);
        }
        catch (...) {
            std::cerr << "[ERROR] Invalid long long value for key: " << key << "\n";
            return 0;
        }
    }

    // unsigned int
    template<>
    inline unsigned int readJson<unsigned int>(const std::string& filename, const std::string& key) {
        std::string val = getRawJsonValue(filename, key);
        try {
            return val.empty() ? 0 : static_cast<unsigned int>(std::stoul(val));
        }
        catch (...) {
            std::cerr << "[ERROR] Invalid unsigned int value for key: " << key << "\n";
            return 0;
        }
    }

    // float
    template<>
    inline float readJson<float>(const std::string& filename, const std::string& key) {
        std::string val = getRawJsonValue(filename, key);
        try {
            return val.empty() ? 0.f : std::stof(val);
        }
        catch (...) {
            std::cerr << "[ERROR] Invalid float value for key: " << key << "\n";
            return 0.f;
        }
    }

    // double
    template<>
    inline double readJson<double>(const std::string& filename, const std::string& key) {
        std::string val = getRawJsonValue(filename, key);
        try {
            return val.empty() ? 0.0 : std::stod(val);
        }
        catch (...) {
            std::cerr << "[ERROR] Invalid double value for key: " << key << "\n";
            return 0.0;
        }
    }
}