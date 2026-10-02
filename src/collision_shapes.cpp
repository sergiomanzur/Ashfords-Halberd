// Adapted from AshfordFamily/recvx-decomp hitchk.c (MIT),
// bhCheckInnerP4 and bhCheckInnerTriangle. Native values replace SDK
// structures.
#include "collision_shapes.hpp"
#include <cmath>
namespace ashford {
namespace {
struct Point2 {
  float x, y;
};
struct Shape {
  unsigned char id;
  float px, py, pz, w, h, d;
};
static int inside_quad(Point2 *pos, Point2 *p0, Point2 *p1, Point2 *p2,
                       Point2 *p3) {
  float min, max;
  float swp;
  float nn;
  float x0, x1;
  float y0, y1, y2, y3;

  if ((pos->y < p0->y) || (pos->y >= p3->y)) {
    return 0;
  }

  nn = p1->x - p0->x;

  x0 = x1 = 0;

  y0 = p1->y - p0->y;
  y1 = p2->y - p0->y;
  y2 = p3->y - p1->y;
  y3 = p3->y - p2->y;

  if (nn > 0) {
    if (y1) {
      x0 = (p2->x - p0->x) / y1;
    }

    if (y0) {
      x1 = nn / y0;
    }
  } else {
    if (y0) {
      x0 = nn / y0;
    }

    if (y1) {
      x1 = (p2->x - p0->x) / y1;
    }
  }

  if (pos->y < p1->y) {
    min = p0->x + (x0 * (pos->y - p0->y));
    max = p0->x + (x1 * (pos->y - p0->y));

    if (min > max) {
      swp = min;

      min = max;

      max = swp;
    }

    if ((min > pos->x) || (max <= pos->x)) {
      return 0;
    }
  } else if (pos->y < p2->y) {
    if (nn > 0) {
      x1 = (y2) ? (p3->x - p1->x) / y2 : 0;

      min = p0->x + (x0 * (pos->y - p0->y));
      max = p1->x + (x1 * (pos->y - p1->y));

      if (min > max) {
        swp = min;

        min = max;

        max = swp;
      }

      if ((min > pos->x) || (max <= pos->x)) {
        return 0;
      }
    } else {
      x0 = (y2) ? (p3->x - p1->x) / y2 : 0;

      min = p1->x + (x0 * (pos->y - p1->y));
      max = p0->x + (x1 * (pos->y - p0->y));

      if (min > max) {
        swp = min;

        min = max;

        max = swp;
      }

      if ((min > pos->x) || (max <= pos->x)) {
        return 0;
      }
    }
  } else {
    if (nn > 0) {
      x0 = (y2) ? (p3->x - p1->x) / y2 : 0;
      x1 = (y3) ? (p3->x - p2->x) / y3 : 0;

      min = p2->x + (x1 * (pos->y - p2->y));
      max = p1->x + (x0 * (pos->y - p1->y));

      if (min > max) {
        swp = min;

        min = max;

        max = swp;
      }

      if ((min > pos->x) || (max <= pos->x)) {
        return 0;
      }
    } else {
      x0 = (y2) ? (p3->x - p1->x) / y2 : 0;
      x1 = (y3) ? (p3->x - p2->x) / y3 : 0;

      min = p1->x + (x0 * (pos->y - p1->y));
      max = p2->x + (x1 * (pos->y - p2->y));

      if (min > max) {
        swp = min;

        min = max;

        max = swp;
      }

      if ((min > pos->x) || (max <= pos->x)) {
        return 0;
      }
    }
  }

  return 1;
}
static int inside_triangle(Shape *hp, Vec3 *pos, float ar, float ah,
                           float room_height) {
  Point2 ps, pa, pb, pc, pd;
  float px, pz;
  float h, w, d;    // w and d are not from DWARF
  float minx, maxx; // not from DWARF
  float minz, maxz; // not from DWARF
  float abx, abz;   // not from DWARF

  if (hp->h) {
    h = hp->h;
  } else {
    h = room_height;
  }

  if ((hp->py > (pos->y + ah)) || ((hp->py + h) < pos->y)) {
    return 0;
  }

  w = hp->w;

  if (w > 0) {
    minx = hp->px;
    maxx = ar + (minx + w);
  } else {
    maxx = hp->px;
    minx = (maxx + w) - ar;
  }

  d = hp->d;

  if (d > 0) {
    minz = hp->pz;
    maxz = ar + (minz + d);
  } else {
    maxz = hp->pz;
    minz = (maxz + d) - ar;
  }

  if (((minx > pos->x) || (maxx <= pos->x)) ||
      ((minz > pos->z) || (maxz <= pos->z))) {
    return 0;
  }

  if (!ar) {
    abx = fabsf(d / (w / (pos->x - hp->px)));
    abz = fabsf((hp->pz + hp->d) - pos->z);

    if (abz <= abx) {
      return 0;
    }
  } else {
    px = hp->px;
    pz = hp->pz;

    ps.x = pos->x;
    ps.y = pos->z;

    switch (hp->id) {
    case 0:
      pa.x = px + hp->w;
      pa.y = pz;

      pb.x = pa.x + ar;
      pb.y = pa.y + ar;

      pc.x = px;
      pc.y = pz + hp->d;

      pd.x = pc.x + ar;
      pd.y = pc.y + ar;

      if (inside_quad(&ps, &pa, &pb, &pc, &pd) != 0) {
        return 1;
      }

      px += ar;
      pz += ar;

      if ((px > pos->x) || (pz > pos->z)) {
        return 0;
      }

      break;
    case 1:
      pa.x = px + ar;
      pa.y = (pz + hp->d) - ar;

      pb.x = pa.x - ar;
      pb.y = pa.y + ar;

      pc.x = ar + (px + hp->w);
      pc.y = pz - ar;

      pd.x = pc.x - ar;
      pd.y = pc.y + ar;

      if (inside_quad(&ps, &pa, &pb, &pc, &pd) != 0) {
        return 1;
      }

      px += ar;
      pz -= ar;

      if ((px > pos->x) || (pz <= pos->z)) {
        return 0;
      }

      break;
    case 2:
      pa.x = px + hp->w;
      pa.y = pz;

      pb.x = pa.x - ar;
      pb.y = pa.y + ar;

      pc.x = px;
      pc.y = pz + hp->d;

      pd.x = pc.x - ar;
      pd.y = pc.y + ar;

      if (inside_quad(&ps, &pa, &pb, &pc, &pd) != 0) {
        return 1;
      }

      px -= ar;
      pz += ar;

      if ((px <= pos->x) || (pz > pos->z)) {
        return 0;
      }

      break;
    case 3:
      pa.x = px - ar;
      pa.y = (pz + hp->d) - ar;

      pb.x = pa.x + ar;
      pb.y = pa.y + ar;

      pc.x = (px + hp->w) - ar;
      pc.y = pz - ar;

      pd.x = pc.x + ar;
      pd.y = pc.y + ar;

      if (inside_quad(&ps, &pa, &pb, &pc, &pd) != 0) {
        return 1;
      }

      px -= ar;
      pz -= ar;

      if ((px <= pos->x) || (pz <= pos->z)) {
        return 0;
      }

      break;
    }

    abx = fabsf(d / (w / (pos->x - px)));
    abz = fabsf((pz + d) - pos->z);

    if (abz <= abx) {
      return 0;
    }
  }

  return 1;
}
} // namespace
bool triangle_wall_inside(const CollisionRecord &r, Vec3 point, float radius,
                          float height, float room_height) {
  Shape shape{r.id,     r.position.x, r.position.y, r.position.z,
              r.size.x, r.size.y,     r.size.z};
  return inside_triangle(&shape, &point, radius, height, room_height) != 0;
}
} // namespace ashford
