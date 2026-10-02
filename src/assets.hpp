#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace ashford {
using Bytes = std::vector<uint8_t>;
struct AfsEntry { uint32_t offset, size; };
std::vector<AfsEntry> afs_index(const Bytes& header, uint64_t file_size);
Bytes expand(const Bytes& source, size_t limit = 64 * 1024 * 1024);
struct RoomInfo {
    float version = 0;
    uint32_t cameras=0, enemies=0, objects=0, items=0, walls=0, floors=0;
    uint32_t model_objects=0;
};
RoomInfo room_info(const Bytes& data);
}
