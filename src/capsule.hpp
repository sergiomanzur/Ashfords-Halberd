#pragma once
#include "ninja_matrix.hpp"
#include "vector.hpp"
#include <cmath>
// Native ports of MIT recvx-decomp ps2_NaColi.c (njDistanceP2L, njDistanceL2L,
// njCollisionCheckSS) and njplus.c (npCollisionCheckCCEx, npDistanceP2C).
// Evaluation order follows the source. The PS2 vector-unit sqrt/rsqrt are
// replaced by IEEE operations; as elsewhere in this port a zero-length vector
// normalizes to zero and non-finite intermediate values are rejected.
namespace ashford {
struct Line3 {
  Vec3 point, direction;
};
struct Sphere3 {
  Vec3 center;
  float radius = 0;
};
struct Capsule3 {
  Vec3 start, end;
  float radius = 0;
};
namespace detail {
inline float cap_finite(float v) { return ninja_finite(v); }
inline Vec3 cap_vec(Vec3 v) {
  cap_finite(v.x);
  cap_finite(v.y);
  cap_finite(v.z);
  return v;
}
inline Vec3 cap_sub(Vec3 a, Vec3 b) {
  return cap_vec({a.x - b.x, a.y - b.y, a.z - b.z});
}
inline float cap_dot(Vec3 a, Vec3 b) {
  return cap_finite((a.x * b.x + a.y * b.y) + a.z * b.z);
}
inline float cap_length(Vec3 v) { return std::sqrt(cap_dot(v, v)); }
// njInvertSqrt of a squared length; zero maps to zero (see header note).
inline float cap_inverse_length(float squared) {
  return squared ? cap_finite(1.f / std::sqrt(squared)) : 0.f;
}
inline Vec3 cap_unit(Vec3 v) {
  const float inv = cap_inverse_length(cap_dot(v, v));
  return cap_vec({v.x * inv, v.y * inv, v.z * inv});
}
} // namespace detail
// njDistanceP2L: distance from a point to an infinite line plus closest point.
inline float distance_point_line(Vec3 p, const Line3 &line, Vec3 *closest) {
  using namespace detail;
  const float inv = cap_inverse_length(cap_dot(line.direction, line.direction));
  Vec3 d{line.direction.x * inv, line.direction.y * inv,
         line.direction.z * inv};
  cap_vec(d);
  const Vec3 l = cap_sub(p, line.point);
  const float squared = cap_dot(l, l);
  if (squared == 0) {
    if (closest)
      *closest = line.point;
    return 0;
  }
  const float along = cap_dot(d, l);
  if (closest)
    *closest = cap_vec({line.point.x + d.x * along, line.point.y + d.y * along,
                        line.point.z + d.z * along});
  return std::sqrt(std::fabs(squared - along * along));
}
// njDistanceL2L: skew-line distance with the source's 0.975 parallel and
// 0.158 zero-distance thresholds. Closest points are always written.
inline float distance_line_line(const Line3 &a, const Line3 &b, Vec3 &closest_a,
                                Vec3 &closest_b) {
  using namespace detail;
  const float ia = cap_inverse_length(cap_dot(a.direction, a.direction));
  const Vec3 v1 = cap_vec(
      {a.direction.x * ia, a.direction.y * ia, a.direction.z * ia});
  const float ib = cap_inverse_length(cap_dot(b.direction, b.direction));
  const Vec3 v2 = cap_vec(
      {b.direction.x * ib, b.direction.y * ib, b.direction.z * ib});
  float iv = cap_dot(v1, v2);
  if (0.975f < std::fabs(iv)) {
    closest_a = a.point;
    return distance_point_line(a.point, b, &closest_b);
  }
  const Vec3 d = cap_sub(b.point, a.point);
  const float nx = cap_dot(d, v1), ny = cap_dot(d, v2), nz = 1.0f - (iv * iv);
  float t = (nx - (iv * ny)) / nz;
  closest_a = cap_vec({a.point.x + v1.x * t, a.point.y + v1.y * t,
                       a.point.z + v1.z * t});
  t = ((nx * iv) - ny) / nz;
  closest_b = cap_vec({b.point.x + v2.x * t, b.point.y + v2.y * t,
                       b.point.z + v2.z * t});
  const Vec3 n{(v1.y * v2.z) - (v1.z * v2.y), (v1.z * v2.x) - (v1.x * v2.z),
               (v1.x * v2.y) - (v1.y * v2.x)};
  t = std::fabs(cap_dot(n, d));
  iv = t * cap_inverse_length(cap_dot(n, n));
  if (iv < 0.158f)
    iv = 0;
  return cap_finite(iv);
}
// njCollisionCheckSS.
inline bool spheres_collide(const Sphere3 &a, const Sphere3 &b) {
  const Vec3 d = detail::cap_sub(b.center, a.center);
  const float r = a.radius + b.radius;
  return detail::cap_dot(d, d) <= r * r;
}
// npCollisionCheckCCEx: capsule/capsule contact returning a hit position.
// The shorter capsule is always used as the first operand, like the source.
inline bool collide_capsules_ex(const Capsule3 &first, const Capsule3 &second,
                                Vec3 &position) {
  using namespace detail;
  Line3 lna{first.start, cap_sub(first.end, first.start)};
  Line3 lnb{second.start, cap_sub(second.end, second.start)};
  float lena = cap_length(lna.direction), lenb = cap_length(lnb.direction);
  const Capsule3 *capa = &first, *capb = &second;
  if (lena > lenb) {
    capa = &second;
    capb = &first;
    std::swap(lna, lnb);
    std::swap(lena, lenb);
  }
  Sphere3 sa, sb;
  distance_line_line(lna, lnb, sa.center, sb.center);
  sa.radius = capa->radius;
  sb.radius = capb->radius;
  // The source normalizes both directions in place at this point.
  Vec3 vec = cap_sub(sa.center, lna.point);
  float length = cap_length(vec);
  vec = cap_unit(vec);
  lna.direction = cap_unit(lna.direction);
  float inner = cap_dot(vec, lna.direction);
  if (inner > 0) {
    if (length > lena)
      sa.center = capa->end;
  } else
    sa.center = capa->start;
  vec = cap_sub(sb.center, lnb.point);
  length = cap_length(vec);
  vec = cap_unit(vec);
  lnb.direction = cap_unit(lnb.direction);
  inner = cap_dot(vec, lnb.direction);
  if (inner > 0) {
    if (length > lenb)
      sb.center = capb->end;
  } else
    sb.center = capb->start;
  auto hit = [&]() {
    if (!spheres_collide(sa, sb))
      return false;
    Vec3 scl = cap_sub(sb.center, sa.center);
    if (cap_length(scl) > sa.radius) {
      scl = cap_unit(scl);
      position = cap_vec({sa.center.x + scl.x * sa.radius,
                          sa.center.y + scl.y * sa.radius,
                          sa.center.z + scl.z * sa.radius});
    } else
      position = sb.center;
    return true;
  };
  if (hit())
    return true;
  for (const Vec3 &end : {capa->start, capa->end}) {
    sa.center = end;
    distance_point_line(sa.center, lnb, &sb.center);
    vec = cap_sub(sb.center, lnb.point);
    length = cap_length(vec);
    vec = cap_unit(vec);
    inner = cap_dot(vec, lnb.direction);
    if (inner > 0) {
      if (length > lenb)
        sb.center = capb->end;
    } else
      sb.center = capb->start;
    if (hit())
      return true;
  }
  return false;
}
// npDistanceP2C: closest point on a capsule's surface to `point`; a point
// inside the capsule returns itself.
inline Vec3 distance_point_capsule(Vec3 point, const Capsule3 &capsule) {
  using namespace detail;
  Line3 line{capsule.start, cap_sub(capsule.end, capsule.start)};
  Vec3 hit;
  distance_point_line(point, line, &hit);
  const float length = cap_length(line.direction);
  Vec3 vec = cap_sub(hit, line.point);
  const float along = cap_length(vec);
  vec = cap_unit(vec);
  line.direction = cap_unit(line.direction);
  const float inner = cap_dot(vec, line.direction);
  if (inner > 0) {
    if (along > length)
      hit = capsule.end;
  } else
    hit = capsule.start;
  vec = cap_sub(point, hit);
  if (capsule.radius < cap_length(vec)) {
    vec = cap_unit(vec);
    return cap_vec({hit.x + vec.x * capsule.radius,
                    hit.y + vec.y * capsule.radius,
                    hit.z + vec.z * capsule.radius});
  }
  return point;
}
} // namespace ashford
