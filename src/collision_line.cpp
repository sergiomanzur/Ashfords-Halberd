#include "collision.hpp"
#include "ninja_matrix.hpp"
#include <cmath>
#include <stdexcept>
// Native adaptation of MIT recvx-decomp hitchkl.c and ps2_NaColi.c.
// Shape face order and signed dimensions are retained; polygon tops avoid
// the source's reads of uninitialized vertices during construction.
namespace ashford {
namespace {
float finite(float v) { return detail::ninja_finite(v); }
Vec3 vector(Vec3 v) {
  finite(v.x);
  finite(v.y);
  finite(v.z);
  return v;
}
Vec3 sub(Vec3 a, Vec3 b) { return vector({a.x - b.x, a.y - b.y, a.z - b.z}); }
float dot(Vec3 a, Vec3 b) {
  return finite((a.x * b.x + a.y * b.y) + a.z * b.z);
}
float length(Vec3 v) { return std::sqrt(dot(v, v)); }
Vec3 unit(Vec3 v) {
  const float squared = dot(v, v),
              inv = squared ? finite(1.f / std::sqrt(squared)) : 0.f;
  return vector({v.x * inv, v.y * inv, v.z * inv});
}
float source_sin(float a) { return ninja_sin_cos(uint16_t(int32_t(a)))[0]; }
float source_cos(float a) { return ninja_sin_cos(uint16_t(int32_t(a)))[1]; }
// njDistanceL2PL normalizes both vectors, including its .025 fallback.
bool cross_plane(Vec3 start, Vec3 delta, Vec3 origin, Vec3 normal, Vec3 &cp) {
  const auto n = unit(normal), line = unit(delta);
  float t = dot(n, line);
  const auto relative = sub(start, origin);
  if (.025f < std::fabs(t)) {
    t = finite(-dot(n, relative) / t);
    cp = vector(
        {start.x + line.x * t, start.y + line.y * t, start.z + line.z * t});
    return true;
  }
  t = dot(n, relative);
  cp = vector({start.x - n.x * t, start.y - n.y * t, start.z - n.z * t});
  return std::fabs(t) == 0.f;
}
Vec3 plane_normal(const Vec3 *area) {
  const auto first = sub(area[1], area[0]), second = sub(area[2], area[1]);
  return vector({first.y * second.z - first.z * second.y,
                 first.z * second.x - first.x * second.z,
                 first.x * second.y - first.y * second.x});
}
bool inside(Vec3 point, const Vec3 *area, Vec3 normal, int count) {
  if (count < 3)
    return false;
  for (int i = 0; i < count; ++i) {
    const auto a = sub(area[(i + 1) % count], area[i]), b = sub(point, area[i]);
    const float side = finite(normal.z * (a.x * b.y - a.y * b.x) +
                              (normal.x * (a.y * b.z - a.z * b.y) +
                               normal.y * (a.z * b.x - a.x * b.z)));
    if (side < 0.f)
      return false;
  }
  return true;
}
int polygon(const Vec3 *p1, Vec3 *p2, const Vec3 *area, int count,
            Vec3 &hit_normal) {
  const auto delta = sub(*p2, *p1), normal = plane_normal(area);
  const float segment_length = length(delta);
  Vec3 cp;
  if (dot(delta, normal) < 0.f &&
      cross_plane(*p1, delta, area[0], normal, cp)) {
    const auto dv = sub(cp, *p1);
    if (dot(delta, dv) >= 0.f && segment_length > length(dv) &&
        inside(cp, area, normal, count)) {
      *p2 = cp;
      hit_normal = normal;
      return 1;
    }
  }
  return 0;
}
int plane_xz(const Vec3 *p1, Vec3 *p2, const Vec3 *pos, float w, float d,
             int flag, Vec3 &normal) {
  const Vec3 n = {0, flag ? -1.f : 1.f, 0};
  Vec3 cp;
  if (cross_plane(*p1, sub(*p2, *p1), *pos, n, cp) && cp.x >= pos->x &&
      cp.x <= finite(pos->x + w) && cp.z >= pos->z &&
      cp.z <= finite(pos->z + d)) {
    *p2 = cp;
    normal = n;
    return 1;
  }
  return 0;
}
int plane_xy(const Vec3 *p1, Vec3 *p2, const Vec3 *pos, float w, float h,
             int flag, Vec3 &normal) {
  const Vec3 n = {0, 0, flag ? -1.f : 1.f};
  Vec3 cp;
  if (cross_plane(*p1, sub(*p2, *p1), *pos, n, cp) && cp.x >= pos->x &&
      cp.x <= finite(pos->x + w) && cp.y >= pos->y &&
      cp.y <= finite(pos->y + h)) {
    *p2 = cp;
    normal = n;
    return 1;
  }
  return 0;
}
int plane_yz(const Vec3 *p1, Vec3 *p2, const Vec3 *pos, float h, float d,
             int flag, Vec3 &normal) {
  const Vec3 n = {flag ? -1.f : 1.f, 0, 0};
  Vec3 cp;
  if (cross_plane(*p1, sub(*p2, *p1), *pos, n, cp) && cp.z >= pos->z &&
      cp.z <= finite(pos->z + d) && cp.y >= pos->y &&
      cp.y <= finite(pos->y + h)) {
    *p2 = cp;
    normal = n;
    return 1;
  }
  return 0;
}
int shape(const CollisionRecord *hp, const Vec3 *vec, const Vec3 *p1, Vec3 *p2,
          float room_height, Vec3 &normal) {
  int ret;
  int j;
  Vec3 area[8]{};
  Vec3 pos;
  float h; // not from DWARF

  ret = 0;

  h = hp->size.y ? hp->size.y : room_height;

  switch (hp->type) {
  case 0:
  case 1:
    if ((((hp->position.x > p1->x) && (hp->position.x > p2->x)) ||
         ((hp->position.z > p1->z) && (hp->position.z > p2->z))) ||
        (((hp->position.x + hp->size.x) < p1->x) &&
         ((hp->position.x + hp->size.x) < p2->x)) ||
        (((hp->position.z + hp->size.z) < p1->z) &&
         ((hp->position.z + hp->size.z) < p2->z))) {
      break;
    }

    if ((vec->y < 0) &&
        ((p1->y >= (hp->position.y + h)) && (p2->y < (hp->position.y + h)))) {
      pos.x = hp->position.x;
      pos.y = hp->position.y + h;
      pos.z = hp->position.z;

      if (plane_xz(p1, p2, &pos, hp->size.x, hp->size.z, 0, normal) != 0) {
        ret = 1;

        p2->y = hp->position.y + h;
      }
    }

    if ((vec->z > 0) &&
        ((p1->z <= hp->position.z) && (p2->z > hp->position.z))) {
      pos.x = hp->position.x;
      pos.y = hp->position.y;
      pos.z = hp->position.z;

      if (plane_xy(p1, p2, &pos, hp->size.x, h, 1, normal) != 0) {
        ret = 1;

        p2->z = hp->position.z;
      }
    }

    if ((vec->x > 0) &&
        ((p1->x <= hp->position.x) && (p2->x > hp->position.x))) {
      pos.x = hp->position.x;
      pos.y = hp->position.y;
      pos.z = hp->position.z;

      if (plane_yz(p1, p2, &pos, h, hp->size.z, 1, normal) != 0) {
        ret = 1;

        p2->x = hp->position.x;
      }
    }

    if ((vec->z < 0) && ((p1->z >= (hp->position.z + hp->size.z)) &&
                         (p2->z < (hp->position.z + hp->size.z)))) {
      pos.x = hp->position.x;
      pos.y = hp->position.y;
      pos.z = hp->position.z + hp->size.z;

      if (plane_xy(p1, p2, &pos, hp->size.x, h, 0, normal) != 0) {
        ret = 1;

        p2->z = hp->position.z + hp->size.z;
      }
    }

    if ((vec->x < 0) && ((p1->x >= (hp->position.x + hp->size.x)) &&
                         (p2->x < (hp->position.x + hp->size.x)))) {
      pos.x = hp->position.x + hp->size.x;
      pos.y = hp->position.y;
      pos.z = hp->position.z;

      if (plane_yz(p1, p2, &pos, h, hp->size.z, 0, normal) != 0) {
        ret = 1;

        p2->x = hp->position.x + hp->size.x;
      }
    }

    if ((vec->y > 0) &&
        ((p1->y <= hp->position.y) && (p2->y > hp->position.y))) {
      pos.x = hp->position.x;
      pos.y = hp->position.y;
      pos.z = hp->position.z;

      if (plane_xz(p1, p2, &pos, hp->size.x, hp->size.z, 1, normal) != 0) {
        ret = 1;

        p2->y = hp->position.y;
      }
    }

    break;
  case 7:
    if ((((hp->position.x <= p1->x) || (hp->position.x <= p2->x)) &&
         ((hp->position.z <= p1->z) || (hp->position.z <= p2->z))) &&
        (((hp->position.x + hp->size.x) >= p1->x) ||
         ((hp->position.x + hp->size.x) >= p2->x)) &&
        (((hp->position.z + hp->size.z) >= p1->z) ||
         ((hp->position.z + hp->size.z) >= p2->z))) {
      if ((vec->y > 0) && ((p1->y <= (hp->position.y + hp->size.y)) &&
                           (p2->y > (hp->position.y + hp->size.y)))) {
        pos.x = hp->position.x;
        pos.y = hp->position.y + hp->size.y;
        pos.z = hp->position.z;

        if (plane_xz(p1, p2, &pos, hp->size.x, hp->size.z, 1, normal) != 0) {
          ret = 1;

          p2->y = hp->position.y + hp->size.y;
        }
      }

      if ((vec->y < 0) &&
          ((p1->y >= hp->position.y) && (p2->y < hp->position.y))) {
        pos.x = hp->position.x;
        pos.y = hp->position.y;
        pos.z = hp->position.z;

        if (plane_xz(p1, p2, &pos, hp->size.x, hp->size.z, 0, normal) != 0) {
          ret = 1;

          p2->y = hp->position.y;
        }
      }

      if ((vec->z > 0) && ((p1->z <= hp->position.z) &&
                           (p2->z > hp->position.z) && (hp->size.y))) {
        pos.x = hp->position.x;
        pos.y = hp->position.y + hp->size.y;
        pos.z = hp->position.z;

        if (plane_xy(p1, p2, &pos, hp->size.x, -hp->size.y, 1, normal) != 0) {
          ret = 1;

          p2->z = hp->position.z;
        }
      }

      if ((vec->x > 0) && ((p1->x <= hp->position.x) &&
                           (p2->x > hp->position.x) && (hp->size.y))) {
        pos.x = hp->position.x;
        pos.y = hp->position.y + hp->size.y;
        pos.z = hp->position.z;

        if (plane_yz(p1, p2, &pos, -hp->size.y, hp->size.z, 1, normal) != 0) {
          ret = 1;

          p2->x = hp->position.x;
        }
      }

      if ((vec->z < 0) &&
          ((p1->z >= (hp->position.z + hp->size.z)) &&
           (p2->z < (hp->position.z + hp->size.z)) && (hp->size.y))) {
        pos.x = hp->position.x;
        pos.y = hp->position.y + hp->size.y;
        pos.z = hp->position.z + hp->size.z;

        if (plane_xy(p1, p2, &pos, hp->size.x, -hp->size.y, 0, normal) != 0) {
          ret = 1;

          p2->z = hp->position.z + hp->size.z;
        }
      }

      if ((vec->x < 0) &&
          ((p1->x <= (hp->position.x + hp->size.x)) &&
           (p2->x > (hp->position.x + hp->size.x)) && (hp->size.y))) {
        pos.x = hp->position.x + hp->size.x;
        pos.y = hp->position.y + hp->size.y;
        pos.z = hp->position.z;

        if (plane_yz(p1, p2, &pos, -hp->size.y, hp->size.z, 0, normal) != 0) {
          ret = 1;

          p2->x = hp->position.x + hp->size.x;
        }
      }
    }

    break;
  case 2:
  case 3:
    if (((((hp->position.x - hp->size.x) <= p1->x) ||
          ((hp->position.x - hp->size.x) <= p2->x)) &&
         (((hp->position.z - hp->size.x) <= p1->z) ||
          ((hp->position.z - hp->size.x) <= p2->z))) &&
        ((((hp->position.x + hp->size.x) >= p1->x) ||
          ((hp->position.x + hp->size.x) >= p2->x)) &&
         (((hp->position.z + hp->size.x) >= p1->z) ||
          ((hp->position.z + hp->size.x) >= p2->z)))) {
      area[0].x = hp->position.x + hp->size.x;
      area[0].y = hp->position.y;
      area[0].z = hp->position.z;

      area[1].x = hp->position.x + hp->size.x;
      area[1].y = hp->position.y + h;
      area[1].z = hp->position.z;

      area[2].y = hp->position.y + h;
      area[3].y = hp->position.y;

      for (j = 1; j <= 8; j++) {
        area[2].x = area[1].x;
        area[2].z = area[1].z;

        area[3].x = area[0].x;
        area[3].z = area[0].z;

        area[0].x = area[1].x =
            hp->position.x +
            (hp->size.x * source_cos(182.04445f * ((360 * j) / 8)));
        area[0].z = area[1].z =
            hp->position.z -
            (hp->size.x * source_sin(182.04445f * ((360 * j) / 8)));

        if (polygon(p1, p2, area, 4, normal) != 0) {
          ret = 1;
        }
      }

      if (vec->y < 0) {
        // Native compatibility: fill all top vertices before querying.
        for (j = 0; j < 8; j++) {
          area[j].x = hp->position.x +
                      (hp->size.x * source_cos(182.04445f * ((360 * j) / 8)));
          area[j].y = hp->position.y + h;
          area[j].z = hp->position.z -
                      (hp->size.x * source_sin(182.04445f * ((360 * j) / 8)));
        }
        if (polygon(p1, p2, area, 8, normal) != 0) {
          ret = 1;
          p2->y = area[0].y;
        }
      }
    }

    break;
  case 4:
  case 5:
    if ((hp->id == 0) || (hp->id == 1)) {
      if ((hp->position.x > p1->x) && (hp->position.x > p2->x)) {
        break;
      } else if (((hp->position.x + hp->size.x) < p1->x) &&
                 ((hp->position.x + hp->size.x) < p2->x)) {
        break;
      }
    } else if ((((hp->position.x + hp->size.x) > p1->x) &&
                ((hp->position.x + hp->size.x) > p2->x)) ||
               ((hp->position.x < p1->x) && (hp->position.x < p2->x))) {
      break;
    }

    if ((hp->id == 0) || (hp->id == 2)) {
      if ((hp->position.z > p1->z) && (hp->position.z > p2->z)) {
        break;
      } else if (((hp->position.z + hp->size.z) < p1->z) &&
                 ((hp->position.z + hp->size.z) < p2->z)) {
        break;
      }
    } else if ((((hp->position.z + hp->size.z) > p1->z) &&
                ((hp->position.z + hp->size.z) > p2->z)) ||
               ((hp->position.z < p1->z) && (hp->position.z < p2->z))) {
      break;
    }

    if ((hp->id == 0) || (hp->id == 3)) {
      if (vec->y < 0) {
        area[0].x = hp->position.x;
        area[0].y = hp->position.y + h;
        area[0].z = hp->position.z;

        area[1].x = hp->position.x;
        area[1].y = hp->position.y + h;
        area[1].z = hp->position.z + hp->size.z;

        area[2].x = hp->position.x + hp->size.x;
        area[2].y = hp->position.y + h;
        area[2].z = hp->position.z;

        if (polygon(p1, p2, area, 3, normal) != 0) {
          ret = 1;

          p2->y = area[0].y;
        }
      }

      area[0].x = hp->position.x;
      area[0].y = hp->position.y;
      area[0].z = hp->position.z;

      area[1].x = hp->position.x;
      area[1].y = hp->position.y;
      area[1].z = hp->position.z + hp->size.z;

      area[2].x = hp->position.x;
      area[2].y = hp->position.y + h;
      area[2].z = hp->position.z + hp->size.z;

      area[3].x = hp->position.x;
      area[3].y = hp->position.y + h;
      area[3].z = hp->position.z;

      if (polygon(p1, p2, area, 4, normal) != 0) {
        ret = 1;

        p2->x = area[0].x;
      }

      area[0].x = hp->position.x;
      area[0].y = hp->position.y;
      area[0].z = hp->position.z;

      area[1].x = hp->position.x;
      area[1].y = hp->position.y + h;
      area[1].z = hp->position.z;

      area[2].x = hp->position.x + hp->size.x;
      area[2].y = hp->position.y + h;
      area[2].z = hp->position.z;

      area[3].x = hp->position.x + hp->size.x;
      area[3].y = hp->position.y;
      area[3].z = hp->position.z;

      if (polygon(p1, p2, area, 4, normal) != 0) {
        ret = 1;

        p2->z = area[0].z;
      }

      area[0].x = hp->position.x + hp->size.x;
      area[0].y = hp->position.y;
      area[0].z = hp->position.z;

      area[1].x = hp->position.x + hp->size.x;
      area[1].y = hp->position.y + h;
      area[1].z = hp->position.z;

      area[2].x = hp->position.x;
      area[2].y = hp->position.y + h;
      area[2].z = hp->position.z + hp->size.z;

      area[3].x = hp->position.x;
      area[3].y = hp->position.y;
      area[3].z = hp->position.z + hp->size.z;

      if (polygon(p1, p2, area, 4, normal) != 0) {
        ret = 1;
      }
    } else {
      if (vec->y < 0) {
        area[0].x = hp->position.x;
        area[0].y = hp->position.y + h;
        area[0].z = hp->position.z;

        area[2].x = hp->position.x;
        area[2].y = hp->position.y + h;
        area[2].z = hp->position.z + hp->size.z;

        area[1].x = hp->position.x + hp->size.x;
        area[1].y = hp->position.y + h;
        area[1].z = hp->position.z;

        if (polygon(p1, p2, area, 3, normal) != 0) {
          ret = 1;

          p2->y = area[0].y;
        }
      }

      area[0].x = hp->position.x;
      area[0].y = hp->position.y;
      area[0].z = hp->position.z;

      area[3].x = hp->position.x;
      area[3].y = hp->position.y;
      area[3].z = hp->position.z + hp->size.z;

      area[2].x = hp->position.x;
      area[2].y = hp->position.y + h;
      area[2].z = hp->position.z + hp->size.z;

      area[1].x = hp->position.x;
      area[1].y = hp->position.y + h;
      area[1].z = hp->position.z;

      if (polygon(p1, p2, area, 4, normal) != 0) {
        ret = 1;

        p2->x = area[0].x;
      }

      area[0].x = hp->position.x;
      area[0].y = hp->position.y;
      area[0].z = hp->position.z;

      area[3].x = hp->position.x;
      area[3].y = hp->position.y + h;
      area[3].z = hp->position.z;

      area[2].x = hp->position.x + hp->size.x;
      area[2].y = hp->position.y + h;
      area[2].z = hp->position.z;

      area[1].x = hp->position.x + hp->size.x;
      area[1].y = hp->position.y;
      area[1].z = hp->position.z;

      if (polygon(p1, p2, area, 4, normal) != 0) {
        ret = 1;

        p2->z = area[0].z;
      }

      area[0].x = hp->position.x + hp->size.x;
      area[0].y = hp->position.y;
      area[0].z = hp->position.z;

      area[3].x = hp->position.x + hp->size.x;
      area[3].y = hp->position.y + h;
      area[3].z = hp->position.z;

      area[2].x = hp->position.x;
      area[2].y = hp->position.y + h;
      area[2].z = hp->position.z + hp->size.z;

      area[1].x = hp->position.x;
      area[1].y = hp->position.y;
      area[1].z = hp->position.z + hp->size.z;

      if (polygon(p1, p2, area, 4, normal) != 0) {
        ret = 1;
      }
    }

    break;
  case 6:
    if ((((hp->position.x <= p1->x) || (hp->position.x <= p2->x)) &&
         ((hp->position.z <= p1->z) || (hp->position.z <= p2->z))) &&
        ((((hp->position.x + hp->size.x) >= p1->x) ||
          ((hp->position.x + hp->size.x) >= p2->x)) &&
         (((hp->position.z + hp->size.z) >= p1->z) ||
          ((hp->position.z + hp->size.z) >= p2->z)))) {
      area[0].x = hp->position.x;
      area[0].y = hp->position.y;
      area[0].z = hp->position.z;

      area[1].x = hp->position.x;
      area[1].y = hp->position.y;
      area[1].z = hp->position.z + hp->size.z;

      area[2].x = hp->position.x + hp->size.x;
      area[2].y = hp->position.y;
      area[2].z = hp->position.z + hp->size.z;

      area[3].x = hp->position.x + hp->size.x;
      area[3].y = hp->position.y;
      area[3].z = hp->position.z;

      switch (hp->id) {
      case 0:
        area[2].y = area[3].y = hp->position.y + h;
        break;
      case 1:
        area[0].y = area[3].y = hp->position.y + h;
        break;
      case 2:
        area[1].y = area[2].y = hp->position.y + h;
        break;
      case 3:
        area[0].y = area[1].y = hp->position.y + h;
        break;
      }

      if (polygon(p1, p2, area, 4, normal) != 0) {
        ret = 1;
      }
    }
  }

  return ret;
}
} // namespace
LineCollisionResult
query_collision_line(const RoomCollision &room,
                     const std::vector<CollisionRecord> &dynamic, Vec3 from,
                     Vec3 to) {
  vector(from);
  vector(to);
  const auto direction = sub(to, from);
  LineCollisionResult result;
  result.position = to;
  auto query = [&](const std::vector<CollisionRecord> &records, size_t base) {
    for (size_t i = 0; i < records.size(); ++i) {
      const auto &record = records[i];
      if (!(record.flags & 1) || record.type > 7)
        continue;
      vector(record.position);
      vector(record.size);
      finite(record.position.x + record.size.x);
      finite(record.position.z + record.size.z);
      finite(record.position.y +
             (record.size.y ? record.size.y : finite(room.height)));
      if (record.type == 2 || record.type == 3) {
        finite(record.position.x - record.size.x);
        finite(record.position.z - record.size.x);
        finite(record.position.z + record.size.x);
      }
      if (shape(&record, &direction, &from, &result.position, room.height,
                result.normal))
        result.wall = base + i;
    }
  };
  query(room.walls, 0);
  query(dynamic, room.walls.size());
  return result;
}
} // namespace ashford
