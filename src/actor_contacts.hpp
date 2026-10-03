#pragma once
#include "scene.hpp"
#include <optional>
namespace ashford {
struct ContactBody {
  Vec3 position{}, offset{}, normal_anchor{};
  float radius = 0, height = 0;
  uint32_t flags = 0, state_flags = 0, secondary_flags = 0;
  std::optional<size_t> linked_parent;
};
struct ActorContacts {
  ContactBody player;
  std::vector<ContactBody> enemies;
};
// hitchk.c::bhCheckPlayer. The source's normal anchor is used when player
// flag8 is clear. Mutation is committed only after response succeeds.
bool collide_enemy_player(ContactBody &enemy, ContactBody &player);
// hitchk.c::bhCheckEnemies. No subject_slot means the subject is the player.
// An enemy subject must refer to the indicated element of pool.
size_t collide_actor_pool(ContactBody &subject, std::vector<ContactBody> &pool,
                          std::optional<size_t> subject_slot = {});
} // namespace ashford
