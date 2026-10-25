#include "host/ashford_disc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main() {
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
    std::puts("Campaign disc validation error handling passed");
}
