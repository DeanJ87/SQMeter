#include "BackupCrypto.h"

#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

// The settings backup's crypto on the device, with ESP-IDF's mbedTLS (already
// linked for TLS). Native tests and the demo use lib/ConfigBackup/src/PortableCrypto.cpp,
// which implements the same three functions (specs/033-config-backup, research R2).

namespace SQM::Backup
{
    namespace
    {
        // A GCM context with the key set, freed on scope exit.
        class Gcm
        {
        public:
            explicit Gcm(const uint8_t (&key)[KEY_BYTES])
            {
                mbedtls_gcm_init(&ctx);
                ready = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, KEY_BYTES * 8) == 0;
            }
            ~Gcm() { mbedtls_gcm_free(&ctx); }
            Gcm(const Gcm &) = delete;
            Gcm &operator=(const Gcm &) = delete;

            mbedtls_gcm_context ctx;
            bool ready = false;
        };

        const unsigned char *bytesOf(const std::string &text)
        {
            return reinterpret_cast<const unsigned char *>(text.data());
        }
    } // namespace

    bool deriveKey(const std::string &passphrase, const uint8_t *salt, size_t saltLength, uint32_t iterations, uint8_t (&key)[KEY_BYTES])
    {
        if (iterations == 0)
            return false;
        return mbedtls_pkcs5_pbkdf2_hmac_ext(
                   MBEDTLS_MD_SHA256, bytesOf(passphrase), passphrase.size(), salt, saltLength, iterations, KEY_BYTES, key) == 0;
    }

    bool seal(
        const uint8_t (&key)[KEY_BYTES],
        const uint8_t (&nonce)[NONCE_BYTES],
        const std::string &aad,
        const std::string &plain,
        std::string &sealed)
    {
        Gcm gcm(key);
        if (!gcm.ready)
            return false;
        sealed.assign(plain.size() + TAG_BYTES, '\0');
        auto *out = reinterpret_cast<unsigned char *>(&sealed[0]);
        return mbedtls_gcm_crypt_and_tag(
                   &gcm.ctx,
                   MBEDTLS_GCM_ENCRYPT,
                   plain.size(),
                   nonce,
                   NONCE_BYTES,
                   bytesOf(aad),
                   aad.size(),
                   bytesOf(plain),
                   out,
                   TAG_BYTES,
                   out + plain.size()) == 0;
    }

    bool open(
        const uint8_t (&key)[KEY_BYTES],
        const uint8_t (&nonce)[NONCE_BYTES],
        const std::string &aad,
        const std::string &sealed,
        std::string &plain)
    {
        plain.clear();
        if (sealed.size() < TAG_BYTES)
            return false;
        Gcm gcm(key);
        if (!gcm.ready)
            return false;
        const size_t size = sealed.size() - TAG_BYTES;
        std::string out(size, '\0');
        const int result = mbedtls_gcm_auth_decrypt(
            &gcm.ctx,
            size,
            nonce,
            NONCE_BYTES,
            bytesOf(aad),
            aad.size(),
            bytesOf(sealed) + size,
            TAG_BYTES,
            bytesOf(sealed),
            reinterpret_cast<unsigned char *>(&out[0]));
        if (result != 0)
            return false;
        plain = std::move(out);
        return true;
    }
} // namespace SQM::Backup
