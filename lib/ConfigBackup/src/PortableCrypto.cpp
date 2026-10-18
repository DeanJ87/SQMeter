// Portable SHA-256, HMAC-SHA256, PBKDF2, AES-256 and GCM for the native tests
// and the browser demo (specs/033-config-backup, research R2). The firmware
// uses mbedTLS instead (src/BackupCryptoMbed.cpp), so none of this is in the
// device image. Held to published test vectors in test/test_config_backup.

#ifndef ESP_PLATFORM

#include "BackupCrypto.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

namespace SQM::Backup
{
    namespace
    {
        // --- SHA-256 (FIPS 180-4) --------------------------------------------
        constexpr size_t SHA_BLOCK = 64;
        constexpr size_t SHA_BYTES = 32;

        constexpr std::array<uint32_t, 64> SHA_K = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
            0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
            0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
            0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

        inline uint32_t rotr(uint32_t x, int n)
        {
            return (x >> n) | (x << (32 - n));
        }

        struct Sha256
        {
            std::array<uint32_t, 8> h = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
            uint64_t length = 0; // bytes hashed
            std::array<uint8_t, SHA_BLOCK> block{};
            size_t used = 0;

            void compress(const uint8_t *p)
            {
                std::array<uint32_t, 64> w{};
                for (size_t i = 0; i < 16; ++i)
                    w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) | (uint32_t(p[i * 4 + 2]) << 8) | p[i * 4 + 3];
                for (size_t i = 16; i < 64; ++i)
                {
                    const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                    const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
                }
                std::array<uint32_t, 8> v = h;
                for (size_t i = 0; i < 64; ++i)
                {
                    const uint32_t t1 =
                        v[7] + (rotr(v[4], 6) ^ rotr(v[4], 11) ^ rotr(v[4], 25)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) + SHA_K[i] + w[i];
                    const uint32_t t2 = (rotr(v[0], 2) ^ rotr(v[0], 13) ^ rotr(v[0], 22)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
                    v = {t1 + t2, v[0], v[1], v[2], v[3] + t1, v[4], v[5], v[6]};
                }
                for (size_t i = 0; i < 8; ++i)
                    h[i] += v[i];
            }

            void update(const uint8_t *data, size_t size)
            {
                length += size;
                while (size > 0)
                {
                    const size_t take = std::min(size, SHA_BLOCK - used);
                    std::memcpy(block.data() + used, data, take);
                    used += take;
                    data += take;
                    size -= take;
                    if (used == SHA_BLOCK)
                    {
                        compress(block.data());
                        used = 0;
                    }
                }
            }

            void finish(uint8_t (&out)[SHA_BYTES])
            {
                const uint64_t bits = length * 8;
                const uint8_t pad = 0x80;
                update(&pad, 1);
                const uint8_t zero = 0;
                while (used != SHA_BLOCK - 8)
                    update(&zero, 1);
                for (int i = 7; i >= 0; --i)
                {
                    const auto byte = static_cast<uint8_t>(bits >> (i * 8));
                    update(&byte, 1);
                }
                for (size_t i = 0; i < 8; ++i)
                    for (size_t j = 0; j < 4; ++j)
                        out[i * 4 + j] = static_cast<uint8_t>(h[i] >> (24 - j * 8));
            }
        };

        // --- HMAC-SHA256 (RFC 2104), with the keyed states kept for PBKDF2 ----
        struct Hmac
        {
            Sha256 inner;
            Sha256 outer;

            explicit Hmac(const std::string &key)
            {
                std::array<uint8_t, SHA_BLOCK> k{};
                if (key.size() > SHA_BLOCK)
                {
                    Sha256 hashed;
                    hashed.update(reinterpret_cast<const uint8_t *>(key.data()), key.size());
                    uint8_t digest[SHA_BYTES];
                    hashed.finish(digest);
                    std::memcpy(k.data(), digest, SHA_BYTES);
                }
                else
                {
                    std::memcpy(k.data(), key.data(), key.size());
                }
                std::array<uint8_t, SHA_BLOCK> pad{};
                for (size_t i = 0; i < SHA_BLOCK; ++i)
                    pad[i] = k[i] ^ 0x36;
                inner.update(pad.data(), SHA_BLOCK);
                for (size_t i = 0; i < SHA_BLOCK; ++i)
                    pad[i] = k[i] ^ 0x5c;
                outer.update(pad.data(), SHA_BLOCK);
            }

            void mac(const uint8_t *data, size_t size, uint8_t (&out)[SHA_BYTES]) const
            {
                Sha256 in = inner;
                in.update(data, size);
                uint8_t digest[SHA_BYTES];
                in.finish(digest);
                Sha256 out2 = outer;
                out2.update(digest, SHA_BYTES);
                out2.finish(out);
            }
        };

        // --- AES-256 (FIPS 197), encryption only (GCM never decrypts blocks) --
        constexpr std::array<uint8_t, 256> SBOX = {
            0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d,
            0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
            0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
            0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
            0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef, 0xaa, 0xfb,
            0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
            0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d,
            0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
            0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
            0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
            0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9,
            0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
            0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

        constexpr size_t AES_BLOCK = 16;
        constexpr size_t AES_ROUNDS = 14;
        using Block = std::array<uint8_t, AES_BLOCK>;

        inline uint8_t xtime(uint8_t x)
        {
            return static_cast<uint8_t>((x << 1) ^ ((x & 0x80) ? 0x1b : 0x00));
        }

        class Aes256
        {
        public:
            explicit Aes256(const uint8_t (&key)[KEY_BYTES])
            {
                std::memcpy(roundKeys.data(), key, KEY_BYTES);
                uint8_t rcon = 1;
                for (size_t i = KEY_BYTES; i < roundKeys.size(); i += 4)
                {
                    uint8_t t[4] = {roundKeys[i - 4], roundKeys[i - 3], roundKeys[i - 2], roundKeys[i - 1]};
                    if (i % KEY_BYTES == 0)
                    {
                        const uint8_t first = t[0];
                        t[0] = static_cast<uint8_t>(SBOX[t[1]] ^ rcon);
                        t[1] = SBOX[t[2]];
                        t[2] = SBOX[t[3]];
                        t[3] = SBOX[first];
                        rcon = xtime(rcon);
                    }
                    else if (i % KEY_BYTES == 16)
                    {
                        for (uint8_t &b : t)
                            b = SBOX[b];
                    }
                    for (size_t j = 0; j < 4; ++j)
                        roundKeys[i + j] = roundKeys[i + j - KEY_BYTES] ^ t[j];
                }
            }

            Block encrypt(const Block &in) const
            {
                Block s = in;
                addRoundKey(s, 0);
                for (size_t round = 1; round <= AES_ROUNDS; ++round)
                {
                    subShift(s);
                    if (round != AES_ROUNDS)
                        mixColumns(s);
                    addRoundKey(s, round);
                }
                return s;
            }

        private:
            std::array<uint8_t, AES_BLOCK *(AES_ROUNDS + 1)> roundKeys{};

            void addRoundKey(Block &s, size_t round) const
            {
                for (size_t i = 0; i < AES_BLOCK; ++i)
                    s[i] ^= roundKeys[round * AES_BLOCK + i];
            }

            // SubBytes then ShiftRows (state is column-major: s[col * 4 + row]).
            static void subShift(Block &s)
            {
                Block t{};
                for (size_t col = 0; col < 4; ++col)
                    for (size_t row = 0; row < 4; ++row)
                        t[col * 4 + row] = SBOX[s[((col + row) % 4) * 4 + row]];
                s = t;
            }

            static void mixColumns(Block &s)
            {
                for (size_t col = 0; col < 4; ++col)
                {
                    uint8_t *c = &s[col * 4];
                    const uint8_t all = c[0] ^ c[1] ^ c[2] ^ c[3];
                    const uint8_t first = c[0];
                    c[0] ^= all ^ xtime(c[0] ^ c[1]);
                    c[1] ^= all ^ xtime(c[1] ^ c[2]);
                    c[2] ^= all ^ xtime(c[2] ^ c[3]);
                    c[3] ^= all ^ xtime(c[3] ^ first);
                }
            }
        };

        // --- GCM (NIST SP 800-38D) -------------------------------------------
        // GF(2^128) multiply, bit by bit (the inputs are a few hundred bytes).
        Block gfMultiply(const Block &x, const Block &y)
        {
            Block z{};
            Block v = y;
            for (size_t i = 0; i < 128; ++i)
            {
                if ((x[i / 8] >> (7 - i % 8)) & 1)
                    for (size_t j = 0; j < AES_BLOCK; ++j)
                        z[j] ^= v[j];
                const bool carry = (v[AES_BLOCK - 1] & 1) != 0;
                for (size_t j = AES_BLOCK - 1; j > 0; --j)
                    v[j] = static_cast<uint8_t>((v[j] >> 1) | (v[j - 1] << 7));
                v[0] >>= 1;
                if (carry)
                    v[0] ^= 0xe1;
            }
            return z;
        }

        void ghashUpdate(Block &state, const Block &h, const uint8_t *data, size_t size)
        {
            for (size_t offset = 0; offset < size; offset += AES_BLOCK)
            {
                const size_t take = std::min(AES_BLOCK, size - offset);
                for (size_t i = 0; i < take; ++i)
                    state[i] ^= data[offset + i];
                state = gfMultiply(state, h);
            }
        }

        void putLength(Block &block, size_t offset, uint64_t bytes)
        {
            const uint64_t bits = bytes * 8;
            for (size_t i = 0; i < 8; ++i)
                block[offset + i] = static_cast<uint8_t>(bits >> (56 - i * 8));
        }

        // The data a GCM pass reads and writes.
        struct GcmData
        {
            const uint8_t *in;
            size_t size;
            uint8_t *out;
            bool encrypting;
        };

        // CTR from inc32(J0) over `in`, then the tag over aad and the ciphertext.
        Block gcm(const Aes256 &aes, const uint8_t (&nonce)[NONCE_BYTES], const std::string &aad, const GcmData &data)
        {
            const uint8_t *in = data.in;
            const size_t size = data.size;
            uint8_t *out = data.out;
            const bool encrypting = data.encrypting;
            const Block h = aes.encrypt(Block{});
            Block counter{};
            std::memcpy(counter.data(), nonce, NONCE_BYTES);
            counter[AES_BLOCK - 1] = 1;
            const Block tagMask = aes.encrypt(counter);

            Block hash{};
            ghashUpdate(hash, h, reinterpret_cast<const uint8_t *>(aad.data()), aad.size());
            if (!encrypting)
                ghashUpdate(hash, h, in, size);
            for (size_t offset = 0; offset < size; offset += AES_BLOCK)
            {
                for (size_t i = AES_BLOCK; i-- > NONCE_BYTES;)
                    if (++counter[i] != 0)
                        break;
                const Block stream = aes.encrypt(counter);
                const size_t take = std::min(AES_BLOCK, size - offset);
                for (size_t i = 0; i < take; ++i)
                    out[offset + i] = in[offset + i] ^ stream[i];
            }
            if (encrypting)
                ghashUpdate(hash, h, out, size);
            Block lengths{};
            putLength(lengths, 0, aad.size());
            putLength(lengths, 8, size);
            ghashUpdate(hash, h, lengths.data(), AES_BLOCK);
            for (size_t i = 0; i < AES_BLOCK; ++i)
                hash[i] ^= tagMask[i];
            return hash;
        }
    } // namespace

    bool deriveKey(const std::string &passphrase, const uint8_t *salt, size_t saltLength, uint32_t iterations, uint8_t (&key)[KEY_BYTES])
    {
        if (iterations == 0)
            return false;
        const Hmac hmac(passphrase);
        std::string first(reinterpret_cast<const char *>(salt), saltLength);
        first.append("\x00\x00\x00\x01", 4); // block index 1: one SHA-256 output is the whole key
        uint8_t u[SHA_BYTES];
        hmac.mac(reinterpret_cast<const uint8_t *>(first.data()), first.size(), u);
        std::memcpy(key, u, KEY_BYTES);
        for (uint32_t i = 1; i < iterations; ++i)
        {
            uint8_t next[SHA_BYTES];
            hmac.mac(u, SHA_BYTES, next);
            std::memcpy(u, next, SHA_BYTES);
            for (size_t j = 0; j < KEY_BYTES; ++j)
                key[j] ^= u[j];
        }
        return true;
    }

    bool seal(
        const uint8_t (&key)[KEY_BYTES],
        const uint8_t (&nonce)[NONCE_BYTES],
        const std::string &aad,
        const std::string &plain,
        std::string &sealed)
    {
        const Aes256 aes(key);
        sealed.assign(plain.size() + TAG_BYTES, '\0');
        auto *out = reinterpret_cast<uint8_t *>(&sealed[0]);
        const Block tag = gcm(aes, nonce, aad, {reinterpret_cast<const uint8_t *>(plain.data()), plain.size(), out, true});
        std::memcpy(out + plain.size(), tag.data(), TAG_BYTES);
        return true;
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
        const size_t size = sealed.size() - TAG_BYTES;
        const Aes256 aes(key);
        std::string out(size, '\0');
        const auto *in = reinterpret_cast<const uint8_t *>(sealed.data());
        const Block tag = gcm(aes, nonce, aad, {in, size, reinterpret_cast<uint8_t *>(&out[0]), false});
        uint8_t difference = 0; // constant-time compare
        for (size_t i = 0; i < TAG_BYTES; ++i)
            difference |= static_cast<uint8_t>(tag[i] ^ in[size + i]);
        if (difference != 0)
            return false;
        plain = std::move(out);
        return true;
    }
} // namespace SQM::Backup

#endif // ESP_PLATFORM
