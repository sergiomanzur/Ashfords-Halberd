#include "assets.hpp"
#include "scene.hpp"
#include "camera.hpp"
#include "collision.hpp"
#include "room.hpp"
#include "scenario.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ashford;
Bytes read(std::ifstream& file, uint64_t offset, size_t size) {
    if(size>64*1024*1024) throw std::runtime_error("Archive read exceeds limit");
    Bytes bytes(size);file.seekg(std::streamoff(offset));
    if(!file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(size)))
        throw std::runtime_error("Could not read archive extent");
    return bytes;
}
int wmain(int argc,wchar_t** argv) {
    try {
        bool collision_check=argc==4 && std::wstring(argv[3])==L"--collision-check";
        bool scenario_check=argc==4 && std::wstring(argv[3])==L"--scenario-check";
        if(argc!=3 && !(argc==4 && (std::wstring(argv[3])==L"--scene" || collision_check || scenario_check))) throw std::runtime_error("Usage: AshfordRoomInspect RDX_LNK.AFS report.tsv [--scene | --collision-check | --scenario-check]");
        const std::filesystem::path archive(argv[1]), report(argv[2]);
        if(std::filesystem::exists(report) && std::filesystem::equivalent(archive,report))
            throw std::runtime_error("Report cannot overwrite archive");
        std::ifstream file(archive,std::ios::binary);
        if(!file) throw std::runtime_error("Could not open room archive");
        auto header=read(file,0,8);
        uint32_t count=uint32_t(header[4])|uint32_t(header[5])<<8|uint32_t(header[6])<<16|uint32_t(header[7])<<24;
        if(count>10000) throw std::runtime_error("Invalid archive count");
        auto entries=afs_index(read(file,0,8+size_t(count)*8),std::filesystem::file_size(archive));
        // Collect the complete report before touching the destination.
        std::string text="index\tcompressed_bytes\texpanded_bytes\tversion\tcameras\tenemies\tobjects\titems\twalls\tfloors\tmodel_objects\ttriangles\ttextures\tactive_wall_types\tentry_positions\tentries_with_camera\n";
        size_t scenario_entries=0;
        size_t corridor_initializations=0;
        for(size_t i=0;i<entries.size();++i) {
            try {
                auto data=expand(read(file,entries[i].offset,entries[i].size));
                auto r=room_info(data);
                auto collision=decode_room_collision(data);
                auto records=decode_room_records(data);
                auto scenario=decode_room_scenario(data);
                scenario_entries+=scenario.entry_count();
                if(scenario_check && i==1) {
                    // RM_0010 initialization, both map-flag paths and both
                    // item-persistence states. No event commands are skipped.
                    for(bool visited:{false,true})for(bool collected:{false,true}) {
                        ScenarioState state;
                        state.interactions=collision.miscellaneous;
                        state.items.resize(records.items.size());
                        state.flags[8][1]=visited?0x20000000u:0;
                        state.flags[7][3]=collected?0x18u:0;
                        run_scenario(scenario.program(0),state);
                        if(state.interactions.size()<4 || state.items.size()<2 ||
                           state.interactions[0].type!=(visited?3:0) ||
                           state.interactions[0].attribute!=(visited?0u:0x80u) ||
                           state.interactions[0].parameters[1]!=(visited?0:2) ||
                           bool(state.items[0].state_flags&0x1000000)!=collected ||
                           bool(state.items[1].state_flags&0x1000000)!=collected ||
                           bool(state.interactions[2].flags&1)==collected ||
                           bool(state.interactions[3].flags&1)==collected)
                            throw std::runtime_error("Retail corridor initialization mismatch");
                        ++corridor_initializations;
                    }
                }
                auto cuts=decode_room_cuts(data);
                size_t entries_with_camera=0;
                for(size_t position=0;position<records.positions.size();++position)
                    if(enter_room(records,position,collision,cuts).cut) ++entries_with_camera;
                std::array<size_t,256> types{};
                for(const auto& wall:collision.walls) if(wall.flags&1) ++types[wall.type];
                std::string collision_column;
                for(size_t type=0;type<types.size();++type) if(types[type]) {
                    if(!collision_column.empty()) collision_column+=",";
                    collision_column+=std::to_string(type)+":"+std::to_string(types[type]);
                }
                std::string scene_columns="\t\t";
                if(collision_check) {
                    for(const auto& wall:collision.walls) if(wall.flags&1) {
                        for(float fraction:{0.f,.25f,.5f,.75f,1.f}) {
                            CollisionBody body;body.floor=wall.floor;
                            if(body.floor<-2 || body.floor>29) body.floor=0;
                            body.position={wall.position.x+wall.size.x*fraction,wall.position.y,wall.position.z+wall.size.z*fraction};
                            body.flags=0x100;
                            collide_player_walls(collision,body);
                            auto previous=body.position;previous.x-=1;previous.z-=1;
                            collide_player_walls_ex(collision,body,previous);
                        }
                    }
                }
                if(argc==4 && !collision_check && !scenario_check) {
                    auto scene=decode_room_scene(data);size_t triangles=0;
                    decode_room_cuts(data);
                    for(const auto& mesh:scene.meshes)triangles+=mesh.triangles.size();
                    scene_columns="\t"+std::to_string(triangles)+"\t"+std::to_string(scene.textures.size());
                }
                text+=std::to_string(i)+"\t"+std::to_string(entries[i].size)+"\t"+std::to_string(data.size())+"\t"+std::to_string(r.version)+"\t"+std::to_string(r.cameras)+"\t"+std::to_string(r.enemies)+"\t"+std::to_string(r.objects)+"\t"+std::to_string(r.items)+"\t"+std::to_string(r.walls)+"\t"+std::to_string(r.floors)+"\t"+std::to_string(r.model_objects)+scene_columns+"\t"+collision_column+"\t"+std::to_string(records.positions.size())+"\t"+std::to_string(entries_with_camera)+"\n";
            } catch(const std::exception& e) {
                throw std::runtime_error("Room "+std::to_string(i)+": "+e.what());
            }
        }
        std::ofstream output(report,std::ios::binary|std::ios::trunc);
        output<<text;output.flush();
        if(!output) throw std::runtime_error("Could not write room report");
        std::cout<<"Validated "<<entries.size()<<" rooms and "<<scenario_entries<<" scenario entry slots\n";
        if(scenario_check) {
            if(corridor_initializations!=4)throw std::runtime_error("Missing retail corridor scenario check");
            std::cout<<"Executed four retail corridor initialization states\n";
        }
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
