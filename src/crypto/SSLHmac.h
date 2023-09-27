//
// Created by User on 30.01.2023.
//

#ifndef LIBAV_TEST_SSLHMAC_H
#define LIBAV_TEST_SSLHMAC_H
#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/bio.h>
#include <fmt/core.h>

class SSLHmac
{
public:
    ENGINE * engine;
    HMAC_CTX * context;

    SSLHmac(const std::string & key)
    {
        ENGINE_load_builtin_engines();
        ENGINE_register_all_complete();
        context = HMAC_CTX_new();
        HMAC_CTX_reset(context);
        HMAC_Init_ex(context, key.c_str(), key.length(), EVP_sha1(), NULL);
    }

    void update(std::string & data) { HMAC_Update(context, reinterpret_cast<const unsigned char *>(data.c_str()), data.length()); }

    std::string get_raw_result()
    {
        unsigned int len = 20;
        uint8_t digest[len];
        HMAC_Final(context, (unsigned char *)&digest, &len);
        std::cout << len << std::endl;
        return std::string(reinterpret_cast<char *>(digest), len);
    }
    std::string get_str_result()
    {
        unsigned int len = 20;
        uint8_t digest[len];
        HMAC_Final(context, (unsigned char *)&digest, &len);

        std::stringstream ss;
        for (int i = 0; i < len; i++)
        {
            ss << fmt::format("{0:02x}", digest[i]);
        }
        return std::move(ss.str());
    }
    ~SSLHmac() { HMAC_CTX_free(context); }
};
#endif //LIBAV_TEST_SSLHMAC_H
