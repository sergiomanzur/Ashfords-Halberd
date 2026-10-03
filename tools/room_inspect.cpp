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
#include "face_deformation.hpp"
#include "face_lipsync.hpp"
#include "runtime.hpp"
#include "zombie.hpp"
#include "effects.hpp"
#include "enemy_targeting.hpp"
#include "enemy_navigation.hpp"
#include "enemy_movement.hpp"
#include "enemy_wall.hpp"
#include "zombie_motion_tables.hpp"
#include "ninja_matrix.hpp"
#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ashford;
// Independent child/sibling traversal checks the production linear work-cache
// traversal using the same source float arithmetic, rather than render math.
std::vector<ModelMatrix> source_hierarchy_matrices(
    const ActorModel& actor, const std::vector<JointPose>& pose) {
    const auto bind=actor_bind_pose(actor);
    if(pose.size()!=bind.size())
        throw std::runtime_error("Retail hierarchy probe requires a complete pose");
    const auto& bytes=actor.model;
    auto u16=[&](size_t at) {
        return uint16_t(bytes.at(at)) | uint16_t(bytes.at(at+1))<<8;
    };
    auto u32=[&](size_t at) {
        return uint32_t(u16(at)) | uint32_t(u16(at+2))<<16;
    };
    const size_t base=u16(4),table=base+size_t(u32(12));
    auto index=[&](uint32_t offset) {
        const size_t at=base+size_t(offset);
        if(at<table || (at-table)%52 || (at-table)/52>=bind.size())
            throw std::runtime_error("Invalid retail hierarchy joint link");
        return (at-table)/52;
    };
    std::vector<ModelMatrix> result(bind.size());
    std::vector<bool> visited(bind.size());
    std::function<void(size_t,detail::NinjaMatrix,size_t)> visit=
        [&](size_t joint,detail::NinjaMatrix matrix,size_t depth) {
            if(depth>256 || visited.at(joint))
                throw std::runtime_error("Cyclic retail hierarchy probe");
            visited[joint]=true;
            const auto parent=matrix;
            detail::ninja_translate(matrix,pose[joint].position.value_or(*bind[joint].position));
            detail::ninja_rotate(matrix,pose[joint].angles.value_or(std::array<int32_t,3>{}));
            result[joint]=detail::ninja_store(matrix);
            const auto child=u32(table+joint*52+44),sibling=u32(table+joint*52+48);
            if(child!=0xffffffffu)visit(index(child),matrix,depth+1);
            if(sibling!=0xffffffffu)visit(index(sibling),parent,depth+1);
        };
    if(!bind.empty())visit(0,detail::ninja_identity(),0);
    if(std::find(visited.begin(),visited.end(),false)!=visited.end())
        throw std::runtime_error("Unreachable retail hierarchy joint");
    return result;
}
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
        size_t zombie_player_contacts=0;
        size_t zombie_auxiliary_frames=0, zombie_manager_frames=0;
        size_t enemy_floor_probes=0, zombie_view_probes=0, zombie_neck_updates=0;
        size_t route_regions=0, route_queries=0, invalid_route_hops=0;
        size_t zombie_awareness_ticks=0;
        size_t enemy_stair_probes=0, zombie_brain_roots=0, zombie_brain_ticks=0;
        size_t zombie_idle_ticks=0, zombie_idle_transitions=0, zombie_idle_sound_requests=0;
        size_t zombie_search_neck_ticks=0;
        size_t zombie_walk_ticks=0, zombie_root_displacements=0, zombie_speed_samples=0;
        size_t zombie_motion_table_ticks=0, zombie_foot_zero_probes=0;
        size_t zombie_set_motion_ticks=0,zombie_native_effect_checks=0;
        size_t zombie_model_ticks=0,zombie_cache_draws=0,zombie_cache_changed_draws=0;
        size_t zombie_mouth_draws=0,zombie_mouth_changed_draws=0;
        size_t zombie_blood_birth_probes=0,zombie_water_birth_probes=0;
        size_t zombie_head_birth_probes=0,zombie_head_birth_works=0;
        size_t zombie_close_pursuit_ticks=0,zombie_walk_phase_boundaries=0;
        size_t zombie_lunge_ticks=0,zombie_capture_probes=0;
        size_t player_grab_ticks=0,player_grab_samples=0,player_grab_releases=0,player_grab_deaths=0;
        size_t zombie_grab_ticks=0,zombie_grab_bites=0,zombie_grab_escapes=0,zombie_grab_fatals=0;
        size_t zombie_grab_player_samples=0,zombie_grab_enemy_samples=0;
        size_t standing_grab_ticks=0,standing_grab_samples=0,standing_grab_releases=0;
        size_t failed_grab_ticks=0,failed_grab_samples=0,failed_grab_recoveries=0,failed_grab_falls=0;
        size_t zombie_motion_sound_requests=0, zombie_motion_vibration_requests=0;
        size_t lip_rooms=0, lip_clips=0, lip_segments=0;
        size_t facial_actor_startups=0, facial_actor_ticks=0, facial_actor_changed_frames=0;
        size_t facial_lip_ticks=0, facial_lip_changed_frames=0;
        for(size_t i=0;i<entries.size();++i) {
            try {
                auto data=expand(read(file,entries[i].offset,entries[i].size));
                auto r=room_info(data);
                const auto resident_lips=decode_room_lip_clips(data);
                if(!resident_lips.empty())++lip_rooms;
                for(const auto& clip:resident_lips) {
                    ++lip_clips;
                    if(!clip.empty()) {
                        const auto playback=start_lip_playback(clip,255);
                        lip_segments+=playback.segments.size();
                    }
                }
                auto collision=decode_room_collision(data);
                auto records=decode_room_records(data);
                const auto routes=decode_room_routes(data);
                route_regions+=routes.regions.size();
                if(scenario_check) for(const auto& from:routes.regions) for(const auto& to:routes.regions) {
                    auto center=[](const CollisionRecord& r) {
                        return Vec3{r.position.x+.5f*r.size.x,r.position.y+.5f*r.size.y,r.position.z+.5f*r.size.z};
                    };
                    const auto a=center(from),b=center(to);
                    const auto start=room_route_id(routes,a),finish=room_route_id(routes,b);
                    if(start==255 || finish==255)continue;
                    if(start!=finish && routes.next_hops[size_t(finish)*routes.regions.size()+start]>=routes.regions.size()) {
                        ++invalid_route_hops;continue; // Source would address outside its region array.
                    }
                    Vec3 target{9999,9999,9999};
                    const auto hop=room_route(routes,a,b,target);
                    const auto expected=hop==255?b:center(routes.regions.at(hop));
                    if(target.x!=expected.x || target.y!=expected.y || target.z!=expected.z)
                        throw std::runtime_error("Retail native route target did not match source region center/destination");
                    ++route_queries;
                }
                if(scenario_check) for(const auto& floor:collision.floors) {
                    if(!(floor.flags&1) || floor.type!=2 || floor.size.x<0 || floor.size.z<0)continue;
                    const Vec3 center{floor.position.x+floor.size.x*.5f,0,floor.position.z+floor.size.z*.5f};
                    const auto* hit=enemy_floor_trigger(collision,{},center,floor.floor,floor.parameters[0],floor.parameters[1]);
                    if(!hit || hit->floor!=floor.floor || hit->parameters[0]!=floor.parameters[0] ||
                       hit->parameters[1]!=floor.parameters[1])
                        throw std::runtime_error("Retail enemy floor trigger did not match its authored center/identity");
                    ++enemy_floor_probes;
                }
                auto scenario=decode_room_scenario(data);
                if(scenario_check) for(const auto& stair:collision.miscellaneous) {
                    if(!(stair.flags&1) || stair.type!=1 || (stair.attribute&0x400000) ||
                       stair.size.x<1 || stair.size.z<1)continue;
                    const Vec3 center{stair.position.x+.5f*stair.size.x,0,stair.position.z+.5f*stair.size.z};
                    const auto hit=enemy_stair_trigger(collision,{},center,0,stair.floor,0);
                    if(!hit || *hit>=collision.miscellaneous.size())
                        throw std::runtime_error("Retail authored stair center has no native query result");
                    const auto& selected=collision.miscellaneous[*hit];
                    if(!(selected.flags&1) || selected.type!=1 || (selected.attribute&0x400000) ||
                       selected.floor!=stair.floor)
                        throw std::runtime_error("Retail stair query selected ineligible source record");
                    ++enemy_stair_probes;
                }
                scenario_entries+=scenario.entry_count();
                if(scenario_check) {
                    ScenarioState bound_enemies;
                    bind_room_enemies(data,collision,bound_enemies);
                    bound_enemies.routes=std::make_shared<const RoomRoutes>(routes);
                    bind_room_props(data,bound_enemies);
                    // Entry 0 has an ID91 facial work and a resident lip bank.
                    // The graveyard entry below intentionally has no lip bank.
                    if(i==0) {
                        size_t authored=0;
                        while(authored<records.enemies.size() &&
                              (!(records.enemies[authored].flags&1) || records.enemies[authored].id!=91))++authored;
                        if(authored>=records.enemies.size() || authored>255 || resident_lips.empty())
                            throw std::runtime_error("Missing selected retail facial lip fixture");
                        const auto slot=bound_enemies.enemy_slots.at(authored);
                        auto native=bound_enemies;
                        for(size_t work=0;work<native.enemies.size();++work)
                            if(work!=slot)native.enemies[work].flags&=~1u;
                        native.flags[6][0]=2;
                        tick_scenario_actors(native);
                        auto& actor=native.enemies.at(slot);
                        if(!actor.face || !actor.rendered_model || !actor.face->controller.selected_clip)
                            throw std::runtime_error("Missing selected retail facial lip binding");
                        const auto held_frame=actor.frame;
                        const auto raw=actor.model->model;
                        const auto original=*actor.rendered_model;
                        run_scenario(Bytes{61,0,uint8_t(authored),0,63,0,uint8_t(authored),0,0,0},native);
                        do {
                            if(++facial_lip_ticks>100000)throw std::runtime_error("Native facial lip clip did not terminate");
                            tick_scenario_actors(native);
                            if(actor.frame!=held_frame || !actor.rendered_model ||
                               actor.rendered_model->meshes.size()!=original.meshes.size() || actor.model->model!=raw)
                                throw std::runtime_error("Native lip playback changed raw asset, body frame or mesh count");
                            bool changed=false;
                            for(size_t mesh=0;mesh<original.meshes.size();++mesh) {
                                const auto& before=original.meshes[mesh].triangles;
                                const auto& after=actor.rendered_model->meshes[mesh].triangles;
                                if(before.size()!=after.size())throw std::runtime_error("Native lip playback changed triangle count");
                                for(size_t triangle=0;triangle<before.size();++triangle)
                                    for(size_t corner=0;corner<3;++corner) {
                                        const auto a=before[triangle].corners[corner].position;
                                        const auto b=after[triangle].corners[corner].position;
                                        if(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>.0001f)changed=true;
                                    }
                            }
                            if(changed)++facial_lip_changed_frames;
                        } while(actor.face->controller.flags&4);
                        if(facial_lip_ticks<2 || !facial_lip_changed_frames)
                            throw std::runtime_error("Native retail lip playback did not animate geometry");
                    }
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
                                const auto facial_scene=pose_actor_rotated(facial.actor,expanded,actor.position,actor.angles);
                                if(raw_skin.size()!=27 || expanded.size()!=31 || facial_scene.meshes.empty())
                                    throw std::runtime_error("Graveyard facial preparation differs from selected model");
                                std::cout<<"Prepared subplayer joints="<<expanded.size()
                                         <<" meshes="<<facial_scene.meshes.size()
                                         <<" tooth="<<facial.tooth.parent<<" tongue="<<facial.tongue.parent
                                         <<" eyes="<<(facial.eyes[0]?std::to_string(facial.eyes[0]->parent):"absent")
                                         <<","<<(facial.eyes[1]?std::to_string(facial.eyes[1]->parent):"absent")<<'\n';
                                const auto track=decode_face_track(actor.model_bank->mask);
                                const auto deformation=decode_face_deformation(actor.model_bank->mask,facial);
                                if(track.empty())throw std::runtime_error("Selected facial model has no shape keys");
                                const auto first=sample_face_track(track,0);
                                const auto last=sample_face_track(track,float(track.back().frame));
                                (void)first;(void)last;
                                size_t sampled_shapes=0;
                                size_t deformed_geometry_frames=0;
                                auto animated=facial;
                                for(float frame=0;frame<=track.back().frame;frame+=0.5f) {
                                    const auto parameters=sample_face_track(track,frame);
                                    try {
                                        deform_face(animated,deformation,parameters);
                                        const auto pose=actor_bind_pose(animated.actor);
                                        ActorPlacement placement;
                                        placement.position=actor.position;placement.angles=actor.angles;
                                        auto work=calculate_actor_model_work(animated.actor,pose,animated.work,placement);
                                        const auto meshes=pose_actor_model_work(animated.actor,pose,work);
                                        if(meshes.meshes.size()!=facial_scene.meshes.size())
                                            throw std::runtime_error("Facial animation changed visible owner count");
                                        bool moved=false;
                                        for(size_t mesh=0;mesh<meshes.meshes.size();++mesh) {
                                            const auto& current=meshes.meshes[mesh].triangles;
                                            const auto& original=facial_scene.meshes[mesh].triangles;
                                            if(current.size()!=original.size())throw std::runtime_error("Facial triangle count changed");
                                            for(size_t triangle=0;triangle<current.size();++triangle)
                                                for(size_t corner=0;corner<3;++corner) {
                                                    const auto a=current[triangle].corners[corner].position;
                                                    const auto b=original[triangle].corners[corner].position;
                                                    if(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>0.0001f)moved=true;
                                                }
                                        }
                                        if(moved)++deformed_geometry_frames;
                                    } catch(const std::exception& error) {
                                        throw std::runtime_error("Facial deformation frame "+std::to_string(frame)+
                                            " eye="+std::to_string(parameters.eye.x)+","+std::to_string(parameters.eye.y)+","+
                                            std::to_string(parameters.eye.z)+": "+error.what());
                                    }
                                    ++sampled_shapes;
                                }
                                if(!deformed_geometry_frames)throw std::runtime_error("Facial parameters produced no geometric change");
                                for(const auto& key:track) {
                                    const auto exact=sample_face_track(track,float(key.frame));
                                    if(exact.muscle!=key.parameters.muscle)
                                        throw std::runtime_error("Facial sampling changed an authored muscle key");
                                }
                                std::cout<<"Selected facial shape keys="<<track.size()
                                         <<" last_frame="<<track.back().frame
                                         <<" samples="<<sampled_shapes
                                         <<" deformed_geometry_frames="<<deformed_geometry_frames<<'\n';
                                if(!actor.face || !actor.model_work || actor.joint_pose.size()!=31)
                                    throw std::runtime_error("Finish-room did not bind selected facial runtime");
                                // Run the production scheduler on the selected ID91 work.
                                // The graveyard's ID1 zombie controller remains separate.
                                auto native=bound_enemies;
                                for(auto& work:native.enemies)if(work.id!=91)work.flags&=~1u;
                                native.flags[6][0]=2;
                                tick_scenario_actors(native);
                                auto& controller=native.enemies.at(slot);
                                if(controller.modes[0]!=1 || controller.modes[3]!=3 ||
                                   controller.frame!=0 || !controller.motions ||
                                   controller.motions->at(0).has_value() || !controller.rendered_model ||
                                   controller.rendered_model->meshes.size()!=6)
                                    throw std::runtime_error("Selected facial subplayer startup differs from source: mode="+
                                        std::to_string(controller.modes[0])+","+std::to_string(controller.modes[3])+
                                        " frame="+std::to_string(controller.frame)+" meshes="+
                                        (controller.rendered_model?std::to_string(controller.rendered_model->meshes.size()):"absent")+
                                        " motion0="+(controller.motions && !controller.motions->empty() && (*controller.motions)[0]?"bound":"null"));
                                ++facial_actor_startups;
                                const auto held_frame=controller.frame;
                                const auto native_first=*controller.rendered_model;
                                const auto native_raw=controller.model->model;
                                controller.face->controller.flags|=0x20;
                                tick_scenario_actors(native); // Original track reset after this sample.
                                controller.face->controller.flags|=3;
                                for(size_t step=0;step<96;++step) {
                                    tick_scenario_actors(native);
                                    if(controller.frame!=held_frame ||
                                       controller.face->controller.frame!=int32_t((step+1)%46) ||
                                       !controller.rendered_model || controller.rendered_model->meshes.size()!=6)
                                        throw std::runtime_error("Native facial actor loop/hold/render mismatch");
                                    ++facial_actor_ticks;
                                    bool changed=false;
                                    for(size_t mesh=0;mesh<native_first.meshes.size();++mesh) {
                                        const auto &before=native_first.meshes[mesh].triangles;
                                        const auto &after=controller.rendered_model->meshes[mesh].triangles;
                                        if(before.size()!=after.size())throw std::runtime_error("Facial actor changed triangle count");
                                        for(size_t triangle=0;triangle<before.size();++triangle)
                                            for(size_t corner=0;corner<3;++corner) {
                                                const auto a=before[triangle].corners[corner].position;
                                                const auto b=after[triangle].corners[corner].position;
                                                if(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>0.0001f)changed=true;
                                            }
                                    }
                                    if(changed)++facial_actor_changed_frames;
                                }
                                if(!facial_actor_changed_frames || controller.model->model!=native_raw)
                                    throw std::runtime_error("Facial actor rendered static geometry or mutated raw asset");
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
                                                    auto expected_matrices=source_hierarchy_matrices(*zombie.model,applied.pose);
                                                    auto native_work=calculate_actor_model_work(*zombie.model,applied.pose);
                                                    if(applied.pose.size()!=bind.size() || expected_matrices.size()!=bind.size() || native_work.joints.size()!=bind.size())
                                                        throw std::runtime_error("Retail zombie motion application changed joint count");
                                                    for(size_t joint=0;joint<bind.size();++joint)
                                                        for(size_t element=0;element<16;++element)
                                                            if(native_work.joints[joint].matrix[element]!=expected_matrices[joint][element])
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
                        if(runtime_check) {
                            // Isolated native targeting dependencies against actual
                            // cached body/Claire joints, not full root AI frames.
                            auto targeting=initialized;
                            const auto player_entry=enter_room(records,0,collision,decode_room_cuts(data));
                            ActorPlacement placement;
                            placement.flags=0x118;placement.position=player_entry.player.position;
                            placement.angles={0,int32_t(player_entry.player.yaw),0};
                            const auto claire=calculate_actor_model_work(player.models.at(0),actor_bind_pose(player.models.at(0)),{},placement);
                            for(auto root:roots) {
                                auto& owner=targeting.enemies[root];
                                const auto cached=*owner.model_work;
                                const auto raw=owner.model->model;
                                const auto body=owner.zombie_work->body.value();
                                const auto cone=zombie_personality(owner.model_version).view_angle;
                                const auto& matrix=cached.joints.at(11).matrix;
                                const Vec3 probe{float(matrix[3]),0,float(matrix[11])};
                                const auto yaw=owner.angles[1]+int32_t(owner.joint_pose.at(11).angles->at(1));
                                const auto result=search_enemy_player(probe,yaw,player_entry.player.position,cone);
                                if(result!=-1 && (result<-32768 || result>32768))
                                    throw std::runtime_error("Retail view cone returned an invalid signed yaw");
                                ++zombie_view_probes;
                                // Isolated Brain00 close pursuit, using actual initialized
                                // root/cache/route work. Player flag4 suppresses stairs;
                                // this is not Move/attack/damage or a full encounter tick.
                                if(owner.type!=2 && owner.type!=3 && owner.type!=5 && owner.type!=6 &&
                                   owner.type!=7 && owner.type!=8 && owner.type!=9) {
                                    auto pursuit=initialized;
                                    auto& actor=pursuit.enemies[root];
                                    auto& extended=*actor.zombie_work;
                                    ScenarioEntity target;
                                    target.position=actor.position;
                                    target.position.x+=2;target.floor=actor.floor;target.flags=4;
                                    extended.player_distance=2;actor.zombie_flags|=0x400;
                                    const auto region=room_route_id(*pursuit.routes,actor.position);
                                    const auto before=actor.modes;
                                    const auto seed=pursuit.random_seed;
                                    const auto original_frame=actor.frame;
                                    for(size_t tick=0;tick<93;++tick) {
                                        tick_zombie_brain(pursuit,root,target);
                                        if(extended.target_x!=target.position.x || extended.target_z!=target.position.z ||
                                           extended.route_id!=region || extended.route_timer!=0 || actor.modes!=before ||
                                           actor.frame!=original_frame || pursuit.random_seed!=seed ||
                                           actor.model->model!=raw || actor.stair_index)
                                            throw std::runtime_error("Retail native Brain00 close pursuit changed unrelated work or route policy");
                                        ++zombie_brain_ticks;
                                    }
                                    ++zombie_brain_roots;
                                    auto idle=initialized;
                                    idle.enemies[root].modes[2]=0;idle.enemies[root].modes[3]=0;
                                    // Exercise actual MV00 then the existing raw channel
                                    // sampler once; full SetMtn effects/fix offsets are pending.
                                    tick_zombie_idle(idle,root);++zombie_idle_ticks;
                                    if(idle.enemies[root].motion_number!=2 || idle.enemies[body].motion_number!=202 ||
                                       idle.enemies[root].frame!=idle.enemies[body].frame)
                                        throw std::runtime_error("Retail MV00 did not select original root/body idle motions");
                                    advance_zombie_motion(idle,root);advance_zombie_motion(idle,body);
                                    bool transitioned=false;
                                    for(size_t tick=0;tick<220;++tick) {
                                        tick_zombie_idle(idle,root);++zombie_idle_ticks;
                                        if(idle.enemies[root].modes[2]==1 && idle.enemies[root].modes[3]==0) {
                                            transitioned=true;++zombie_idle_transitions;break;
                                        }
                                    }
                                    if(!transitioned)throw std::runtime_error("Retail idle timer did not enter original walking state");
                                    for(const auto& sound:idle.pending_enemy_sounds) {
                                        auto seed=idle.random_seed;
                                        const auto expected=choose_zombie_sound(idle.enemies[root],seed,16847639,false);
                                        if(!expected || sound.code!=*expected || sound.work!=root || seed!=idle.random_seed)
                                            throw std::runtime_error("Retail idle sound request lost source slot/code/RNG policy");
                                        ++zombie_idle_sound_requests;
                                    }
                                    auto walking=initialized;
                                    walking.enemies[root].modes[2]=1;walking.enemies[root].modes[3]=0;
                                    walking.enemies[root].zombie_work->player_distance=100;
                                    ScenarioEntity walk_target;
                                    walk_target.position=player_entry.player.position;
                                    const auto original_position=walking.enemies[root].position;
                                    const auto expected_motion=zombie_walk_motion(*walking.enemies[root].zombie_walk_choice,false);
                                    const auto expected_body_motion=zombie_walk_motion(*walking.enemies[root].zombie_walk_choice,true);
                                    // Isolated MV01 policy at retained frame0/distance100,
                                    // with actual room walls. Full Move/SetMtn is pending.
                                    for(size_t tick=0;tick<120;++tick) {
                                        tick_zombie_walk(walking,root,walk_target);++zombie_walk_ticks;
                                        const auto& moved=walking.enemies[root];
                                        if(moved.motion_number!=expected_motion || walking.enemies[body].motion_number!=expected_body_motion ||
                                           moved.position.x!=original_position.x || moved.position.y!=original_position.y ||
                                           moved.position.z!=original_position.z || moved.model->model!=raw || moved.modes[2]!=1)
                                            throw std::runtime_error("Retail isolated MV01 changed assets/position or lost walking selection");
                                    }
                                    for(auto slot:{root,body}) {
                                        const auto& actor=walking.enemies[slot];
                                        const auto& motion=*actor.motions->at(actor.motion_number);
                                        ScenarioEntity probe;
                                        probe.motions=actor.motions;probe.motion_number=actor.motion_number;
                                        for(uint32_t frame=0;frame<motion.frames;++frame) {
                                            probe.position=original_position;
                                            measure_enemy_motion_speed(probe,int32_t(frame),0);++zombie_speed_samples;
                                            translate_enemy_motion(probe,int32_t(frame),0);++zombie_root_displacements;
                                            if(!std::isfinite(probe.speed) || probe.speed<0 ||
                                               !std::isfinite(probe.position.x) || !std::isfinite(probe.position.y) ||
                                               !std::isfinite(probe.position.z))
                                                throw std::runtime_error("Retail original walking root key produced invalid displacement/speed");
                                        }
                                    }
                                    // Real source effect routing in SetMtn -> CalcEnemy work order.
                                    const ZombieMotionEffects native_effects{
                                        [&](ScenarioState& state,size_t slot,int32_t frame) {
                                            tick_zombie_kami_effect(state,slot,frame);
                                            ++zombie_native_effect_checks;
                                        },
                                        [&](ScenarioState& state,size_t slot,int32_t frame) {
                                            tick_zombie_water_effect(state,slot,frame);
                                            ++zombie_native_effect_checks;
                                        }};
                                    auto planted=walking;
                                    std::optional<Scene> first_cache_draw;
                                    for(size_t tick=0;tick<80;++tick) {
                                        const auto before_sounds=planted.pending_enemy_sounds.size();
                                        tick_zombie_motion(planted,root,native_effects);++zombie_set_motion_ticks;++zombie_motion_table_ticks;
                                        calculate_zombie_model(planted,root);++zombie_model_ticks;
                                        tick_zombie_motion(planted,body,native_effects);++zombie_set_motion_ticks;++zombie_motion_table_ticks;
                                        calculate_zombie_model(planted,body);++zombie_model_ticks;
                                        zombie_motion_sound_requests+=planted.pending_enemy_sounds.size()-before_sounds;
                                        auto& current=planted.enemies[root];
                                        const auto& upper=planted.enemies[body];
                                        if(current.geometry.capsule_radius!=2 ||
                                           !std::isfinite(current.geometry.capsule_start.y) ||
                                           !std::isfinite(current.geometry.capsule_end.y) ||
                                           upper.geometry.height!=float(current.model_work->joints[10].matrix[7])-upper.position.y ||
                                           upper.geometry.collision_height!=upper.geometry.height ||
                                           current.model->model!=raw)
                                            throw std::runtime_error("Retail source CalcEnemy capsule/height or immutable model changed");
                                        ActorPlacement cached_placement;
                                        cached_placement.flags=current.flags;cached_placement.position=current.position;
                                        cached_placement.angles=current.angles;
                                        if(tick==0 || tick==40 || tick==79) {
                                            const auto retained_pose=current.joint_pose;
                                            const auto retained_cache=*current.model_work;
                                            auto drawn=draw_zombie_model(planted,root);
                                            if(!current.morph_models)
                                                throw std::runtime_error("Retail zombie missing immutable prepared mouth models");
                                            const auto baseline=pose_actor_model_work(*current.model,current.joint_pose,*current.model_work);
                                            size_t mouth_changes=0;
                                            if(drawn.meshes.size()!=baseline.meshes.size())
                                                throw std::runtime_error("Retail mouth morph changed mesh topology");
                                            for(size_t mesh=0;mesh<drawn.meshes.size();++mesh) {
                                                if(drawn.meshes[mesh].triangles.size()!=baseline.meshes[mesh].triangles.size())
                                                    throw std::runtime_error("Retail mouth morph changed triangle topology");
                                                for(size_t triangle=0;triangle<drawn.meshes[mesh].triangles.size();++triangle)
                                                    for(size_t corner=0;corner<3;++corner) {
                                                        const auto a=drawn.meshes[mesh].triangles[triangle].corners[corner].position;
                                                        const auto b=baseline.meshes[mesh].triangles[triangle].corners[corner].position;
                                                        mouth_changes+=a.x!=b.x || a.y!=b.y || a.z!=b.z;
                                                    }
                                            }
                                            if(current.model_flags&2) {
                                                ++zombie_mouth_draws;
                                                if(mouth_changes)++zombie_mouth_changed_draws;
                                                if(current.shape_count!=0 && !mouth_changes)
                                                    throw std::runtime_error("Retail active mouth shape did not change original geometry");
                                            }
                                            if(current.model->model!=raw || current.model_work->base_matrix!=retained_cache.base_matrix)
                                                throw std::runtime_error("Retail mouth drawing changed raw model/base cache");
                                            for(size_t joint=0;joint<retained_cache.joints.size();++joint) {
                                                if(current.model_work->joints[joint].matrix!=retained_cache.joints[joint].matrix ||
                                                   current.model_work->joints[joint].flags!=retained_cache.joints[joint].flags ||
                                                   current.joint_pose[joint].angles!=retained_pose[joint].angles)
                                                    throw std::runtime_error("Retail mouth drawing changed retained pose/cache");
                                                if(retained_pose[joint].position) {
                                                    const auto a=current.joint_pose[joint].position.value(),b=retained_pose[joint].position.value();
                                                    if(a.x!=b.x || a.y!=b.y || a.z!=b.z)
                                                        throw std::runtime_error("Retail mouth drawing changed retained joint position");
                                                } else if(current.joint_pose[joint].position)
                                                    throw std::runtime_error("Retail mouth drawing created retained joint position");
                                            }
                                            size_t corners=0,changed=0;
                                            for(size_t mesh=0;mesh<drawn.meshes.size();++mesh) {
                                                if(first_cache_draw && (mesh>=first_cache_draw->meshes.size() ||
                                                   drawn.meshes[mesh].triangles.size()!=first_cache_draw->meshes[mesh].triangles.size()))
                                                    throw std::runtime_error("Retail cached drawing changed topology");
                                                for(size_t triangle=0;triangle<drawn.meshes[mesh].triangles.size();++triangle)
                                                    for(size_t corner=0;corner<3;++corner) {
                                                        const auto p=drawn.meshes[mesh].triangles[triangle].corners[corner].position;
                                                        if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                                                            throw std::runtime_error("Retail cached drawing has invalid geometry");
                                                        ++corners;
                                                        if(first_cache_draw) {
                                                            const auto q=first_cache_draw->meshes[mesh].triangles[triangle].corners[corner].position;
                                                            changed+=p.x!=q.x || p.y!=q.y || p.z!=q.z;
                                                        }
                                                    }
                                            }
                                            if(!corners || (first_cache_draw && drawn.meshes.size()!=first_cache_draw->meshes.size()))
                                                throw std::runtime_error("Retail cached drawing has missing geometry");
                                            if(changed)++zombie_cache_changed_draws;
                                            if(!first_cache_draw)first_cache_draw=std::move(drawn);
                                            ++zombie_cache_draws;
                                        }
                                        for(size_t path:{0,1,2,3,6,7}) {
                                            const auto delta=calculate_actor_fix_offset(current.joint_pose,*current.model_work,
                                                cached_placement,detail::zombie_fix_paths[path],{0,-1,-1.9f});
                                            if(delta.x!=0 || delta.y!=0 || delta.z!=0)
                                                throw std::runtime_error("Retail unchanged current pose/cache has foot-lock drift");
                                            ++zombie_foot_zero_probes;
                                        }
                                    }
                                    auto close=initialized;
                                    auto& tracker=close.enemies[root];
                                    tracker.modes={1,1,2,0};tracker.zombie_flags|=0x400;
                                    tracker.zombie_work->player_distance=10;
                                    ScenarioEntity near_player;
                                    near_player.flags=0x118;near_player.floor=tracker.floor;
                                    const auto close_yaw=ninja_sin_cos(uint16_t(tracker.angles[1]));
                                    near_player.position={tracker.position.x-close_yaw[0]*10,
                                        tracker.position.y,tracker.position.z-close_yaw[1]*10};
                                    tracker.zombie_work->target_x=near_player.position.x;
                                    tracker.zombie_work->target_z=near_player.position.z;
                                    for(size_t tick=0;tick<18;++tick) {
                                        tick_zombie_close_pursuit(close,root,near_player);
                                        ++zombie_close_pursuit_ticks;
                                    }
                                    if(tracker.motion_number!=(tracker.type==5?125u:117u) ||
                                       close.enemies[body].motion_number!=(tracker.type==5?325u:317u) ||
                                       tracker.zombie_work->pursuit_transition_timer!=0 ||
                                       tracker.zombie_work->pursuit_phase_word!=1 || tracker.model->model!=raw)
                                        throw std::runtime_error("Retail source close-pursuit/paired transition mismatch");
                                    for(size_t joint=0;joint<cached.joints.size();++joint)
                                        if(tracker.model_work->joints[joint].matrix!=cached.joints[joint].matrix)
                                            throw std::runtime_error("Retail pursuit scalar state changed cached model");
                                    const auto old_frames=tracker.motions->at(tracker.motion_number)->frames;
                                    tracker.frame=int32_t((old_frames-1)*65536u);
                                    change_zombie_walk_motion(close,root);
                                    const auto new_frames=tracker.motions->at(tracker.motion_number)->frames;
                                    if(uint32_t(tracker.frame)!=new_frames*65536u ||
                                       close.enemies[body].motion_number!=119)
                                        throw std::runtime_error("Retail source legacy walking phase boundary mismatch");
                                    // Native compatibility at the source remap boundary: compare
                                    // frame0 sampling, retaining literal advancement/boundary flags.
                                    const auto terminal_blend=tracker.blend_count;
                                    const auto terminal_attribute=tracker.motion_attribute;
                                    const auto& terminal_motion=*tracker.motions->at(tracker.motion_number);
                                    auto reference=close;
                                    reference.enemies[root].frame=0;
                                    const ZombieMotionEffects boundary_effects{
                                        [](ScenarioState& state,size_t slot,int32_t frame) {
                                            tick_zombie_kami_effect(state,slot,frame);
                                        },
                                        [](ScenarioState& state,size_t slot,int32_t frame) {
                                            tick_zombie_water_effect(state,slot,frame);
                                        }};
                                    for(auto* sampled : {&close,&reference}) {
                                        tick_zombie_motion(*sampled,root,boundary_effects);
                                        calculate_zombie_model(*sampled,root);
                                        tick_zombie_motion(*sampled,body,boundary_effects);
                                        calculate_zombie_model(*sampled,body);
                                    }
                                    const auto& expected=reference.enemies[root];
                                    if(tracker.frame!=65536 || tracker.motion_boundary!=1 ||
                                       expected.frame!=65536 || expected.motion_boundary!=0 ||
                                       close.enemies[body].frame!=65536 || tracker.model->model!=raw ||
                                       !(tracker.flags&0x2000000) || tracker.blend_count!=terminal_blend-1 ||
                                       tracker.motion_attribute!=(terminal_motion.attributes.empty()?terminal_attribute:
                                           terminal_motion.attributes[0]))
                                        throw std::runtime_error("Retail terminal sampling counter order mismatch");
                                    for(size_t joint=0;joint<tracker.joint_pose.size();++joint) {
                                        const auto& actual=tracker.joint_pose[joint];
                                        const auto& wanted=expected.joint_pose[joint];
                                        if(actual.angles!=wanted.angles ||
                                           actual.position.has_value()!=wanted.position.has_value() ||
                                           (actual.position && (actual.position->x!=wanted.position->x ||
                                            actual.position->y!=wanted.position->y || actual.position->z!=wanted.position->z)))
                                            throw std::runtime_error("Retail terminal paired pose differs from next-loop sampling");
                                        // Source foot fixes retain the original incoming frame, so
                                        // world translations can differ from the frame0 reference.
                                        for(float element : tracker.model_work->joints[joint].matrix)
                                            if(!std::isfinite(element))
                                                throw std::runtime_error("Retail terminal cache contains nonfinite matrix");
                                    }
                                    if(!std::isfinite(tracker.geometry.capsule_start.y) ||
                                       !std::isfinite(tracker.geometry.capsule_end.y) ||
                                       tracker.geometry.capsule_radius!=2)
                                        throw std::runtime_error("Retail terminal collision cache invalid");
                                    ++zombie_walk_phase_boundaries;
                                    // Complete NG01 with actual room walls and original paired clips.
                                    // Root/Nage routing remains separate from this callback probe.
                                    auto failed_grab=initialized;
                                    auto& recoiling=failed_grab.enemies[root];
                                    recoiling.modes={2,0,1,0};
                                    bool saw_recovery=false,failed_grab_finished=false;
                                    for(size_t tick=0;tick<240;++tick) {
                                        tick_zombie_failed_grab(failed_grab,root);++failed_grab_ticks;
                                        if(!tick && (recoiling.motion_number!=7 ||
                                           failed_grab.enemies[body].motion_number!=207 ||
                                           recoiling.frame!=16*65536 ||
                                           failed_grab.enemies[body].frame!=16*65536))
                                            throw std::runtime_error("Retail NG01 paired frame16 startup mismatch");
                                        uint32_t paired=0;
                                        if(recoiling.motion_number==12)paired=212;
                                        else if(recoiling.motion_number==50)paired=250;
                                        else if(recoiling.motion_number==124)paired=324;
                                        if(paired) {
                                            saw_recovery=true;
                                            if(failed_grab.enemies[body].motion_number!=paired)
                                                throw std::runtime_error("Retail NG01 paired recovery motion mismatch");
                                        }
                                        const bool fell=recoiling.modes==std::array<uint8_t,4>{4,0,3,0};
                                        const bool recovered=recoiling.modes==std::array<uint8_t,4>{1,0,4,0};
                                        if(fell || recovered) {
                                            if(!saw_recovery || recoiling.motion_step ||
                                               failed_grab.enemies[body].motion_step ||
                                               failed_grab.enemies[body].modes!=recoiling.modes ||
                                               (fell?recoiling.health!=-1:!(recoiling.zombie_flags&0x40000)))
                                                throw std::runtime_error("Retail NG01 complete recovery/fall cleanup mismatch");
                                            if(fell)++failed_grab_falls;else ++failed_grab_recoveries;
                                            failed_grab_finished=true;break;
                                        }
                                        tick_zombie_motion(failed_grab,root,boundary_effects);
                                        calculate_zombie_model(failed_grab,root);
                                        tick_zombie_motion(failed_grab,body,boundary_effects);
                                        calculate_zombie_model(failed_grab,body);failed_grab_samples+=2;
                                        for(const auto& joint:recoiling.model_work->joints)
                                            for(double value:joint.matrix)
                                                if(!std::isfinite(value))
                                                    throw std::runtime_error("Retail NG01 original paired cache is nonfinite");
                                    }
                                    if(!failed_grab_finished)
                                        throw std::runtime_error("Retail NG01 failed to finish original recovery sequence");
                                    auto lunging=initialized;
                                    auto& lunging_root=lunging.enemies[root];
                                    lunging_root.modes={1,0,5,0};
                                    lunging_root.zombie_flags&=~0x20000000u;
                                    lunging_root.flags|=0x40000;
                                    ScenarioEntity victim;
                                    victim.flags=5;victim.health=100;victim.floor=lunging_root.floor;
                                    victim.position={lunging_root.position.x,lunging_root.position.y,
                                        lunging_root.position.z-4};
                                    ActorPlacement victim_placement;
                                    victim_placement.flags=0x118;victim_placement.position=victim.position;
                                    victim.model_work=calculate_actor_model_work(player.models.at(0),
                                        actor_bind_pose(player.models.at(0)),{},victim_placement);
                                    const auto lunge_frames=lunging_root.motions->at(122)->frames;
                                    bool recovered=false;
                                    for(size_t tick=0;tick<lunge_frames+2;++tick) {
                                        tick_zombie_lunge(lunging,root,victim);++zombie_lunge_ticks;
                                        if(lunging_root.modes==std::array<uint8_t,4>{1,1,1,0}) {
                                            recovered=true;break;
                                        }
                                        tick_zombie_motion(lunging,root,boundary_effects);
                                        calculate_zombie_model(lunging,root);
                                        tick_zombie_motion(lunging,body,boundary_effects);
                                        calculate_zombie_model(lunging,body);
                                    }
                                    if(!recovered || lunging.enemies[body].modes!=lunging_root.modes ||
                                       lunging_root.model->model!=raw)
                                        throw std::runtime_error("Retail complete lunge miss/recovery sequence failed");
                                    // Place original cached Claire joints at the attack origin, using
                                    // actual model hierarchy; no edits to cached retail enemy matrices.
                                    auto grabbing=initialized;
                                    auto& grabber=grabbing.enemies[root];
                                    grabber.modes={1,0,5,0};grabber.zombie_flags&=~0x20000000u;
                                    for(size_t tick=0;tick<8;++tick) {
                                        tick_zombie_lunge(grabbing,root,victim);++zombie_lunge_ticks;
                                        tick_zombie_motion(grabbing,root,boundary_effects);
                                        calculate_zombie_model(grabbing,root);
                                        tick_zombie_motion(grabbing,body,boundary_effects);
                                        calculate_zombie_model(grabbing,body);
                                    }
                                    if(grabber.frame!=8*65536 || grabber.motion_number!=122)
                                        throw std::runtime_error("Retail capture must follow eight native lunge motion/cache frames");
                                    const auto& chest=grabber.model_work->joints.at(10).matrix;
                                    victim.flags=1;victim.modes={};victim.position=grabber.position;
                                    victim.position.z-=4;
                                    victim.angles[1]=32768;victim_placement.position=victim.position;
                                    victim.model_work=calculate_actor_model_work(player.models.at(0),
                                        actor_bind_pose(player.models.at(0)),{},victim_placement);
                                    const auto& target_joint=victim.model_work->joints.at(3).matrix;
                                    victim.position.x+=float(chest[3])-float(target_joint[3]);
                                    victim.position.z+=float(chest[11])-float(target_joint[11])-1;
                                    grabber.angles[1]=enemy_direction(grabber.position,victim.position);
                                    victim_placement.position=victim.position;
                                    victim.model_work=calculate_actor_model_work(player.models.at(0),
                                        actor_bind_pose(player.models.at(0)),{},victim_placement);
                                    if(!zombie_capture_check(grabbing,root,victim,3640,5.5f,0))
                                        throw std::runtime_error("Retail original cached Claire/chest capture gate rejected");
                                    tick_zombie_lunge(grabbing,root,victim);
                                    if(grabber.motion_number!=85 || grabbing.enemies[body].motion_number!=285 ||
                                       grabber.modes!=std::array<uint8_t,4>{2,0,0,2} ||
                                       grabbing.enemies[body].modes!=grabber.modes ||
                                       victim.modes!=std::array<uint8_t,4>{4,0,0,4} || !(victim.flags&0x10004) ||
                                       grabber.control0!=75)
                                        throw std::runtime_error("Retail native lunge player/grab pair state mismatch");
                                    ++zombie_capture_probes;
                                    // Player-side held/lunge-grab dependency on original Claire
                                    // clips. Enemy NG00 damage and PlayerLink remain separate.
                                    for(bool back : {false,true}) {
                                        auto held=grabbing;
                                        auto& attacker=held.enemies[root];
                                        if(back)attacker.zombie_flags|=0x4000;
                                        else attacker.zombie_flags&=~0x4000u;
                                        auto captured=victim;
                                        captured.player_work.emplace();
                                        captured.base_motions=std::make_shared<MotionBank>(player.motions);
                                        captured.motions=captured.base_motions;
                                        captured.joint_pose=actor_bind_pose(player.models.at(0));
                                        held.input_held=0xabcd;
                                        auto sample_player=[&](ScenarioState& context,ScenarioEntity& actor,int account=1) {
                                            const auto& clip=actor.motions->at(actor.motion_number).value();
                                            MotionApplicationOptions options;
                                            options.mode=actor.motion_mode;
                                            for(const auto& joint:actor.model_work->joints)
                                                options.joint_flags.push_back(uint8_t(joint.flags));
                                            if(actor.blend_count)
                                                options.blend_rate=.000015258789f*float(65536-actor.blend_rate);
                                            const auto incoming=actor.frame;
                                            auto result=apply_motion_pose(actor.joint_pose,clip,incoming,
                                                options,context.motion_angle_scratch);
                                            actor.joint_pose=std::move(result.pose);
                                            context.motion_angle_scratch=result.angle_scratch;
                                            actor.motion_attribute=motion_attribute_at(clip,incoming,actor.motion_attribute);
                                            actor.motion_boundary=advance_motion_frame(clip.frames,int32_t(actor.motion_step),
                                                actor.motion_mode,actor.frame,actor.frame_direction);
                                            if(actor.blend_count)--actor.blend_count;
                                            ActorPlacement placement;
                                            if(account==2)placement.flags=actor.flags;
                                            placement.position=actor.position;placement.angles=actor.angles;
                                            actor.model_work=calculate_actor_model_work(player.models.at(0),
                                                actor.joint_pose,*actor.model_work,placement);
                                            for(const auto& joint:actor.model_work->joints)
                                                for(double element:joint.matrix)
                                                    if(!std::isfinite(element))
                                                        throw std::runtime_error("Retail player grab cache is nonfinite");
                                            if(account==1)++player_grab_samples;
                                            else if(account==0)++zombie_grab_player_samples;
                                            else ++standing_grab_samples;
                                        };
                                        // Isolated complete PlyDG01 with original Claire clips.
                                        // NG02 timing/head cutting and PlayerLink remain separate.
                                        for(bool immediate:{false,true}) {
                                            auto standing=grabbing;
                                            auto& enemy=standing.enemies[root];
                                            if(back)enemy.zombie_flags|=0x4000;
                                            else enemy.zombie_flags&=~0x4000u;
                                            enemy.frame=(immediate?19:11)*65536;
                                            auto held_player=victim;
                                            held_player.player_work.emplace();
                                            held_player.base_motions=captured.base_motions;
                                            held_player.motions=held_player.base_motions;
                                            held_player.joint_pose=actor_bind_pose(player.models.at(0));
                                            held_player.modes={4,0,1,uint8_t(immediate?4:0)};
                                            const auto voice_count=standing.pending_player_voices.size();
                                            tick_zombie_player_grab(standing,root,held_player);++standing_grab_ticks;
                                            if(held_player.motion_number!=uint32_t(back?401:402) ||
                                               held_player.modes[3]!=(immediate?5:1) ||
                                               held_player.control0!=(!immediate && !back?1:0) ||
                                               !(held_player.flags&0x1000) ||
                                               enemy.grab_yaw_offset!=(back?-32768:0))
                                                throw std::runtime_error("Retail standing player startup/facing mismatch");
                                            const auto hold_frames=held_player.motions->at(held_player.motion_number)->frames;
                                            for(uint32_t frame=0;frame<hold_frames;++frame) {
                                                enemy.frame=(!immediate && frame==7?19:12)*65536;
                                                tick_zombie_player_grab(standing,root,held_player);++standing_grab_ticks;
                                                const auto expected_yaw=int32_t(uint16_t(uint32_t(enemy.angles[1])+
                                                    (back?32768u:0u)))+(!immediate && !back?int32_t(frame*546u):0);
                                                if(held_player.yaw_correction!=expected_yaw)
                                                    throw std::runtime_error("Retail standing Claire frame546 yaw mismatch");
                                                sample_player(standing,held_player,2);
                                            }
                                            if(standing.pending_player_voices.size()!=voice_count+1 ||
                                               standing.pending_player_voices.back()!=1026 || held_player.frame!=0)
                                                throw std::runtime_error("Retail standing hold/voice/sample boundary mismatch");
                                            held_player.modes[3]=2;
                                            tick_zombie_player_grab(standing,root,held_player);++standing_grab_ticks;
                                            if(held_player.motion_number!=uint32_t(back?404:405) ||
                                               held_player.modes[3]!=3 || held_player.frame!=0)
                                                throw std::runtime_error("Retail standing release setup mismatch");
                                            const auto release_frames=held_player.motions->at(held_player.motion_number)->frames;
                                            sample_player(standing,held_player,2);
                                            for(uint32_t frame=1;frame<release_frames;++frame) {
                                                tick_zombie_player_grab(standing,root,held_player);++standing_grab_ticks;
                                                sample_player(standing,held_player,2);
                                            }
                                            if(held_player.frame!=0)
                                                throw std::runtime_error("Retail standing release did not reach original loop boundary");
                                            const auto cached_x=float(held_player.model_work->joints[0].matrix[3]);
                                            const auto cached_z=float(held_player.model_work->joints[0].matrix[11]);
                                            standing.input_held=0xabcd;
                                            tick_zombie_player_grab(standing,root,held_player);++standing_grab_ticks;
                                            ++standing_grab_samples; // Callback's immediate base42 sample.
                                            if(held_player.motions!=held_player.base_motions ||
                                               held_player.motion_number!=42 || held_player.frame!=0 ||
                                               held_player.motion_step || held_player.blend_count!=8 ||
                                               held_player.blend_rate!=49152 ||
                                               held_player.modes!=std::array<uint8_t,4>{1,0,0,0} ||
                                               (held_player.flags&0x11004) || standing.input_held!=0xabc0 ||
                                               (enemy.zombie_flags&0xa4080) ||
                                               held_player.position.x!=cached_x || held_player.position.z!=cached_z ||
                                               held_player.player_work->movement_anchor.x!=cached_x ||
                                               held_player.player_work->movement_anchor.z!=cached_z)
                                                throw std::runtime_error("Retail standing cached-root/base42 release mismatch");
                                            for(const auto& pose:held_player.joint_pose)
                                                if(pose.position && (!std::isfinite(pose.position->x) ||
                                                    !std::isfinite(pose.position->y) || !std::isfinite(pose.position->z)))
                                                    throw std::runtime_error("Retail standing immediate base42 pose is nonfinite");
                                            ++standing_grab_releases;
                                        }
                                        const auto voices=held.pending_player_voices.size();
                                        for(size_t tick=0;tick<8;++tick) {
                                            tick_zombie_player_grab(held,root,captured);++player_grab_ticks;
                                            sample_player(held,captured);
                                            tick_zombie_motion(held,root,boundary_effects);
                                            calculate_zombie_model(held,root);
                                            tick_zombie_motion(held,body,boundary_effects);
                                            calculate_zombie_model(held,body);
                                        }
                                        if(captured.motion_number!=uint32_t(back?408:409) ||
                                           captured.modes[3]!=5 || captured.motions!=attacker.motions ||
                                           captured.control0!=8 || attacker.grab_yaw_offset!=(back?-32768:0) ||
                                           held.pending_player_voices.size()!=voices+1 ||
                                           held.pending_player_voices.back()!=1026)
                                            throw std::runtime_error("Retail original held-grab alignment/voice/clip mismatch");
                                        auto fatal=held;auto dying=captured;
                                        dying.modes[0]=6;dying.modes[3]=6;dying.health=-1;
                                        captured.modes[3]=2;
                                        tick_zombie_player_grab(held,root,captured);++player_grab_ticks;
                                        if(captured.motion_number!=uint32_t(back?410:411))
                                            throw std::runtime_error("Retail player escape clip mismatch");
                                        const auto escape_frames=captured.motions->at(captured.motion_number)->frames;
                                        bool released=false;
                                        for(size_t tick=0;tick<escape_frames+1;++tick) {
                                            tick_zombie_player_grab(held,root,captured);++player_grab_ticks;
                                            if(captured.modes==std::array<uint8_t,4>{1,0,0,0}) {released=true;break;}
                                            sample_player(held,captured);
                                        }
                                        if(!released || captured.motions!=captured.base_motions ||
                                           (captured.flags&0x10004) || held.input_held!=0xabc0)
                                            throw std::runtime_error("Retail complete player escape cleanup failed");
                                        ++player_grab_releases;
                                        tick_zombie_player_grab(fatal,root,dying);++player_grab_ticks;
                                        if(dying.motion_number!=uint32_t(back?412:413) ||
                                           fatal.pending_player_voices.back()!=1025)
                                            throw std::runtime_error("Retail player fatal grab clip/voice mismatch");
                                        const auto death_frames=dying.motions->at(dying.motion_number)->frames;
                                        for(size_t tick=0;tick<death_frames+1 && dying.motion_step;++tick) {
                                            tick_zombie_player_grab(fatal,root,dying);++player_grab_ticks;
                                            if(dying.motion_step)sample_player(fatal,dying);
                                        }
                                        if(dying.motion_step || !(dying.flags&2) || (dying.flags&0x10004) ||
                                           fatal.input_held!=0xabc0 || (fatal.enemies[root].zombie_flags&0xa4080))
                                            throw std::runtime_error("Retail complete player fatal-grab cleanup failed");
                                        ++player_grab_deaths;
                                        // Join actual captured NG00 to PlyDG00 in source order.
                                        // Press escape only after a real bite; fatal work begins
                                        // with1HP and reaches death through the same bite callback.
                                        // PlayerLink/world positioning remains a separate dependency.
                                        for(bool lethal : {false,true}) {
                                            auto combat=grabbing;
                                            auto& biting=combat.enemies[root];
                                            if(back)biting.zombie_flags|=0x4000;
                                            else biting.zombie_flags&=~0x4000u;
                                            auto bitten_player=victim;
                                            bitten_player.player_work.emplace();
                                            bitten_player.base_motions=captured.base_motions;
                                            bitten_player.motions=bitten_player.base_motions;
                                            bitten_player.joint_pose=actor_bind_pose(player.models.at(0));
                                            bitten_player.health=lethal?1:100;
                                            bool saw_bite=false,finished=false;
                                            const int32_t damage=biting.model_version==32?15:10;
                                            for(size_t tick=0;tick<240;++tick) {
                                                combat.input_pressed=saw_bite?0x2d0f:0;
                                                const auto old_health=bitten_player.health;
                                                tick_zombie_grab(combat,root,bitten_player);++zombie_grab_ticks;
                                                if(bitten_player.health!=old_health) {
                                                    if(saw_bite || bitten_player.health!=old_health-damage)
                                                        throw std::runtime_error("Retail joined grab bite damage/order mismatch");
                                                    saw_bite=true;++zombie_grab_bites;
                                                }
                                                tick_zombie_player_grab(combat,root,bitten_player);
                                                if(lethal && (bitten_player.flags&2) && !bitten_player.motion_step &&
                                                   biting.modes[3]==7) {finished=true;break;}
                                                if(!lethal && bitten_player.modes==std::array<uint8_t,4>{1,0,0,0} &&
                                                   biting.modes[0]!=2) {finished=true;break;}
                                                tick_zombie_motion(combat,root,boundary_effects);
                                                calculate_zombie_model(combat,root);
                                                tick_zombie_motion(combat,body,boundary_effects);
                                                calculate_zombie_model(combat,body);zombie_grab_enemy_samples+=2;
                                                if(bitten_player.modes[0]==4 || bitten_player.modes[0]==6)
                                                    sample_player(combat,bitten_player,false);
                                            }
                                            if(!finished || !saw_bite || (bitten_player.flags&0x10004) ||
                                               (combat.enemies[root].zombie_flags&0x20000))
                                                throw std::runtime_error("Retail joined enemy/player grab failed to release");
                                            if(lethal) {
                                                if(bitten_player.health>=0 || !(bitten_player.state_flags&0x40000) ||
                                                   combat.enemies[root].motion_number!=111 ||
                                                   combat.enemies[body].motion_number!=311)
                                                    throw std::runtime_error("Retail native bite fatal transition/cache relocation failed");
                                                ++zombie_grab_fatals;
                                            } else {
                                                if(bitten_player.health!=100-damage ||
                                                   bitten_player.motions!=bitten_player.base_motions)
                                                    throw std::runtime_error("Retail native bite escape failed to restore player bank");
                                                ++zombie_grab_escapes;
                                            }
                                        }
                                    }
                                    auto blood=initialized;
                                    auto& bitten=blood.enemies[root];
                                    const auto birth_seed=blood.random_seed;
                                    bitten.motion_number=8;
                                    tick_zombie_kami_effect(blood,root,28);
                                    tick_zombie_kami_effect(blood,root,38);
                                    if(!blood.effect_pool || blood.effect_pool->works[0].id!=5 ||
                                       blood.effect_pool->works[1].id!=302 || blood.effect_pool->works[4].id!=302 ||
                                       blood.effect_pool->works[7].id!=5 || bitten.damage_joint!=11 ||
                                       blood.effect_pool->works[1].linked_parent!=root ||
                                       blood.effect_pool->works[1].particle_storage!=std::array<size_t,2>{2,3} ||
                                       blood.effect_pool->works[4].particle_storage!=std::array<size_t,2>{5,6} ||
                                       blood.random_seed!=birth_seed || bitten.model->model!=raw)
                                        throw std::runtime_error("Retail cached zombie source blood birth mismatch");
                                    for(size_t joint=0;joint<cached.joints.size();++joint)
                                        if(bitten.model_work->joints[joint].matrix!=cached.joints[joint].matrix)
                                            throw std::runtime_error("Retail blood birth changed cached model");
                                    ++zombie_blood_birth_probes;
                                    auto heads=initialized;
                                    std::vector<size_t> head_sources{root};
                                    if(heads.enemies[root].zombie_work->body)
                                        head_sources.push_back(*heads.enemies[root].zombie_work->body);
                                    auto head_rng=initialized;
                                    size_t head_slot=0;
                                    for(const auto source:head_sources) {
                                        const auto& actor=heads.enemies[source];
                                        const auto owner=actor.pose_owner.value_or(source);
                                        const auto point=detail::ninja_point(detail::ninja_load(
                                            heads.enemies[owner].model_work->joints.at(10).matrix),{0,.5f,-1});
                                        const auto seed_before=heads.random_seed;
                                        tick_zombie_neck_blood_effect(heads,source,0);
                                        if(heads.random_seed!=seed_before)
                                            throw std::runtime_error("Retail neck blood consumed RNG");
                                        for(size_t i=0;i<8;++i) {
                                            const auto& effect=heads.effect_pool->works.at(head_slot+i);
                                            if(effect.flags!=0x81 || effect.id!=370 || effect.linked_parent!=source ||
                                               effect.linked_joint!=10 || effect.texture_owner!=owner ||
                                               effect.state_flags!=0x20 || effect.control0!=int(i/2) ||
                                               effect.control1!=((i&1)?3:6) || effect.texture_id<6 || effect.texture_id>11)
                                                throw std::runtime_error("Retail neck blood cached-owner birth mismatch");
                                        }
                                        head_slot+=8;
                                        tick_zombie_head_fragment_effect(heads,source,1);
                                        for(size_t i=0;i<4;++i) {
                                            const auto roll=scenario_random(head_rng);
                                            const float scale=2.f+float(-int32_t(roll))/-2.1474836E9f;
                                            const auto& effect=heads.effect_pool->works.at(head_slot+i);
                                            if(effect.flags!=1 || effect.id!=362 || effect.type!=0 || effect.linked_parent ||
                                               effect.texture_owner!=owner || effect.state_flags!=0x20 ||
                                               effect.texture_id<5 || effect.texture_id>10 ||
                                               effect.position.x!=point.x || effect.position.y!=point.y || effect.position.z!=point.z ||
                                               effect.scale.x!=scale || effect.scale.y!=scale || effect.scale.z!=0 ||
                                               !std::isfinite(effect.direction.x+effect.direction.y+effect.direction.z))
                                                throw std::runtime_error("Retail head fragment original joint/RNG birth mismatch");
                                        }
                                        head_slot+=4;
                                        if(heads.random_seed!=head_rng.random_seed ||
                                           heads.enemies[owner].model->model!=initialized.enemies[owner].model->model ||
                                           actor.damage_velocity.x!=initialized.enemies[source].damage_velocity.x)
                                            throw std::runtime_error("Retail head births changed assets or RNG order");
                                        const auto& before=*initialized.enemies[owner].model_work;
                                        for(size_t j=0;j<before.joints.size();++j)
                                            if(heads.enemies[owner].model_work->joints[j].matrix!=before.joints[j].matrix)
                                                throw std::runtime_error("Retail head births changed original cached matrices");
                                    }
                                    size_t head_active=0;
                                    for(const auto& effect:heads.effect_pool->works)head_active+=bool(effect.flags&1);
                                    if(head_active!=head_slot)
                                        throw std::runtime_error("Retail head birth allocation count mismatch");
                                    ++zombie_head_birth_probes;zombie_head_birth_works+=head_slot;
                                    auto water=initialized;
                                    auto& wet=water.enemies[root];
                                    wet.motion_number=33;wet.state_flags|=0x100000;
                                    tick_zombie_water_effect(water,root,12);
                                    size_t rings=0,splashes=0;
                                    if(!water.effect_pool)throw std::runtime_error("Retail step birth missing effect pool");
                                    for(const auto& effect:water.effect_pool->works)if(effect.flags&1) {
                                        if(effect.id==108) {
                                            ++rings;
                                            const auto& matrix=cached.joints[0].matrix;
                                            if(effect.position.x!=float(matrix[3]) || effect.position.y!=float(matrix[7]) ||
                                               effect.position.z!=float(matrix[11]) || effect.linked_joint!=15)
                                                throw std::runtime_error("Retail step ring did not use original cached root");
                                        } else if(effect.id==11) {
                                            ++splashes;
                                            if(effect.scale.x!=2 || effect.type!=1 || effect.linked_parent)
                                                throw std::runtime_error("Retail step splash metadata mismatch");
                                        } else throw std::runtime_error("Unexpected retail step effect birth");
                                    }
                                    if(rings!=2 || splashes%3 || splashes>6 || water.random_seed!=birth_seed ||
                                       wet.model->model!=raw)
                                        throw std::runtime_error("Retail duplicate step events or creation RNG mismatch");
                                    for(size_t joint=0;joint<cached.joints.size();++joint)
                                        if(wet.model_work->joints[joint].matrix!=cached.joints[joint].matrix)
                                            throw std::runtime_error("Retail step birth changed cached model");
                                    ++zombie_water_birth_probes;
                                    auto vibration=initialized;
                                    auto& attack=vibration.enemies[root];
                                    if(!attack.motions->at(8) || attack.motions->at(8)->frames<=24)
                                        throw std::runtime_error("Retail original attack motion missing vibration frame24");
                                    attack.motion_number=8;attack.modes[0]=1;
                                    tick_zombie_motion_table(vibration,root,24);++zombie_motion_table_ticks;
                                    if(vibration.pending_enemy_vibrations.size()!=1 ||
                                       vibration.pending_enemy_vibrations[0].preset!=9 ||
                                       vibration.pending_enemy_vibrations[0].channel!=1)
                                        throw std::runtime_error("Retail original motion-table vibration request changed");
                                    zombie_motion_vibration_requests+=vibration.pending_enemy_vibrations.size();
                                }
                                auto search_neck=initialized;
                                auto& searching=search_neck.enemies[body];
                                search_neck.enemies[root].zombie_flags|=0x800;
                                search_neck.enemies[root].zombie_work->awareness|=0x40;
                                searching.zombie_work->player_distance=10;
                                searching.zombie_work->neck_yaw=-2000;
                                for(size_t tick=0;tick<64;++tick) {
                                    const auto before=search_neck.enemies[root].joint_pose[11].angles.value();
                                    const auto prior=searching.zombie_work->neck_yaw;
                                    const auto frame=searching.frame;
                                    tick_zombie_neck(search_neck,body,claire);
                                    const auto after=search_neck.enemies[root].joint_pose[11].angles.value();
                                    const auto delta=uint16_t(uint32_t(after[1])-uint32_t(prior));
                                    if(after[0]!=before[0] || after[2]!=before[2] || after[1]!=searching.zombie_work->neck_yaw ||
                                       (delta>910 && delta<65536-910) || searching.frame!=frame ||
                                       search_neck.enemies[root].model->model!=raw)
                                        throw std::runtime_error("Retail SearchNeck changed unrelated work or retained yaw policy");
                                    for(size_t joint=0;joint<cached.joints.size();++joint)
                                        if(search_neck.enemies[root].model_work->joints[joint].matrix!=cached.joints[joint].matrix)
                                            throw std::runtime_error("Retail SearchNeck changed prior cached world matrices");
                                    ++zombie_search_neck_ticks;
                                }
                                for(auto work:{root,body}) {
                                    auto& sensed=targeting.enemies[work];
                                    sensed.zombie_work->awareness=0;
                                    sensed.zombie_work->awareness_timeout=0;
                                    sensed.zombie_flags&=~0x400u;
                                    const auto& neck=cached.joints.at(11).matrix;
                                    const Vec3 sense_origin{float(neck[3]),0,float(neck[11])};
                                    const auto sense_yaw=int32_t(uint32_t(sensed.angles[1])+uint32_t(owner.joint_pose.at(11).angles->at(1)));
                                    const auto visible=search_enemy_player(sense_origin,sense_yaw,player_entry.player.position,
                                        zombie_personality_at(sensed.zombie_work->personality).view_angle)!=-1;
                                    const auto prior_flags=sensed.zombie_flags;
                                    for(size_t tick=0;tick<93;++tick) {
                                        tick_zombie_awareness(targeting,work,player_entry.player.position);
                                        const auto want=uint8_t((tick+1)%31)+(visible?0x40:0);
                                        if(sensed.zombie_work->awareness!=want ||
                                           sensed.zombie_work->awareness_timeout!=(visible?15:0) ||
                                           sensed.zombie_flags!=(prior_flags|(visible?0x400:0)))
                                            throw std::runtime_error("Retail native awareness trace changed source counter/latch policy");
                                        ++zombie_awareness_ticks;
                                    }
                                }
                                for(auto work:{root,body}) for(size_t tick=0;tick<32;++tick) {
                                    const auto before=owner.joint_pose.at(11).angles.value();
                                    const auto frame=targeting.enemies[work].frame;
                                    rotate_zombie_neck(targeting,work,11,claire,5);
                                    const auto after=owner.joint_pose.at(11).angles.value();
                                    const auto delta=uint16_t(after[1]-before[1]);
                                    if(after[0]!=before[0] || after[2]!=before[2] ||
                                       (delta>910 && delta<65536-910) || targeting.enemies[work].frame!=frame ||
                                       owner.model->model!=raw)
                                        throw std::runtime_error("Retail cached neck update changed unrelated work or step size");
                                    for(size_t joint=0;joint<cached.joints.size();++joint)
                                        if(owner.model_work->joints[joint].matrix!=cached.joints[joint].matrix)
                                            throw std::runtime_error("Retail neck wrapper changed prior cached matrices");
                                    ++zombie_neck_updates;
                                }
                            }
                        }
                        // Exercise production auxiliary callbacks without claiming
                        // the separate root movement/damage controller is ready.
                        auto scheduled_parts=initialized;
                        for(auto& work:scheduled_parts.enemies) {
                            if(work.id==1)work.state_flags|=0x40000000;
                            else if(work.id==31 || work.id==32 || work.id==33 || work.id==38) {
                                work.modes[0]=0;
                                work.damage.fill(99);work.combo_timeout=4;
                                work.combo_flags=0xff;work.secondary_flags|=4;
                            } else work.flags&=~1u;
                        }
                        size_t nearest=roots.front();
                        const auto origin=records.positions.at(0).position;
                        for(auto root:roots) {
                            auto& work=scheduled_parts.enemies[root];
                            const auto dx=work.position.x-origin.x,dy=work.position.y-origin.y,dz=work.position.z-origin.z;
                            work.zombie_work->player_distance=std::sqrt(dx*dx+dy*dy+dz*dz);
                            if(work.zombie_work->player_distance<scheduled_parts.enemies[nearest].zombie_work->player_distance)
                                nearest=root;
                        }
                        const auto seed=scheduled_parts.random_seed;
                        scheduled_parts.flags[6][0]=2;
                        for(size_t frame=0;frame<64;++frame) {
                            tick_scenario_actors(scheduled_parts);
                            if(scheduled_parts.random_seed!=seed)
                                throw std::runtime_error("Native auxiliary idle callback consumed RNG");
                            for(size_t slot=0;slot<scheduled_parts.enemies.size();++slot) {
                                const auto& work=scheduled_parts.enemies[slot];
                                if(!(work.flags&1) || (work.id!=31 && work.id!=32 && work.id!=33 && work.id!=38))continue;
                                const auto owner=work.pose_owner.value();
                                const auto joints=scheduled_parts.enemies.at(owner).model_work->joints.size();
                                if(work.modes!=std::array<uint8_t,4>{1,0,0,0} || work.secondary_flags&4 ||
                                   work.combo_flags!=0x8f || work.combo_timeout!=std::max(0,3-int(frame)) ||
                                   !(work.model_flags&1) || work.model->model!=initialized.enemies[slot].model->model)
                                    throw std::runtime_error("Retail auxiliary callback or common frame cleanup mismatch");
                                for(size_t joint=0;joint<work.damage.size();++joint)
                                    if(work.damage[joint]!=(joint<joints?0:99))
                                        throw std::runtime_error("Retail auxiliary damage cleanup owner/count mismatch");
                                ++zombie_auxiliary_frames;
                                if(frame && work.id==38)++zombie_manager_frames;
                            }
                            if(frame) for(auto root:roots)
                                if(bool(scheduled_parts.enemies[root].zombie_secondary_flags&2)!=(root==nearest))
                                    throw std::runtime_error("Retail manager did not select nearest cached root");
                        }
                        for(auto root:roots) {
                            const auto& zombie=initialized.enemies[root];
                            ContactBody enemy;
                            enemy.position=zombie.position;
                            enemy.offset=zombie.collision_offset;
                            enemy.flags=zombie.flags;
                            enemy.state_flags=zombie.state_flags;
                            enemy.secondary_flags=zombie.secondary_flags;
                            enemy.radius=zombie.geometry.collision_radius;
                            enemy.height=zombie.geometry.collision_height;
                            ContactBody player_contact;
                            player_contact.flags=8;
                            player_contact.position=zombie.position;
                            player_contact.position.z+=1;
                            player_contact.radius=2.5f;player_contact.height=15.5f;
                            if(!collide_enemy_player(enemy,player_contact) || !(enemy.state_flags&4) ||
                               !(player_contact.state_flags&2) ||
                               std::abs(enemy.position.z-(zombie.position.z-2))>.002f ||
                               std::abs(player_contact.position.z-(zombie.position.z+3))>.002f)
                                throw std::runtime_error("Retail native player/zombie contact differs from source");
                            ++zombie_player_contacts;
                        }
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
        std::cout<<"Decoded "<<lip_clips<<" resident lip clips in "<<lip_rooms
                 <<" rooms with "<<lip_segments<<" validated segments\n";
        if(scenario_check)std::cout<<"Initialized "<<facial_actor_startups
            <<" selected native facial actors and advanced "<<facial_actor_ticks
            <<" joined mask/body/model ticks with "<<facial_actor_changed_frames
            <<" changed geometry frames\n";
        if(scenario_check)std::cout<<"Played selected retail lip clip through "<<facial_lip_ticks
            <<" native actor ticks to termination with "<<facial_lip_changed_frames<<" changed geometry frames\n";
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
            if(zombie_auxiliary_frames!=zombie_auxiliary_startups*64 || zombie_manager_frames!=zombie_managers*63)
                throw std::runtime_error("Missing retail auxiliary/manager scheduler coverage");
            std::cout<<"Dispatched "<<zombie_auxiliary_frames<<" native auxiliary frames, including "
                     <<zombie_manager_frames<<" manager policy updates and original post-controller cleanup\n";
            std::cout<<"Verified "<<zombie_player_contacts<<" original player/zombie contacts with initialized retail geometry\n";
            if(!enemy_floor_probes)throw std::runtime_error("Missing retail enemy floor trigger coverage");
            std::cout<<"Queried "<<enemy_floor_probes<<" authored enemy floor trigger centers\n";
            if(!enemy_stair_probes)throw std::runtime_error("Missing authored stair query coverage");
            std::cout<<"Queried "<<enemy_stair_probes<<" authored stair trigger centers\n";
            if(!route_regions || !route_queries)throw std::runtime_error("Missing retail native route coverage");
            std::cout<<"Decoded "<<route_regions<<" route regions and queried "<<route_queries
                     <<" native center-to-center routes; "<<invalid_route_hops<<" out-of-range source hops excluded\n";
            if(runtime_check) {
                if(zombie_view_probes!=zombie_parameters || zombie_neck_updates!=zombie_parameters*64)
                    throw std::runtime_error("Missing retail native zombie targeting coverage");
                std::cout<<"Probed "<<zombie_view_probes<<" actual personality view cones and applied "
                         <<zombie_neck_updates<<" cached root/body neck updates against Claire joint5\n";
                if(zombie_awareness_ticks!=zombie_parameters*2*93)
                    throw std::runtime_error("Missing native graveyard awareness trace coverage");
                std::cout<<"Advanced "<<zombie_awareness_ticks<<" isolated native root/body awareness updates\n";
                if(!zombie_brain_roots || zombie_brain_ticks!=zombie_brain_roots*93)
                    throw std::runtime_error("Missing isolated native Brain00 pursuit coverage");
                std::cout<<"Advanced "<<zombie_brain_ticks<<" isolated Brain00 close-pursuit updates across "
                         <<zombie_brain_roots<<" original roots\n";
                if(zombie_idle_transitions!=zombie_brain_roots || !zombie_idle_ticks || !zombie_idle_sound_requests ||
                   zombie_search_neck_ticks!=zombie_parameters*64)
                    throw std::runtime_error("Missing retail native MV00/SearchNeck coverage");
                std::cout<<"Advanced "<<zombie_idle_ticks<<" isolated MV00 ticks through "<<zombie_idle_transitions
                         <<" idle-to-walk transitions with "<<zombie_idle_sound_requests<<" retained native sound requests\n";
                std::cout<<"Advanced "<<zombie_search_neck_ticks<<" native linked-body SearchNeck updates\n";
                if(zombie_walk_ticks!=zombie_brain_roots*120 || !zombie_root_displacements || zombie_speed_samples!=zombie_root_displacements)
                    throw std::runtime_error("Missing retail native MV01/root displacement coverage");
                std::cout<<"Advanced "<<zombie_walk_ticks<<" isolated MV01 ticks with actual room walls; probed "
                         <<zombie_root_displacements<<" walking root-key displacements and "<<zombie_speed_samples<<" speed samples\n";
                if(zombie_motion_table_ticks!=zombie_brain_roots*161 || zombie_foot_zero_probes!=zombie_brain_roots*80*6 ||
                   !zombie_motion_sound_requests || zombie_motion_vibration_requests!=zombie_brain_roots)
                    throw std::runtime_error("Missing retail native foot-lock/motion-table coverage");
                std::cout<<"Applied "<<zombie_motion_table_ticks<<" isolated native motion-table callbacks with "
                         <<zombie_motion_sound_requests<<" sound requests and "<<zombie_motion_vibration_requests
                         <<" vibration requests; verified "<<zombie_foot_zero_probes<<" exact zero-drift cached foot paths\n";
                if(zombie_set_motion_ticks!=zombie_brain_roots*160 || zombie_native_effect_checks!=zombie_set_motion_ticks*2)
                    throw std::runtime_error("Retail SetMtn/native-effect probe coverage mismatch");
                std::cout<<"Advanced "<<zombie_set_motion_ticks<<" paired source SetMtn calls with "
                         <<zombie_native_effect_checks<<" native Kami/Water routing calls\n";
                if(zombie_blood_birth_probes!=zombie_brain_roots || zombie_water_birth_probes!=zombie_brain_roots)
                    throw std::runtime_error("Retail native effect birth coverage mismatch");
                std::cout<<"Verified "<<zombie_blood_birth_probes<<" blood/particle and "<<zombie_water_birth_probes
                         <<" duplicate step birth probes using original cached models and room ground/water\n";
                if(zombie_head_birth_probes!=zombie_brain_roots ||
                   zombie_head_birth_works!=zombie_brain_roots*24)
                    throw std::runtime_error("Retail standing-grab head birth coverage mismatch");
                std::cout<<"Verified "<<zombie_head_birth_probes<<" standing-grab neck blood/head fragment probes and "
                         <<zombie_head_birth_works<<" births using original root/body cached joints and texture owners\n";
                if(zombie_close_pursuit_ticks!=zombie_brain_roots*18 || zombie_walk_phase_boundaries!=zombie_brain_roots)
                    throw std::runtime_error("Retail close-pursuit source state coverage mismatch");
                std::cout<<"Applied "<<zombie_close_pursuit_ticks<<" isolated source close-pursuit states and "
                         <<zombie_walk_phase_boundaries<<" literal walking phase boundaries with paired terminal sampling/cache updates\n";
                if(zombie_capture_probes!=zombie_brain_roots || zombie_lunge_ticks<=zombie_brain_roots*13)
                    throw std::runtime_error("Retail native lunge/capture probe count mismatch");
                std::cout<<"Advanced "<<zombie_lunge_ticks<<" native lunge states with paired motion/cache/effect routing and "
                         <<zombie_capture_probes<<" cached Claire capture/player-state transitions\n";
                if(player_grab_releases!=zombie_brain_roots*2 || player_grab_deaths!=player_grab_releases ||
                   player_grab_samples<player_grab_releases*8 || player_grab_ticks<=player_grab_samples)
                    throw std::runtime_error("Retail player held-grab coverage mismatch");
                std::cout<<"Advanced "<<player_grab_ticks<<" player held-grab states and "<<player_grab_samples
                         <<" original Claire motion/cache samples; "<<player_grab_releases<<" escapes and "
                         <<player_grab_deaths<<" fatal releases across both facings\n";
                if(zombie_grab_escapes!=zombie_brain_roots*2 || zombie_grab_fatals!=zombie_grab_escapes ||
                   zombie_grab_bites!=zombie_grab_escapes+zombie_grab_fatals ||
                   zombie_grab_player_samples<zombie_grab_bites*25 || zombie_grab_enemy_samples<zombie_grab_bites*50)
                    throw std::runtime_error("Retail joined zombie grab coverage mismatch");
                std::cout<<"Advanced "<<zombie_grab_ticks<<" joined enemy/player grab states with "<<zombie_grab_bites
                         <<" authored bites, "<<zombie_grab_escapes<<" input escapes and "<<zombie_grab_fatals
                         <<" fatal releases; "<<zombie_grab_player_samples<<" original Claire and "
                         <<zombie_grab_enemy_samples<<" paired enemy motion/cache samples\n";
                if(standing_grab_releases!=zombie_brain_roots*4 ||
                   standing_grab_samples<standing_grab_releases*3 || standing_grab_ticks<=standing_grab_samples)
                    throw std::runtime_error("Retail standing player callback coverage mismatch");
                std::cout<<"Advanced "<<standing_grab_ticks<<" standing player grab states and "
                         <<standing_grab_samples<<" original Claire motion samples; "
                         <<standing_grab_releases<<" cached-root/base42 releases across both facings and startup paths\n";
                if(failed_grab_recoveries+failed_grab_falls!=zombie_brain_roots ||
                   failed_grab_samples<zombie_brain_roots*2 || failed_grab_ticks<=zombie_brain_roots)
                    throw std::runtime_error("Retail NG01 failed-grab coverage mismatch");
                std::cout<<"Advanced "<<failed_grab_ticks<<" native failed-grab states and "
                         <<failed_grab_samples<<" paired original motion/cache samples; "
                         <<failed_grab_recoveries<<" recoveries and "<<failed_grab_falls<<" cliff deaths\n";
                if(zombie_model_ticks!=zombie_set_motion_ticks || zombie_cache_draws!=zombie_brain_roots*3 ||
                   zombie_cache_changed_draws!=zombie_brain_roots*2)
                    throw std::runtime_error("Retail CalcEnemy/cache drawing coverage mismatch");
                std::cout<<"Applied "<<zombie_model_ticks<<" source CalcEnemy cache/collision updates; derived "
                         <<zombie_cache_draws<<" headless posed draws with "<<zombie_cache_changed_draws<<" changed frames\n";
                if(zombie_mouth_draws!=zombie_cache_draws || zombie_mouth_changed_draws<zombie_brain_roots)
                    throw std::runtime_error("Retail mouth morph drawing coverage mismatch");
                std::cout<<"Drew "<<zombie_mouth_draws<<" source mouth morph frames with "<<zombie_mouth_changed_draws
                         <<" changed same-cache geometry frames; retained raw assets, poses and collision caches\n";
            }
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
