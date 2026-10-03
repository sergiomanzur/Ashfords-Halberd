#include "assets.hpp"
#include "scene.hpp"
#include "camera.hpp"
#include "collision.hpp"
#include "room.hpp"
#include "scenario.hpp"
#include "motion.hpp"
#include "actor.hpp"
#include "face_model.hpp"
#include "face_parameters.hpp"
#include "runtime.hpp"
#include "zombie.hpp"
#include "render.hpp"
#include <algorithm>
#include <cmath>
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
        bool transition_check=argc==5 && std::wstring(argv[3])==L"--transition-check";
        bool runtime_check=transition_check || (argc==5 && std::wstring(argv[3])==L"--runtime-check");
        bool scenario_check=runtime_check || (argc==4 && std::wstring(argv[3])==L"--scenario-check");
        if(argc!=3 && !runtime_check && !(argc==4 && (std::wstring(argv[3])==L"--scene" || collision_check || scenario_check))) throw std::runtime_error("Usage: AshfordRoomInspect RDX_LNK.AFS report.tsv [--scene | --collision-check | --scenario-check | --runtime-check SYSTEM.AFS | --transition-check SYSTEM.AFS]");
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
        size_t zombie_linked_work=0;
        size_t zombie_parameters=0;
        size_t zombie_joint_samples=0, zombie_linked_poses=0;
        size_t zombie_channel_applications=0;
        size_t zombie_startups=0, zombie_auxiliary_startups=0;
        size_t zombie_startup_links=0, zombie_managers=0;
        size_t zombie_model_work_samples=0, zombie_range_draws=0;
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
                    if(i==2 || i==3) {
                        for(size_t slot=0;slot<bound_enemies.enemies.size();++slot) {
                            const auto& actor=bound_enemies.enemies[slot];
                            if((actor.flags&1) && actor.id==91) {
                                const auto raw_skin=decode_skin(actor.model->skin,actor_bind_pose(*actor.model).size());
                                std::cout<<"Graveyard subplayer dependency: room="<<i<<" slot="<<slot
                                         <<" joints="<<actor_bind_pose(*actor.model).size()
                                         <<" skin_bytes="<<actor.model->skin.size()
                                         <<" mask_bytes="<<actor.model_bank->mask.size()<<'\n';
                                const auto facial=prepare_face_model(*actor.model,actor.model_bank->mask);
                                const auto expanded=actor_bind_pose(facial.actor);
                                const auto facial_scene=pose_actor(facial.actor,expanded,actor.position,uint16_t(actor.angles[1]));
                                if(raw_skin.size()!=27 || expanded.size()!=31 || facial_scene.meshes.empty())
                                    throw std::runtime_error("Graveyard facial preparation differs from selected model");
                                std::cout<<"Prepared subplayer joints="<<expanded.size()
                                         <<" meshes="<<facial_scene.meshes.size()<<'\n';
                                const auto track=decode_face_track(actor.model_bank->mask);
                                if(track.empty())throw std::runtime_error("Selected facial model has no shape keys");
                                const auto first=sample_face_track(track,0);
                                const auto last=sample_face_track(track,float(track.back().frame));
                                (void)first;(void)last;
                                size_t sampled_shapes=0;
                                for(float frame=0;frame<=track.back().frame;frame+=0.5f) {
                                    (void)sample_face_track(track,frame);
                                    ++sampled_shapes;
                                }
                                for(const auto& key:track) {
                                    const auto exact=sample_face_track(track,float(key.frame));
                                    if(exact.muscle!=key.parameters.muscle)
                                        throw std::runtime_error("Facial sampling changed an authored muscle key");
                                }
                                std::cout<<"Selected facial shape keys="<<track.size()
                                         <<" last_frame="<<track.back().frame
                                         <<" samples="<<sampled_shapes<<'\n';
                            }
                        }
                        auto linked=bound_enemies;
                        linked.collision=collision;
                        size_t parents=0,bindings=0;
                        // Verify required linked ownership against real graveyard
                        // assets, separately from the pending zombie controller.
                        for(size_t root=0;root<bound_enemies.enemies.size();++root)
                            if((bound_enemies.enemies[root].flags&1) && bound_enemies.enemies[root].id==1) {
                                ++parents;
                                auto& zombie=linked.enemies[root];
                                zombie.health=roll_zombie_health(linked,zombie.model_version);
                                zombie.zombie_walk_choice=0; // calloc'd extended walking work.
                                initialize_zombie_type(linked,root);
                                if(!zombie.zombie_walk_choice || zombie.modes[0]!=1)
                                    throw std::runtime_error("Retail zombie type initialization mismatch");
                                const auto choice=*zombie.zombie_walk_choice;
                                if(zombie.health<=0 || !zombie.motions)
                                    throw std::runtime_error("Retail zombie startup parameters mismatch");
                                for(bool body:{false,true}) {
                                    const auto motion=zombie_walk_motion(choice,body);
                                    if(motion>=zombie.motions->size() || !(*zombie.motions)[motion] ||
                                       !(*zombie.motions)[motion]->frames)
                                        throw std::runtime_error("Retail zombie walking motion missing");
                                    sample_motion(*(*zombie.motions)[motion],0,true);
                                }
                                for(uint32_t motion:{zombie.motion_number,202u}) {
                                    if(motion>=zombie.motions->size() || !(*zombie.motions)[motion] || !(*zombie.motions)[motion]->frames)
                                        throw std::runtime_error("Retail zombie startup motion missing");
                                    sample_motion(*(*zombie.motions)[motion],0,true);
                                }
                                // en01_flipTree / en01_flipTree2; body channels
                                // address the complete skeleton starting at 8.
                                for(bool body:{false,true}) {
                                    const std::vector<int8_t> flip=body
                                        ? std::vector<int8_t>{0,1,2,3,7,8,9,4,5,6,-1}
                                        : std::vector<int8_t>{0,1,5,6,7,2,3,4,8,9,10,11,15,16,17,12,13,14,-1};
                                    for(uint32_t number:{body?202u:zombie.motion_number,zombie_walk_motion(choice,body)}) {
                                        const auto& motion=*(*zombie.motions)[number];
                                        for(uint32_t frame:{0u,motion.frames/2,motion.frames-1})
                                            for(bool mirrored:{false,true})
                                                for(bool blended:{false,true}) {
                                                    auto bind=actor_bind_pose(*zombie.model);
                                                    MotionApplicationOptions options;
                                                    options.mode=mirrored?2:0;
                                                    options.joint_base=body?8:0;
                                                    options.mirror_map=flip;
                                                    if(blended)options.blend_rate=.35f;
                                                    auto applied=apply_motion_pose(bind,motion,int64_t(frame)*65536,options);
                                                    auto expected_matrices=actor_joint_matrices(*zombie.model,applied.pose);
                                                    auto native_work=calculate_actor_model_work(*zombie.model,applied.pose);
                                                    if(applied.pose.size()!=bind.size() || expected_matrices.size()!=bind.size() || native_work.joints.size()!=bind.size())
                                                        throw std::runtime_error("Retail zombie motion application changed joint count");
                                                    for(size_t joint=0;joint<bind.size();++joint)
                                                        for(size_t element=0;element<16;++element)
                                                            if(std::abs(native_work.joints[joint].matrix[element]-expected_matrices[joint][element])>.0001)
                                                                throw std::runtime_error("Retail linear model work differs from hierarchy matrix");
                                                    ++zombie_model_work_samples;
                                                    if(body)
                                                        for(size_t joint=0;joint<8;++joint) {
                                                            const auto a=*bind[joint].position,b=*applied.pose[joint].position;
                                                            if(a.x!=b.x || a.y!=b.y || a.z!=b.z || bind[joint].angles!=applied.pose[joint].angles)
                                                                throw std::runtime_error("Retail body motion changed joints before offset eight");
                                                        }
                                                    if(!blended) {
                                                        const bool slow=motion.flags&0x20,fast=!slow && !(motion.flags&0x10);
                                                        const auto sampled=sample_motion(motion,int64_t(frame)*65536,slow);
                                                        for(size_t track=0;track<sampled.size();++track) {
                                                            const auto destination=options.joint_base+(mirrored?size_t(flip.at(track)):track);
                                                            const auto& q=applied.pose.at(destination);
                                                            if((!fast || !track) && sampled[track].position) {
                                                                auto p=*sampled[track].position;if(mirrored)p.x=-p.x;
                                                                if(q.position->x!=p.x || q.position->y!=p.y || q.position->z!=p.z)
                                                                    throw std::runtime_error("Retail zombie translation channel mapping mismatch");
                                                            }
                                                            if(sampled[track].angles) {
                                                                auto angles=*sampled[track].angles;
                                                                if(mirrored){angles[1]=uint16_t(-int(angles[1]));angles[2]=uint16_t(-int(angles[2]));}
                                                                if(q.angles!=angles)throw std::runtime_error("Retail zombie angle channel mapping mismatch");
                                                            }
                                                        }
                                                    }
                                                    ++zombie_channel_applications;
                                                }
                                    }
                                }
                                ++zombie_parameters;
                                const auto& startup=*(*zombie.motions)[zombie.motion_number];
                                std::vector<std::vector<JointPose>> startup_poses;
                                std::vector<std::vector<ModelMatrix>> startup_joints;
                                std::vector<Scene> local_scenes;
                                const auto original_model=zombie.model->model;
                                const auto original_skin=zombie.model->skin;
                                for(uint32_t frame:{0u,startup.frames/2,startup.frames-1}) {
                                    startup_poses.push_back(sample_motion(startup,int64_t(frame)*65536,true));
                                    startup_joints.push_back(actor_joint_matrices(*zombie.model,startup_poses.back(),zombie.position,zombie.angles));
                                    if(startup_joints.back().size()<2)
                                        throw std::runtime_error("Retail zombie skeleton has no linked body joint");
                                    local_scenes.push_back(pose_actor(*zombie.model,startup_poses.back()));
                                    if(local_scenes.back().meshes.empty())
                                        throw std::runtime_error("Retail zombie startup pose has no geometry");
                                    ++zombie_joint_samples;
                                }
                                for(const auto part:std::array<std::pair<int,uint16_t>,6>{
                                    {{1,1},{0,31},{0,32},{0,32},{0,33},{0,33}}}) {
                                    auto slot=link_zombie_enemy(linked,root,part.first,part.second);
                                    if(!slot || linked.enemies[*slot].linked_parent!=root ||
                                       linked.enemies[*slot].model!=bound_enemies.enemies[root].model ||
                                       linked.enemies[*slot].motions!=bound_enemies.enemies[root].motions)
                                        throw std::runtime_error("Retail linked zombie work mismatch");
                                    ++zombie_linked_work;
                                    ++bindings;
                                    const auto& child=linked.enemies[*slot];
                                    // Whole-model placement probe, not partial-body
                                    // sampling or complete controller initialization.
                                    for(size_t frame=0;frame<startup_poses.size();++frame) {
                                        const auto& parent=startup_joints[frame].at(size_t(child.linked_joint));
                                        auto posed=pose_actor_linked(*child.model,startup_poses[frame],parent,child.linked_offset,child.angles);
                                        const auto& local=local_scenes[frame];
                                        if(posed.meshes.size()!=local.meshes.size() || posed.textures.size()!=local.textures.size())
                                            throw std::runtime_error("Retail linked pose changed geometry or texture count");
                                        size_t corners=0;
                                        for(size_t mesh=0;mesh<posed.meshes.size();++mesh) {
                                            if(posed.meshes[mesh].triangles.size()!=local.meshes[mesh].triangles.size())
                                                throw std::runtime_error("Retail linked pose changed triangle count");
                                            for(size_t triangle=0;triangle<posed.meshes[mesh].triangles.size();++triangle)
                                                for(size_t corner=0;corner<3;++corner) {
                                                    const auto p=local.meshes[mesh].triangles[triangle].corners[corner].position;
                                                    const auto q=posed.meshes[mesh].triangles[triangle].corners[corner].position;
                                                    if(std::abs(q.x-(parent[0]*p.x+parent[1]*p.y+parent[2]*p.z+parent[3]))>.05 ||
                                                       std::abs(q.y-(parent[4]*p.x+parent[5]*p.y+parent[6]*p.z+parent[7]))>.05 ||
                                                       std::abs(q.z-(parent[8]*p.x+parent[9]*p.y+parent[10]*p.z+parent[11]))>.05)
                                                        throw std::runtime_error("Retail linked pose differs from parent joint placement");
                                                    ++corners;
                                                }
                                        }
                                        if(!corners)throw std::runtime_error("Retail linked pose has no triangle corners");
                                        ++zombie_linked_poses;
                                    }
                                }
                                if(zombie.model->model!=original_model || zombie.model->skin!=original_skin)
                                    throw std::runtime_error("Retail linked posing mutated cached assets");
                            }
                        if(!parents || bindings!=parents*6)
                            throw std::runtime_error("Missing retail linked zombie work coverage");
                        // Exercise the selected native main Init paths, including
                        // the extra first-root manager and shared body ML_WORK.
                        auto initialized=bound_enemies;
                        initialized.collision=collision;
                        std::vector<size_t> roots;
                        for(size_t slot=0;slot<bound_enemies.enemies.size();++slot)
                            if((bound_enemies.enemies[slot].flags&1) && bound_enemies.enemies[slot].id==1)
                                roots.push_back(slot);
                        for(auto root:roots) {
                            initialize_zombie_work(initialized,root);
                            const auto& work=initialized.enemies[root];
                            if(!work.zombie_work || !work.zombie_work->body || work.modes[0]!=1 || work.health<=0)
                                throw std::runtime_error("Retail native zombie root Init mismatch");
                            ++zombie_startups;
                        }
                        for(size_t body=0;body<initialized.enemies.size();++body) {
                            if(!(initialized.enemies[body].flags&1) || (bound_enemies.enemies[body].flags&1))continue;
                            const auto id=initialized.enemies[body].id;
                            if(id!=1) {
                                initialize_zombie_auxiliary(initialized,body);
                                const auto& part=initialized.enemies[body];
                                if(part.modes!=std::array<uint8_t,4>{1,0,0,0} || part.state_flags || !(part.flags&0x8000) ||
                                   (id!=38 && (!part.zombie_part_work || part.zombie_part_work->words.size()!=(id==31?8u:28u))))
                                    throw std::runtime_error("Retail native zombie auxiliary Init mismatch");
                                ++zombie_auxiliary_startups;
                                continue;
                            }
                            const auto root=*initialized.enemies[body].pose_owner;
                            initialize_zombie_work(initialized,body);
                            const auto& owner=initialized.enemies[root];
                            const auto& work=initialized.enemies[body];
                            if(work.pose_owner!=root || work.health!=owner.health || !work.joint_pose.empty() ||
                               work.motion_number!=202 || work.modes[0]!=1 || work.geometry.height<=0 ||
                               owner.geometry.capsule_radius!=2 || !owner.rendered_model || owner.rendered_model->meshes.empty())
                                throw std::runtime_error("Retail native zombie body Init/capsule mismatch");
                            for(const auto& point:{owner.geometry.capsule_start,owner.geometry.capsule_end})
                                if(!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
                                    throw std::runtime_error("Retail native zombie capsule is not finite");
                            ++zombie_startups;
                        }
                        size_t allocated=0,managers=0;
                        for(size_t slot=0;slot<initialized.enemies.size();++slot)
                            if((initialized.enemies[slot].flags&1) && !(bound_enemies.enemies[slot].flags&1)) {
                                const auto& work=initialized.enemies[slot];
                                ++allocated;
                                if(!work.pose_owner || std::find(roots.begin(),roots.end(),*work.pose_owner)==roots.end())
                                    throw std::runtime_error("Retail startup linked model owner mismatch");
                                if(work.id==38) {
                                    if(work.linked_parent || work.flags&0x80 || !(work.model_flags&1))
                                        throw std::runtime_error("Retail startup manager visibility/parent mismatch");
                                    ++managers;
                                }
                            }
                        if(allocated!=roots.size()*6+1 || managers!=1)
                            throw std::runtime_error("Retail native startup allocation count mismatch");
                        zombie_startup_links+=allocated;
                        zombie_managers+=managers;
                        for(auto root:roots) for(bool upper:{false,true}) {
                            const auto& owner=initialized.enemies[root];
                            if(!owner.model_work)throw std::runtime_error("Missing retail zombie model work");
                            auto suffix=*owner.model_work;
                            suffix.first_joint=upper?8:0;
                            suffix.joint_count=upper?suffix.joints.size()-8:8;
                            for(auto& joint:suffix.joints)joint.flags|=1;
                            ActorPlacement displaced;
                            displaced.position={1000,2000,3000};
                            suffix=calculate_actor_model_work(*owner.model,owner.joint_pose,suffix,displaced);
                            for(size_t joint=0;joint<suffix.joints.size();++joint)
                                if(suffix.joints[joint].matrix!=owner.model_work->joints[joint].matrix)
                                    throw std::runtime_error("Retail frozen suffix changed cached matrices");
                            auto drawn=pose_actor_model_work(*owner.model,owner.joint_pose,suffix);
                            std::vector<const Mesh*> expected;
                            for(const auto& mesh:owner.rendered_model->meshes)
                                if(mesh.object>=512+suffix.first_joint && mesh.object<512+suffix.first_joint+*suffix.joint_count)
                                    expected.push_back(&mesh);
                            // Retail skin geometry can be owned entirely by
                            // joint zero; a matrix suffix need not own a mesh.
                            if(drawn.meshes.size()!=expected.size())
                                throw std::runtime_error("Retail frozen model drawing range mismatch");
                            for(size_t mesh=0;mesh<expected.size();++mesh) {
                                if(drawn.meshes[mesh].object!=expected[mesh]->object || drawn.meshes[mesh].triangles.size()!=expected[mesh]->triangles.size())
                                    throw std::runtime_error("Retail suffix mesh ownership mismatch");
                                for(size_t triangle=0;triangle<expected[mesh]->triangles.size();++triangle)
                                    for(size_t corner=0;corner<3;++corner) {
                                        const auto a=drawn.meshes[mesh].triangles[triangle].corners[corner].position;
                                        const auto b=expected[mesh]->triangles[triangle].corners[corner].position;
                                        if(std::abs(a.x-b.x)>.0001 || std::abs(a.y-b.y)>.0001 || std::abs(a.z-b.z)>.0001)
                                            throw std::runtime_error("Retail frozen suffix drawing changed geometry");
                                    }
                            }
                            ++zombie_range_draws;
                        }
                    }
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
                            auto examined=runtime;
                            const auto& trigger=examined.scenario.interactions.at(0);
                            examined.world.player.yaw=0;
                            examined.world.player.position={trigger.position.x+trigger.size.x*.5f,
                                trigger.position.y,trigger.position.z+trigger.size.z*.5f+
                                (trigger.type==0?4.5f:6.f)};
                            examined.world.front_anchor=examined.world.player.position;
                            examined.world.front_offset=0;
                            examined.world.floor=trigger.floor;
                            if(!examine_room_interaction(examined) ||
                               examined.scenario.interaction_index!=0 ||
                               !(examined.scenario.flags[10][0]&0x100))
                                throw std::runtime_error("Retail corridor examine selection mismatch");
                            run_scenario(scenario.program(1),examined.scenario);
                            if(!examined.scenario.door || examined.scenario.door->stage!=0 ||
                               examined.scenario.door->room!=2 || examined.scenario.door->position!=0)
                                throw std::runtime_error("Retail examined door request mismatch");
                            if(transition_check) {
                                if(examined.scenario.room_case>1)
                                    throw std::runtime_error("Unexpected graveyard case");
                                const auto next=entries.at(room_archive_index(
                                    examined.scenario.door->stage,examined.scenario.door->room,
                                    examined.scenario.room_case));
                                auto destination=expand(read(file,next.offset,next.size));
                                for(const auto& enemy:decode_room_records(destination).enemies)
                                    if(enemy.flags&1)
                                        std::cout<<"Destination authored enemy: id="<<enemy.id
                                            <<" type="<<enemy.type<<" model="<<int(enemy.model_version)
                                            <<" work="<<int(enemy.work_number)<<" flags="<<enemy.flags<<"\n";
                                transition_room_runtime(examined,destination,player);
                                if(examined.scenario.room!=2 || examined.scenario.door)
                                    throw std::runtime_error("Retail destination entry mismatch");
                            }
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
            std::cout<<"Verified "<<zombie_linked_work<<" linked zombie work bindings against graveyard assets\n";
            std::cout<<"Verified "<<zombie_parameters<<" graveyard zombie health/type/walking parameters and motion pairs\n";
            if(zombie_joint_samples!=zombie_parameters*3 || zombie_linked_poses!=zombie_linked_work*3)
                throw std::runtime_error("Missing retail zombie linked pose coverage");
            std::cout<<"Verified "<<zombie_joint_samples<<" graveyard skeleton samples and "<<zombie_linked_poses<<" whole-model linked placements\n";
            if(zombie_channel_applications!=zombie_parameters*48)
                throw std::runtime_error("Missing retail zombie channel application coverage");
            std::cout<<"Applied "<<zombie_channel_applications<<" full/body graveyard motion samples with original mirroring, joint offset and blending\n";
            if(zombie_startups!=zombie_parameters*2 || zombie_startup_links!=zombie_parameters*6+2 || zombie_managers!=2)
                throw std::runtime_error("Missing retail zombie main Init coverage");
            std::cout<<"Initialized "<<zombie_startups<<" native root/body zombie works with "<<zombie_startup_links<<" linked allocations and "<<zombie_managers<<" hidden managers\n";
            if(zombie_auxiliary_startups!=zombie_parameters*5+2)
                throw std::runtime_error("Missing retail zombie auxiliary Init coverage");
            std::cout<<"Initialized "<<zombie_auxiliary_startups<<" native head/arm/leg/manager works in pool order\n";
            if(zombie_model_work_samples!=zombie_channel_applications || zombie_range_draws!=zombie_parameters*2)
                throw std::runtime_error("Missing retail zombie model-work coverage");
            std::cout<<"Verified "<<zombie_model_work_samples<<" linear model-work samples and "<<zombie_range_draws<<" frozen prefix/suffix drawing ranges\n";
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
