#include "CaptiveDns.h"

namespace SQM
{
    namespace CaptiveDns
    {
        namespace
        {
            constexpr size_t HEADER = 12;
            constexpr uint16_t TYPE_A = 1;
            constexpr uint16_t CLASS_IN = 1;
            constexpr uint32_t TTL_SECONDS = 60;

            uint16_t read16(const uint8_t *p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
            void push16(std::vector<uint8_t> &out, uint16_t v)
            {
                out.push_back(static_cast<uint8_t>(v >> 8));
                out.push_back(static_cast<uint8_t>(v & 0xFF));
            }
        } // namespace

        bool buildResponse(const uint8_t *query, size_t len, const uint8_t ipv4[4], std::vector<uint8_t> &out)
        {
            out.clear();
            if (query == nullptr || len < HEADER + 5)
                return false;

            const bool isResponse = (query[2] & 0x80) != 0;
            const uint8_t opcode = (query[2] >> 3) & 0x0F;
            if (isResponse || opcode != 0 || read16(query + 4) != 1)
                return false;

            // Question name: length-prefixed labels ending in a zero byte.
            size_t pos = HEADER;
            while (pos < len && query[pos] != 0)
            {
                if ((query[pos] & 0xC0) != 0) // compression pointers don't belong in a question
                    return false;
                pos += 1 + query[pos];
            }
            if (pos + 5 > len)
                return false;
            const size_t questionEnd = pos + 5; // zero byte + type + class
            const uint16_t qtype = read16(query + pos + 1);
            const uint16_t qclass = read16(query + pos + 3);
            const bool answerA = qtype == TYPE_A && qclass == CLASS_IN;

            out.reserve(questionEnd + 16);
            out.push_back(query[0]); // id
            out.push_back(query[1]);
            out.push_back(static_cast<uint8_t>(0x84 | (query[2] & 0x01))); // QR, AA, keep RD
            out.push_back(0x00);                                           // RA=0, RCODE=0 (no error)
            push16(out, 1);                                                // QDCOUNT
            push16(out, answerA ? 1 : 0);                                  // ANCOUNT
            push16(out, 0);                                                // NSCOUNT
            push16(out, 0);                                                // ARCOUNT
            out.insert(out.end(), query + HEADER, query + questionEnd);

            if (answerA)
            {
                push16(out, 0xC00C); // pointer to the question name
                push16(out, TYPE_A);
                push16(out, CLASS_IN);
                push16(out, static_cast<uint16_t>(TTL_SECONDS >> 16));
                push16(out, static_cast<uint16_t>(TTL_SECONDS & 0xFFFF));
                push16(out, 4);
                out.insert(out.end(), ipv4, ipv4 + 4);
            }
            return true;
        }
    } // namespace CaptiveDns
} // namespace SQM
