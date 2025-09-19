#include <curl/curl.h>
#include <iostream>
#include <string>

#include "JsonReader.hpp"

struct upload_status {
    size_t lines_read;
    const char** payload;
};

static size_t payload_source(void* ptr, size_t size, size_t nmemb, void* userp) {
    upload_status* upload_ctx = (upload_status*)userp;
    const char* data = upload_ctx->payload[upload_ctx->lines_read];
    if (!data) return 0;
    size_t len = strlen(data);
    memcpy(ptr, data, len);
    upload_ctx->lines_read++;
    return len;
}

void sendMail(const std::string& to, const std::string& password) {
    CURL* curl;
    CURLcode res = CURLE_OK;

    const char* payload_text[] = {
        ("To: " + to + "\r\n").c_str(),
        ("From: " + JsonReader::readJson<std::string>("Helper/config.json", "MAIL_DOMAIN") + "\r\n").c_str(),
        "Subject: Forgot Password?\r\n",
        "\r\n",
        "Hi, your password is ",
        password.c_str(),
        "\r\n",
        NULL
    };

    upload_status upload_ctx = { 0, payload_text };

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

        curl_easy_setopt(curl, CURLOPT_USERNAME, JsonReader::readJson<std::string>("Helper/config.json", "MAIL_DOMAIN").c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, JsonReader::readJson<std::string>("Helper/config.json", "MAIL_PASS").c_str());

        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, JsonReader::readJson<std::string>("Helper/config.json", "MAIL_DOMAIN").c_str());

        struct curl_slist* recipients = NULL;
        recipients = curl_slist_append(recipients, to.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) std::cerr << "Error sending mail: " << curl_easy_strerror(res) << std::endl;

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }
}