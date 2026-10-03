#include "assets.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ashford {
namespace {
void range(const Bytes& b, uint64_t p, uint64_t n) {
    if (p>b.size() || n>b.size()-p) throw std::runtime_error("Asset range exceeds input");
}
uint32_t u32(const Bytes& b, size_t p) {
    range(b,p,4);
    return uint32_t(b[p]) | uint32_t(b[p+1])<<8 | uint32_t(b[p+2])<<16 | uint32_t(b[p+3])<<24;
}
float f32(const Bytes& b, size_t p) {
    auto bits=u32(b,p); float v; std::memcpy(&v,&bits,4); return v;
}
}
std::vector<AfsEntry> afs_index(const Bytes& header, uint64_t file_size) {
    range(header,0,8);
    if (header[0]!='A' || header[1]!='F' || header[2]!='S' ||
        (header[3]!=0 && header[3]!=0x20))
        throw std::runtime_error("Invalid AFS signature");
    auto count=u32(header,4);
    if (count==0 || count>10000) throw std::runtime_error("Invalid AFS entry count");
    const uint64_t table_end=8+uint64_t(count)*8;
    range(header,0,table_end);
    std::vector<AfsEntry> entries;
    for(uint32_t i=0;i<count;++i) {
        AfsEntry e{u32(header,8+i*8),u32(header,12+i*8)};
        if(e.offset<table_end || e.offset>file_size || e.size>file_size-e.offset)
            throw std::runtime_error("Invalid AFS extent");
        entries.push_back(e);
    }
    auto sorted=entries;
    std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.offset<b.offset;});
    uint64_t previous_end=table_end;
    for(auto e:sorted) {
        if(e.size && e.offset<previous_end) throw std::runtime_error("Overlapping AFS entries");
        previous_end=std::max(previous_end,uint64_t(e.offset)+e.size);
    }
    return entries;
}
// Adapted from the Expand algorithm and Clownacy's explanatory C version in
// AshfordFamily/recvx-decomp, src/ps2/veronica/prog/expand.c (MIT).
Bytes expand(const Bytes& source, size_t limit) {
    size_t cursor=0; unsigned flags=0,remaining=0; Bytes out;
    auto byte=[&]() -> unsigned {
        if(cursor==source.size()) throw std::runtime_error("Truncated Expand stream");
        return source[cursor++];
    };
    auto bit=[&]() -> unsigned {
        if(!remaining) {flags=byte();remaining=8;}
        auto value=flags&1; flags>>=1;--remaining;return value;
    };
    while(true) {
        if(bit()) {
            if(out.size()==limit) throw std::runtime_error("Expand output limit exceeded");
            out.push_back(uint8_t(byte()));
            continue;
        }
        size_t distance,count;
        if(bit()) {
            const auto low=byte(); const auto word=low | byte()<<8;
            if(!word) {
                if(cursor!=source.size()) throw std::runtime_error("Trailing Expand data");
                return out;
            }
            distance=8192-(word>>3);
            count=(word&7) ? (word&7)+2 : byte()+1;
        } else {
            const auto high=bit(); const auto low=bit();
            count=(high<<1)+low+2; distance=256-byte();
        }
        if(distance>out.size()) throw std::runtime_error("Invalid Expand back reference");
        if(count>limit-out.size()) throw std::runtime_error("Expand output limit exceeded");
        for(size_t i=0;i<count;++i) out.push_back(out[out.size()-distance]);
    }
}
RoomInfo room_info(const Bytes& data) {
    range(data,0,36); RoomInfo result;result.version=f32(data,0);
    if(!std::isfinite(result.version) || result.version<1.72f || result.version>100.f)
        throw std::runtime_error("Unsupported room version");
    const auto rom=u32(data,16);range(data,rom,1004);
    auto count=[&](size_t index) {
        auto n=u32(data,rom+0x80+index*4);
        if(n>100000) throw std::runtime_error("Invalid room record count");
        return n;
    };
    auto records=[&](size_t index,size_t stride) {
        const auto n=count(index),p=u32(data,rom+index*4);
        if(n && !p) throw std::runtime_error("Missing room record table");
        range(data,p,uint64_t(n)*stride);return n;
    };
    result.cameras=records(0,680);
    result.enemies=records(2,36);result.objects=records(3,36);result.items=records(4,36);
    result.walls=records(6,36);records(7,36);result.floors=records(8,36);records(9,16);
    // ROM_WORK's initial event pointer is replaced by outer-header +28 by
    // bhSetRoom; it must not be treated as a live file pointer here.
    const auto models=u32(data,20);
    if(models) {
        auto model=u32(data,models);range(data,model,16);
        if(data[model]!='M' || data[model+1]!='D' || data[model+2]!='L')
            throw std::runtime_error("Invalid room model signature");
        auto header_size=unsigned(data[model+4]) | unsigned(data[model+5])<<8;
        result.model_objects=unsigned(data[model+6]) | unsigned(data[model+7])<<8;
        if(header_size<16) throw std::runtime_error("Invalid model header size");
        range(data,model,header_size);
        range(data,uint64_t(model)+header_size+u32(data,model+12),uint64_t(result.model_objects)*52);
    }
    return result;
}
}
