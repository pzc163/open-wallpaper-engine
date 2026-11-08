#pragma once
#include <string_view>

namespace wallpaper {
// Engine-provided LightingV1 implementation. Positions and radiance are in world
// space; the material shader supplies its normal map, roughness and metallic mask.
inline constexpr std::string_view kSceneLightingV1 = R"GLSL(
uniform vec4 g_LightsPosition[4];
uniform vec4 g_LightsColorPremultiplied[3];
vec3 PerformLighting_V1(vec3 world_pos, vec3 albedo, vec3 normal, vec3 view_vector,
                       vec3 specular_tint, vec3 f0, float roughness, float metallic) {
    vec3 result = vec3(0.0);
    vec3 N = normalize(normal);
    vec3 V = normalize(view_vector);
    float nv = max(dot(N, V), 0.0001);
    float a = max(roughness * roughness, 0.0025);
    float a2 = a * a;
    for (int i = 0; i < 3; ++i) {
        vec3 delta = g_LightsPosition[i].xyz - world_pos;
        float distance2 = max(dot(delta, delta), 0.0001);
        vec3 L = delta * inversesqrt(distance2);
        vec3 H = normalize(L + V);
        float nl = max(dot(N, L), 0.0);
        float nh = max(dot(N, H), 0.0);
        float vh = max(dot(V, H), 0.0);
        vec3 F = f0 + (vec3(1.0) - f0) * pow(1.0 - vh, 5.0);
        float denominator = nh * nh * (a2 - 1.0) + 1.0;
        float D = a2 / max(3.14159265 * denominator * denominator, 0.00001);
        float k = (roughness + 1.0) * (roughness + 1.0) * 0.125;
        float G = (nv / (nv * (1.0 - k) + k)) * (nl / (nl * (1.0 - k) + k));
        vec3 specular = D * G * F * specular_tint / max(4.0 * nv * nl, 0.0001);
        vec3 diffuse = (vec3(1.0) - F) * (1.0 - metallic) * albedo / 3.14159265;
        result += (diffuse + specular) * g_LightsColorPremultiplied[i].rgb * (nl / distance2);
    }
    return result;
}
vec3 PerformLighting_V1(vec3 world_pos, vec3 albedo, vec3 normal, vec3 view_vector,
                       vec3 specular_tint, vec3 f0, float roughness, float metallic, float ao) {
    return PerformLighting_V1(world_pos, albedo, normal, view_vector,
                              specular_tint, f0, roughness, metallic) * ao;
}
)GLSL";
}
