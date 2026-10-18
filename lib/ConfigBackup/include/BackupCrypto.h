#pragma once

// The three cryptographic steps a settings backup needs (specs/033-config-backup,
// research R2). The firmware implements them with mbedTLS (src/BackupCryptoMbed.cpp);
// native tests and the browser demo use the portable code in PortableCrypto.cpp,
// which is compiled only where ESP_PLATFORM isn't defined.

#include <cstddef>
#include <cstdint>
#include <string>

namespace SQM::Backup
{
    constexpr size_t KEY_BYTES = 32;   // AES-256
    constexpr size_t SALT_BYTES = 16;  // PBKDF2 salt
    constexpr size_t NONCE_BYTES = 12; // GCM nonce
    constexpr size_t TAG_BYTES = 16;   // GCM tag

    // PBKDF2-HMAC-SHA256(passphrase, salt, iterations) -> 32-byte key.
    bool deriveKey(const std::string &passphrase, const uint8_t *salt, size_t saltLength, uint32_t iterations, uint8_t (&key)[KEY_BYTES]);

    // AES-256-GCM encryption: `sealed` becomes ciphertext followed by the tag.
    bool seal(const uint8_t (&key)[KEY_BYTES], const uint8_t (&nonce)[NONCE_BYTES], const std::string &aad, const std::string &plain,
              std::string &sealed);

    // AES-256-GCM decryption of ciphertext + tag. False (and `plain` empty)
    // when the tag doesn't verify: a wrong key or changed data.
    bool open(const uint8_t (&key)[KEY_BYTES], const uint8_t (&nonce)[NONCE_BYTES], const std::string &aad, const std::string &sealed,
              std::string &plain);
} // namespace SQM::Backup
