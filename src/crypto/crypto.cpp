#include "crypto.h"
#include "utils/logging.h"
#include <memory>

namespace naivefs {
    // use AES 256, CBC mode
    const size_t KEYLEN = 256;
    Auth::Auth(const char* s) : aes_(AESKeyLength::AES_256) {
        if(!s || !strlen(s)) flag_ = false;
        else {
            flag_ = true;
            auto _s = reinterpret_cast<const unsigned char*>(s);
            key_ = new unsigned char[KEYLEN];
            memset(key_, 48, KEYLEN);
            memcpy(key_, s, std::min(KEYLEN, strlen(s)));
        }
    }

    Auth::~Auth() {
        if(flag_) delete[] key_;
    }

    void Auth::read(unsigned char* s, size_t len) {
        if(len % 16 || !flag_) return;
        unsigned char iv[16];
        memset(iv, 0, sizeof(iv));
        auto ret = aes_.DecryptCBC(s, len, key_, iv);  
        memcpy(s, ret, len); 
        delete[] ret;
    }

    unsigned char* Auth::write(unsigned char* s, size_t len) {
        if(len % 16 || !flag_) return nullptr;
        unsigned char iv[16];
        memset(iv, 0, sizeof(iv));
        auto ret = aes_.EncryptCBC(s, len, key_, iv);
        return ret;
    }

    Auth* auth;
}