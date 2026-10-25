#ifndef RECVX_PICTURE_SHADER_H
#define RECVX_PICTURE_SHADER_H

static const char* present_vs =
    "#version 330 core\n"
    "uniform vec4 u_rect;\n"
    "out vec2 v_uv;\n"
    "void main(){\n"
    "  vec2 c = vec2((gl_VertexID & 1) != 0 ? 1.0 : 0.0, (gl_VertexID & 2) != 0 ? 1.0 : 0.0);\n"
    "  v_uv = vec2(mix(u_rect.x, u_rect.z, c.x), mix(u_rect.y, u_rect.w, c.y));\n"
    "  gl_Position = vec4(c.x * 2.0 - 1.0, 1.0 - c.y * 2.0, 0.0, 1.0);\n"
    "}\n";
/* Presentation only: FXAA works in physical source texels, independently of the
   internal-resolution scale. Clamp every tap to the cropped display rectangle.
   The neutral branch retains the original sample and tone-calibration path. */
static const char* present_fs =
    "#version 330 core\n"
    "in vec2 v_uv; uniform sampler2D u_tex; uniform vec4 u_rect, u_pic;\n"
    "uniform float u_saturation; uniform int u_antialias; out vec4 o;\n"
    "vec3 source(vec2 uv){\n"
    "  vec2 h = 0.5 / vec2(textureSize(u_tex, 0));\n"
    "  return clamp(texture(u_tex, clamp(uv, max(u_rect.xy, vec2(0.0)) + h, min(u_rect.zw, vec2(1.0)) - h)).rgb, 0.0, 1.0);\n"
    "}\n"
    "float luma(vec3 c){ return dot(c, vec3(0.2126, 0.7152, 0.0722)); }\n"
    /* Classic fast approximate AA: estimate the edge from four diagonal taps,
       search along that direction, and reject a wider filter outside its luma range. */
    "vec3 fxaa(vec3 center){\n"
    "  vec2 px = 1.0 / vec2(textureSize(u_tex, 0));\n"
    "  float nw = luma(source(v_uv + vec2(-1.0,-1.0) * px));\n"
    "  float ne = luma(source(v_uv + vec2( 1.0,-1.0) * px));\n"
    "  float sw = luma(source(v_uv + vec2(-1.0, 1.0) * px));\n"
    "  float se = luma(source(v_uv + vec2( 1.0, 1.0) * px));\n"
    "  float m = luma(center);\n"
    "  float lo = min(m, min(min(nw, ne), min(sw, se)));\n"
    "  float hi = max(m, max(max(nw, ne), max(sw, se)));\n"
    "  if (hi - lo < max(0.03125, hi * 0.125)) return center;\n"
    "  vec2 dir = vec2(-((nw + ne) - (sw + se)), (nw + sw) - (ne + se));\n"
    "  float reduce = max((nw + ne + sw + se) * 0.03125, 0.0078125);\n"
    "  dir = clamp(dir / (min(abs(dir.x), abs(dir.y)) + reduce), vec2(-8.0), vec2(8.0)) * px;\n"
    "  vec3 a = 0.5 * (source(v_uv - dir / 6.0) + source(v_uv + dir / 6.0));\n"
    "  vec3 b = a * 0.5 + 0.25 * (source(v_uv - dir * 0.5) + source(v_uv + dir * 0.5));\n"
    "  float lb = luma(b);\n"
    "  return (lb < lo || lb > hi) ? a : b;\n"
    "}\n"
    "void main(){\n"
    "  vec3 c = clamp(texture(u_tex, v_uv).rgb, 0.0, 1.0);\n"
    "  if (u_antialias == 1) c = fxaa(source(v_uv));\n"
    "  if (u_pic.w != 0.0) c = clamp((pow(c, vec3(u_pic.z)) - 0.5) * u_pic.y + 0.5 + u_pic.x, 0.0, 1.0);\n"
    "  if (u_saturation != 1.0) c = clamp(mix(vec3(luma(c)), c, u_saturation), 0.0, 1.0);\n"
    "  o = vec4(c, 1.0);\n"
    "}\n";

#endif
