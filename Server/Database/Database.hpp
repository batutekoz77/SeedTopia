#pragma once
#define _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS

#include "sqlite_modern_cpp.h"
#include <string>
#include <vector>

class Database {
public:
    Database(const std::string& db_file);

    bool add_user(const std::string& mail, const std::string& seedid, const std::string& password, const std::string& hwid);

    std::string get_pass_by_seedid(const std::string& seedid);
    std::string get_mail_by_seedid(const std::string& seedid);

    bool mail_used(const std::string& mail);
    bool user_exists(const std::string& seedid);

    bool check_password(const std::string& seedid, const std::string& password);

private:
    sqlite::database db;

    std::string join(const std::vector<std::string>& vec, const std::string& delimiter);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string hash_password(const std::string& password);
};