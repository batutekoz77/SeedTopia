#include "Database.hpp"
#include <sstream>
#include <functional>
#include <optional>

Database::Database(const std::string& db_file)
    : db(db_file) {
    db <<
        "CREATE TABLE IF NOT EXISTS security ("
        "   id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "   mail TEXT NOT NULL UNIQUE, "
        "   seedid TEXT NOT NULL UNIQUE, "
        "   password TEXT NOT NULL, "
        "   hwids TEXT NOT NULL"
        ");";
}

bool Database::add_user(const std::string& mail, const std::string& seedid, const std::string& password, const std::string& hwid) {
    std::string pwd_hash = hash_password(password);

    try {
        db << "INSERT INTO security (mail, seedid, password, hwids) VALUES (?, ?, ?, ?);"
            << mail
            << seedid
            << pwd_hash
            << hwid;
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool Database::user_exists(const std::string& seedid) {
    int count = 0;
    db << "SELECT COUNT(*) FROM security WHERE LOWER(seedid) = LOWER(?);"
        << seedid
        >> count;
    return count > 0;
}

std::string Database::get_pass_by_seedid(const std::string& seedid) {
    std::string pass;
    db << "SELECT LOWER(password) FROM security WHERE LOWER(seedid) = ?;"
        << seedid
        >> pass;
    return pass;
}

std::string Database::get_mail_by_seedid(const std::string& seedid) {
    std::string mail;
    db << "SELECT LOWER(mail) FROM security WHERE LOWER(seedid) = ?;"
        << seedid
        >> mail;
    return mail;
}

bool Database::mail_used(const std::string& mail) {
    int count = 0;
    db << "SELECT COUNT(*) FROM security WHERE LOWER(mail) = LOWER(?);"
        << mail
        >> count;
    return count > 0;
}

bool Database::check_password(const std::string& seedid, const std::string& password) {
    std::optional<std::string> stored_hash;

    db << "SELECT LOWER(password) FROM security WHERE LOWER(seedid) = ?;"
        << seedid
        >> [&](std::string pwd) {
        stored_hash = pwd;
    };

    if (!stored_hash.has_value()) return false;

    std::string given_hash = hash_password(password);
    return stored_hash.value() == given_hash;
}

// ---------------- Private Helpers ----------------

std::string Database::join(const std::vector<std::string>& vec, const std::string& delimiter) {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << delimiter;
        oss << vec[i];
    }
    return oss.str();
}

std::vector<std::string> Database::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        tokens.push_back(item);
    }
    return tokens;
}

std::string Database::hash_password(const std::string& password) {
    std::hash<std::string> hasher;
    size_t hashed = hasher(password);
    return std::to_string(hashed);
}