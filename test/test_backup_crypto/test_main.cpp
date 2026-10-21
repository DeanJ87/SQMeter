#include <unity.h>

#include <cstring>
#include <string>

#include "BackupCrypto.h"
#include "ConfigBackup.h"

using namespace SQM;

// The portable crypto behind settings backups (specs/033-config-backup,
// research R2), held to published vectors: RFC 7914 section 11 and the
// widely used PBKDF2-HMAC-SHA256 vectors, and NIST GCM test cases 13, 14 and
// 16 (AES-256). The expected values were also recomputed with OpenSSL.

namespace
{
    std::string fromHex(const char *hex)
    {
        std::string out;
        for (size_t i = 0; hex[i] != '\0' && hex[i + 1] != '\0'; i += 2)
        {
            const char pair[3] = {hex[i], hex[i + 1], '\0'};
            out += static_cast<char>(std::strtol(pair, nullptr, 16));
        }
        return out;
    }

    std::string hexOf(const std::string &bytes)
    {
        return Backup::toHex(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    }

    std::string derive(const char *passphrase, const char *salt, uint32_t iterations)
    {
        uint8_t key[Backup::KEY_BYTES];
        TEST_ASSERT_TRUE(Backup::deriveKey(passphrase, reinterpret_cast<const uint8_t *>(salt), std::strlen(salt), iterations, key));
        return Backup::toHex(key, sizeof(key));
    }

    template <size_t N> void copyTo(uint8_t (&out)[N], const std::string &bytes)
    {
        TEST_ASSERT_EQUAL(N, bytes.size());
        std::memcpy(out, bytes.data(), N);
    }
} // namespace

void test_pbkdf2_matches_published_vectors_when_one_iteration()
{
    TEST_ASSERT_EQUAL_STRING("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b", derive("password", "salt", 1).c_str());
}

void test_pbkdf2_matches_published_vectors_when_many_iterations()
{
    TEST_ASSERT_EQUAL_STRING("c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a", derive("password", "salt", 4096).c_str());
    // RFC 7914 section 11, first 32 bytes.
    TEST_ASSERT_EQUAL_STRING("4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56", derive("Password", "NaCl", 80000).c_str());
}

void test_pbkdf2_hashes_a_passphrase_longer_than_a_block()
{
    TEST_ASSERT_EQUAL_STRING(
        "59bfa49750dd5462ce38370a1e7abe0736ff334cf1c2f0d84f23a03435d660d1", derive(std::string(100, 'x').c_str(), "salt", 3).c_str());
}

void test_pbkdf2_refuses_zero_iterations()
{
    uint8_t key[Backup::KEY_BYTES];
    TEST_ASSERT_FALSE(Backup::deriveKey("password", reinterpret_cast<const uint8_t *>("salt"), 4, 0, key));
}

void test_gcm_matches_nist_case_13_and_14_when_key_is_zero()
{
    uint8_t key[Backup::KEY_BYTES] = {};
    uint8_t nonce[Backup::NONCE_BYTES] = {};
    std::string sealed;
    TEST_ASSERT_TRUE(Backup::seal(key, nonce, "", "", sealed));
    TEST_ASSERT_EQUAL_STRING("530f8afbc74536b9a963b4f1c4cb738b", hexOf(sealed).c_str());
    TEST_ASSERT_TRUE(Backup::seal(key, nonce, "", std::string(16, '\0'), sealed));
    TEST_ASSERT_EQUAL_STRING("cea7403d4d606b6e074ec5d3baf39d18d0d1c8a799996bf0265b98b5d48ab919", hexOf(sealed).c_str());
}

void test_gcm_matches_nist_case_16_with_associated_data()
{
    uint8_t key[Backup::KEY_BYTES];
    uint8_t nonce[Backup::NONCE_BYTES];
    copyTo(key, fromHex("feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308"));
    copyTo(nonce, fromHex("cafebabefacedbaddecaf888"));
    const std::string plain =
        fromHex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa"
                "0de657ba637b39");
    const std::string aad = fromHex("feedfacedeadbeeffeedfacedeadbeefabaddad2");
    std::string sealed;
    TEST_ASSERT_TRUE(Backup::seal(key, nonce, aad, plain, sealed));
    TEST_ASSERT_EQUAL_STRING(
        "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f6"
        "6276fc6ece0f4e1768cddf8853bb2d551b",
        hexOf(sealed).c_str());
    std::string opened;
    TEST_ASSERT_TRUE(Backup::open(key, nonce, aad, sealed, opened));
    TEST_ASSERT_TRUE(opened == plain);
}

void test_gcm_open_refuses_changed_data_tag_or_associated_data()
{
    uint8_t key[Backup::KEY_BYTES] = {1};
    uint8_t nonce[Backup::NONCE_BYTES] = {2};
    std::string sealed;
    Backup::seal(key, nonce, "aad", "the secrets", sealed);
    std::string plain;
    for (size_t i : {size_t(0), sealed.size() - 1})
    {
        std::string changed = sealed;
        changed[i] = static_cast<char>(changed[i] ^ 1);
        TEST_ASSERT_FALSE(Backup::open(key, nonce, "aad", changed, plain));
        TEST_ASSERT_TRUE(plain.empty());
    }
    TEST_ASSERT_FALSE(Backup::open(key, nonce, "other", sealed, plain));
    TEST_ASSERT_FALSE(Backup::open(key, nonce, "aad", sealed.substr(0, 10), plain));
    key[0] = 9;
    TEST_ASSERT_FALSE(Backup::open(key, nonce, "aad", sealed, plain));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_pbkdf2_matches_published_vectors_when_one_iteration);
    RUN_TEST(test_pbkdf2_matches_published_vectors_when_many_iterations);
    RUN_TEST(test_pbkdf2_hashes_a_passphrase_longer_than_a_block);
    RUN_TEST(test_pbkdf2_refuses_zero_iterations);
    RUN_TEST(test_gcm_matches_nist_case_13_and_14_when_key_is_zero);
    RUN_TEST(test_gcm_matches_nist_case_16_with_associated_data);
    RUN_TEST(test_gcm_open_refuses_changed_data_tag_or_associated_data);
    return UNITY_END();
}
