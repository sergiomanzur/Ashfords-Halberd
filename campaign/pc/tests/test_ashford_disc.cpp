#include "host/ashford_disc.h"
#include "core.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

static bool verify_sha1_vector(const char* filename, const char* payload, const char* expected) {
    {
        std::ofstream out(filename, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(payload, static_cast<std::streamsize>(std::strlen(payload)));
    }
    std::string actual = ashford::sha1_file(ashford::fs::u8path(filename));
    std::error_code ec;
    ashford::fs::remove(ashford::fs::u8path(filename), ec);
    if (actual != expected) {
        std::fprintf(stderr, "SHA-1 mismatch for %s: got %s expected %s\n", filename, actual.c_str(), expected);
        return false;
    }
    return true;
}

int main() {
    if (!verify_sha1_vector("ashford_sha1_empty.tmp", "", "da39a3ee5e6b4b0d3255bfef95601890afd80709")) return 1;
    if (!verify_sha1_vector("ashford_sha1_abc.tmp", "abc", "a9993e364706816aba3e25717850c26c9cd0d89d")) return 1;
    if (!verify_sha1_vector("ashford_sha1_2blk.tmp", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "84983e441c3bd26ebaae4aa1f95129e5e54670f1")) return 1;
    char error[256]={};
    if (ashford_validate_disc("this-disc-does-not-exist.iso",error,sizeof error) || !error[0]) return 1;
    char small[1]={'x'};
    if (ashford_validate_disc("missing.iso",small,sizeof small) || small[0]) return 1;
    if (ashford_validate_disc(nullptr,error,sizeof error) || !error[0]) return 1;
    if (const char* iso=std::getenv("RECVX_ISO")) {
        if (!ashford_validate_disc(iso,error,sizeof error)) {
            std::fprintf(stderr,"Retail ISO validation failed: %s\n",error); return 1;
        }
        std::puts("Retail USA boot identity, executable SHA-1 and inventory verified");
    }
    std::puts("Campaign disc validation and SHA-1 vectors passed");
}
