#include "actor.hpp"
#include "ninja_matrix.hpp"
#include <stdexcept>
namespace ashford {
Vec3 calculate_actor_fix_offset(const std::vector<JointPose> &pose,
                                const ActorModelWork &cached,
                                const ActorPlacement &placement,
                                const std::vector<int8_t> &path, Vec3 offset) {
  size_t count = 0;
  while (count < path.size() && path[count] != -1) {
    if (path[count] < 0 || size_t(path[count]) >= pose.size() ||
        !pose[path[count]].position)
      throw std::runtime_error("Invalid actor fix object path");
    ++count;
  }
  if (!count || count == path.size() ||
      size_t(path[count - 1]) >= cached.joints.size())
    throw std::runtime_error("Undefined actor fix terminal path/cache");
  auto matrix = detail::ninja_identity();
  if (placement.flags & 0x80) {
    if (!placement.parent_joint)
      throw std::runtime_error("Missing actor fix parent matrix");
    matrix = detail::ninja_load(*placement.parent_joint);
    detail::ninja_translate(matrix, placement.linked_offset);
  } else
    detail::ninja_translate(matrix, placement.position);
  // Source does not inspect1000 or cached object flags while rebuilding path.
  detail::ninja_rotate(matrix, placement.angles);
  for (size_t i = 0; i < count; ++i) {
    const auto &joint = pose[path[i]];
    detail::ninja_translate(matrix, *joint.position);
    detail::ninja_rotate(matrix,
                         joint.angles.value_or(std::array<int32_t, 3>{}));
  }
  const auto current = detail::ninja_point(matrix, offset);
  const auto previous = detail::ninja_point(
      detail::ninja_load(cached.joints[path[count - 1]].matrix), offset);
  return {detail::ninja_finite(current.x - previous.x),
          detail::ninja_finite(current.y - previous.y),
          detail::ninja_finite(current.z - previous.z)};
}
} // namespace ashford
