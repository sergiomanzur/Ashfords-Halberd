#include "assets.hpp"
#include "scene.hpp"
#include "camera.hpp"
#include "collision.hpp"
#include "room.hpp"
#include "scenario.hpp"
#include "motion.hpp"
#include "actor.hpp"
#include "runtime.hpp"
#include "render.hpp"
#include <algorithm>
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
        bool runtime_check=argc==5 && std::wstring(argv[3])==L"--runtime-check";
        bool scenario_check=runtime_check || (argc==4 && std::wstring(argv[3])==L"--scenario-check");
        if(argc!=3 && !runtime_check && !(argc==4 && (std::wstring(argv[3])==L"--scene" || collision_check || scenario_check))) throw std::runtime_error("Usage: AshfordRoomInspect RDX_LNK.AFS report.tsv [--scene | --collision-check | --scenario-check | --runtime-check SYSTEM.AFS]");
        const std::filesystem::path archive(argv[1]), report(argv[2]);
        if(std::filesystem::exists(report) && std::filesystem::equivalent(archive,report))
            throw std::runtime_error("Report cannot overwrite archive");
        std::ifstream file(archive,std::ios::binary);
        if(!file) throw std::runtime_error("Could not open room archive");
        auto header=read(file,0,8);
        uint32_t count=uint32_t(header[4])|uint32_t(header[5])<<8|uint32_t(header[6])<<16|uint32_t(header[7])<<24;
        if(count>10000) throw std::runtime_error("Invalid archive count");
        auto entries=afs_index(read(file,0,8+size_t(count)*8),std::filesystem::file_size(archive));
        PlayerPackage player;
        if(runtime_check) {
            std::filesystem::path system_path(argv[4]);
            if(std::filesystem::exists(report) && std::filesystem::equivalent(system_path,report))
                throw std::runtime_error("Report cannot overwrite player archive");
            std::ifstream system(system_path,std::ios::binary);
            if(!system)throw std::runtime_error("Could not open player archive");
            auto sh=read(system,0,8);
            uint32_t n=uint32_t(sh[4])|uint32_t(sh[5])<<8|uint32_t(sh[6])<<16|uint32_t(sh[7])<<24;
            if(n>10000)throw std::runtime_error("Invalid player archive count");
            auto si=afs_index(read(system,0,8+size_t(n)*8),std::filesystem::file_size(system_path));
            if(si.size()<=10)throw std::runtime_error("Missing Claire player package");
            player=decode_player_package(read(system,si[10].offset,si[10].size));
        }
        // Collect the complete report before touching the destination.
        std::string text="index\tcompressed_bytes\texpanded_bytes\tversion\tcameras\tenemies\tobjects\titems\twalls\tfloors\tmodel_objects\ttriangles\ttextures\tactive_wall_types\tentry_positions\tentries_with_camera\n";
        size_t scenario_entries=0;
        size_t corridor_initializations=0;
        size_t runtime_entries=0;
        size_t runtime_ticks=0;
        size_t runtime_movement_ticks=0;
        size_t runtime_images=0;
        size_t runtime_entity_images=0;
        size_t corridor_controls=0;
        size_t corridor_door_requests=0;
        size_t room_motions=0, room_motion_poses=0;
        size_t enemy_motions=0, enemy_motion_poses=0;
        size_t corridor_actor_poses=0;
        size_t corridor_model_rebuilds=0;
        size_t corridor_lifecycles=0;
        size_t prop_models=0, prop_skins=0;
        for(size_t i=0;i<entries.size();++i) {
            try {
                auto data=expand(read(file,entries[i].offset,entries[i].size));
                auto r=room_info(data);
                auto collision=decode_room_collision(data);
                auto records=decode_room_records(data);
                auto scenario=decode_room_scenario(data);
                scenario_entries+=scenario.entry_count();
                if(scenario_check) {
                    ScenarioState bound_enemies;
                    bind_room_enemies(data,collision,bound_enemies);
                    bind_room_props(data,bound_enemies);
                    for(const auto* entities:{&bound_enemies.objects,&bound_enemies.items})
                        for(const auto& entity:*entities)if(entity.model) {
                            ++prop_models;if(!entity.model->skin.empty())++prop_skins;
                        }
                    auto enemy_motion_banks=decode_room_enemy_motion_banks(data);
                    std::vector<const MotionBank*> checked_banks;
                    for(const auto& enemy_bank:enemy_motion_banks) {
                        if(std::find(checked_banks.begin(),checked_banks.end(),enemy_bank.get())!=checked_banks.end())continue;
                        checked_banks.push_back(enemy_bank.get());
                        for(const auto& motion:*enemy_bank)if(motion) {
                            ++enemy_motions;
                            for(uint32_t frame:{0u,motion->frames/2,motion->frames-1}) {
                                sample_motion(*motion,int64_t(frame)*65536,true);
                                ++enemy_motion_poses;
                            }
                        }
                    }
                    auto bank=decode_room_motion_bank(data);
                    for(const auto& motion:bank) if(motion) {
                        ++room_motions;
                        for(uint32_t frame:{0u,motion->frames/2,motion->frames-1}) {
                            sample_motion(*motion,int64_t(frame)*65536,true);
                            ++room_motion_poses;
                        }
                    }
                    if(i==1 && !bank[0])
                        throw std::runtime_error("Retail corridor room motion 0 missing");
                    if(i==1) {
                        auto enemies=decode_room_enemy_models(data);
                        if(enemies.empty() || enemies[0].models.empty())
                            throw std::runtime_error("Retail corridor enemy model missing");
                        int32_t actor_frame=0;
                        uint32_t actor_direction=0;
                        for(uint32_t tick=0;tick<bank[0]->frames;++tick) {
                            auto posed=pose_actor(enemies[0].models[0],sample_motion(*bank[0],actor_frame));
                            if(posed.meshes.empty() || posed.textures.empty())
                                throw std::runtime_error("Retail corridor actor pose missing geometry or textures");
                            ++corridor_actor_poses;
                            int boundary=advance_motion_frame(bank[0]->frames,65536,0x20,actor_frame,actor_direction);
                            if(boundary!=(tick+1==bank[0]->frames?1:0))
                                throw std::runtime_error("Retail corridor animation boundary mismatch");
                        }
                        if(actor_frame!=0 || actor_direction!=0)
                            throw std::runtime_error("Retail corridor animation did not wrap");
                        ScenarioState controller=std::move(bound_enemies);
                        controller.scenario=std::make_shared<const RoomScenario>(scenario);
                        controller.collision=collision;
                        controller.room_motions=std::make_shared<const std::vector<std::optional<Motion>>>(bank);
                        controller.flags[6][0]=0x12;
                        controller.flags[4][0]=0x80000000;
                        tick_scenario_actors(controller);
                        int32_t startup_frame=(*enemy_motion_banks[0])[0]?32768:0;
                        if(controller.enemies[0].frame!=startup_frame || controller.enemies[0].modes[3]!=3 ||
                           !controller.enemies[0].rendered_model || controller.enemies[0].rendered_model->meshes.empty())
                            throw std::runtime_error("Retail corridor startup actor mismatch");
                        tick_scenario_actors(controller);
                        if(controller.enemies[0].frame!=startup_frame)throw std::runtime_error("Retail corridor startup hold mismatch");
                        start_scenario_event(controller,0,0);
                        tick_scenario_tasks(controller);
                        auto hold_pc=controller.tasks[0].pc;
                        tick_scenario_tasks(controller);
                        if(!controller.tasks[0].active || controller.tasks[0].pc!=hold_pc ||
                           controller.enemies[0].motion_mode!=0x20 || controller.enemies[0].blend_count!=16 ||
                           controller.enemies[0].motions!=controller.room_motions || !controller.enemies[0].rendered_model ||
                           controller.enemies[0].rendered_model->meshes.empty() ||
                           controller.enemies[0].rendered_model->textures.empty() ||
                           controller.enemies[0].position.x!=0 || controller.enemies[0].position.y!=0 || controller.enemies[0].position.z!=0)
                            throw std::runtime_error("Retail corridor model-rebuild prefix mismatch");
                        for(size_t tick=0;tick<149;++tick) {
                            tick_scenario_actors(controller);
                            tick_scenario_tasks(controller);
                        }
                        if(controller.enemies[0].frame!=149*65536 || controller.enemies[0].blend_count!=0 || controller.enemies[0].joint_pose.empty())
                            throw std::runtime_error("Retail corridor native actor advancement mismatch");
                        tick_scenario_actors(controller);
                        tick_scenario_tasks(controller);
                        if(controller.tasks[0].active || (controller.flags[4][0]&0x80000000) || controller.enemies[0].frame!=0 || controller.enemies[0].motion_boundary!=1)
                            throw std::runtime_error("Retail corridor controller failed to finish");
                        ++corridor_model_rebuilds;
                    }
                }
                if(scenario_check && i==1) {
                    // RM_0010 initialization, both map-flag paths and both
                    // item-persistence states. No event commands are skipped.
                    for(bool visited:{false,true})for(bool collected:{false,true}) {
                        ScenarioState state;
                        bind_room_enemies(data,collision,state);
                        bind_room_props(data,state);
                        state.scenario=std::make_shared<const RoomScenario>(scenario);
                        state.collision=collision;
                        state.room_motions=std::make_shared<const MotionBank>(decode_room_motion_bank(data));
                        state.flags[6][0]=2;
                        state.interactions=collision.miscellaneous;
                        state.flags[8][1]=visited?0x20000000u:0;
                        state.flags[7][3]=collected?0x18u:0;
                        if(runtime_check) {
                            auto runtime=load_room_runtime(data,0,1,0,std::move(state),player);
                            if(runtime.world.player.frame!=65536 || runtime.player_pose.empty() ||
                               runtime.player_scene.meshes.empty() || runtime.scene.meshes.empty())
                                throw std::runtime_error("Retail corridor runtime entry mismatch");
                            ++runtime_entries;
                            auto running=runtime;
                            auto translations=decode_player_translation_tables(player.auxiliary);
                            for(size_t tick=0;tick<240;++tick) {
                                auto previous=running.world.player.position;
                                try {tick_room_runtime(running,player,translations,tick<180?0:1);}
                                catch(const std::exception& error) {
                                    throw std::runtime_error("Runtime tick "+std::to_string(tick)+": "+error.what());
                                }
                                if(running.scenario.player_frame!=running.world.player.frame)
                                    throw std::runtime_error("Runtime player/scenario frame diverged");
                                if(previous.x!=running.world.player.position.x || previous.z!=running.world.player.position.z)
                                    ++runtime_movement_ticks;
                                ++runtime_ticks;
                                if(tick%60==0 && running.world.cut) {
                                    auto combined=compose_room_runtime(running,0xffffffff,0);
                                    auto image=render_camera(combined,resolve_camera(running.cuts.at(*running.world.cut),0),320,240);
                                    if(image.rgb.size()!=320*240*3)throw std::runtime_error("Invalid runtime rendered frame");
                                    auto room_only=render_camera(running.scene,resolve_camera(running.cuts.at(*running.world.cut),0),320,240);
                                    if(image.rgb!=room_only.rgb)++runtime_entity_images;
                                    ++runtime_images;
                                }
                            }
                            if(!collected && !running.scenario.items[0].rendered_model)
                                throw std::runtime_error("Runtime pickup model was not rebuilt");
                            state=std::move(runtime.scenario);
                        } else initialize_scenario(state);
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
                        if(!visited && !collected) {
                            auto lifecycle=state;
                            control_scenario(lifecycle);
                            if(!lifecycle.tasks[0].active || lifecycle.event_timer!=1 ||
                               lifecycle.enemies[0].motions!=lifecycle.room_motions)
                                throw std::runtime_error("Retail corridor lifecycle did not start event");
                            auto wait_pc=lifecycle.tasks[0].pc;
                            control_scenario(lifecycle);
                            if(lifecycle.tasks[0].pc!=wait_pc || lifecycle.event_timer!=2)
                                throw std::runtime_error("Retail corridor lifecycle did not retain wait");
                            for(size_t tick=0;tick<150;++tick) {
                                tick_scenario_actors(lifecycle);
                                control_scenario(lifecycle);
                            }
                            if(lifecycle.tasks[0].active || lifecycle.enemies[0].frame!=0 ||
                               lifecycle.enemies[0].motion_boundary!=1)
                                throw std::runtime_error("Retail corridor lifecycle did not finish event");
                            ++corridor_lifecycles;
                        }
                        for(int weapon:{0,1}) {
                            auto controlled=state;
                            controlled.scenario=std::make_shared<RoomScenario>(scenario);
                            controlled.equipped_weapon=weapon;
                            run_scenario(scenario.program(1),controlled);
                            // Collected pickups skip the speed command and
                            // retain their authored value, rather than zero.
                            int scripted_speed=weapon==1?100:0;
                            if(!(controlled.flags[4][0]&0x80000000u) ||
                               !(controlled.flags[6][0]&0x10) || !controlled.tasks[0].active ||
                               controlled.items[0].angular_speed!=(collected?state.items[0].angular_speed:scripted_speed) ||
                               controlled.items[1].angular_speed!=(collected?state.items[1].angular_speed:scripted_speed))
                                throw std::runtime_error("Retail corridor control mismatch: visited="+std::to_string(visited)+" collected="+std::to_string(collected)+" weapon="+std::to_string(weapon)+" speeds="+std::to_string(controlled.items[0].angular_speed)+","+std::to_string(controlled.items[1].angular_speed)+" initial="+std::to_string(state.items[0].angular_speed)+","+std::to_string(state.items[1].angular_speed));
                            ++corridor_controls;
                            controlled.tasks[0].pc=1;
                            run_scenario(scenario.program(1),controlled);
                            if(controlled.tasks[0].pc!=1 || controlled.event_timer!=0)
                                throw std::runtime_error("Retail corridor event restarted unexpectedly");
                            ++corridor_controls;
                        }
                        if(visited)for(bool acquired:{false,true})for(bool event_done:{false,true}) {
                            auto controlled=state;
                            controlled.scenario=std::make_shared<RoomScenario>(scenario);
                            controlled.flags[10][0]=0x100;
                            controlled.interaction_index=0;
                            controlled.flags[7][0]=acquired?0x08000000u:0;
                            controlled.flags[1][3]=event_done?0x80u:0;
                            run_scenario(scenario.program(1),controlled);
                            if(!controlled.door || controlled.door->stage!=0 || controlled.door->room!=2 ||
                               controlled.door->position!=0 || controlled.door->type!=1 ||
                               controlled.room_case!=(acquired?1:0) ||
                               controlled.room_sound_case!=(acquired && event_done?1:0))
                                throw std::runtime_error("Retail corridor scripted door mismatch");
                            ++corridor_door_requests;
                        }
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
            std::cout<<"Decoded "<<prop_models<<" room object/item models, including "<<prop_skins<<" skinned objects\n";
            if(runtime_check)std::cout<<"Loaded "<<runtime_entries<<" native corridor runtime entries with Claire pose and initialized scenario state\n";
            if(runtime_check)std::cout<<"Advanced "<<runtime_ticks<<" joined player/actor/prop/scenario runtime ticks\n";
            if(runtime_check)std::cout<<"Applied player displacement on "<<runtime_movement_ticks<<" runtime ticks\n";
            if(runtime_check)std::cout<<"Rendered "<<runtime_images<<" joined runtime camera frames headlessly\n";
            if(runtime_check && !runtime_entity_images)throw std::runtime_error("No runtime entities contributed visible pixels");
            if(runtime_check)std::cout<<"Runtime entities contributed visible pixels in "<<runtime_entity_images<<" camera frames\n";
            std::cout<<"Decoded "<<enemy_motions<<" enemy motions and sampled "<<enemy_motion_poses<<" poses\n";
            std::cout<<"Decoded "<<room_motions<<" room motions and sampled "<<room_motion_poses<<" poses\n";
            std::cout<<"Posed "<<corridor_actor_poses<<" retail corridor actor frames\n";
            std::cout<<"Executed "<<corridor_model_rebuilds<<" complete corridor events with native blended actor advancement through frame 149\n";
            if(corridor_initializations!=4)throw std::runtime_error("Missing retail corridor scenario check");
            std::cout<<"Executed four retail corridor initialization states\n";
            if(corridor_lifecycles!=1)throw std::runtime_error("Missing retail corridor lifecycle check");
            std::cout<<"Executed one complete native corridor initialization/control/actor/event sequence\n";
            if(corridor_controls!=16)throw std::runtime_error("Missing retail corridor control check");
            std::cout<<"Executed sixteen retail corridor idle control passes\n";
            if(corridor_door_requests!=8)throw std::runtime_error("Missing retail corridor door checks");
            std::cout<<"Executed eight retail corridor scripted door requests\n";
        }
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
