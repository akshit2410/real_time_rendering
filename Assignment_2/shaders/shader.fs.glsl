#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;

uniform samplerCube skybox;
uniform vec3 cameraPos;

uniform float ior;
uniform float fresnelPower;
uniform float dispersion;
uniform float exposure;

void main()
{
    float clampedDispersion = max(dispersion, 0.0001) * 5.0;
    float clampedExposure = max(exposure, 0.0001);
    vec3 N = normalize(Normal);
    vec3 V = normalize(cameraPos - WorldPos);

    // ---------- Reflection ----------
    vec3 R = reflect(-V, N);
    vec3 reflected = texture(skybox, R).rgb;

    // ---------- Chromatic Dispersion ----------
    vec3 refrR = refract(-V, N, 1.0 / (ior - clampedDispersion));
    vec3 refrG = refract(-V, N, 1.0 / ior);
    vec3 refrB = refract(-V, N, 1.0 / (ior + clampedDispersion));

    vec3 refracted;
    refracted.r = texture(skybox, refrR).r;
    refracted.g = texture(skybox, refrG).g;
    refracted.b = texture(skybox, refrB).b;

    // ---------- Fresnel ----------
    float cosTheta = clamp(dot(V, N), 0.0, 1.0);
    float F = pow(1.0 - cosTheta, fresnelPower);

    vec3 color = mix(refracted, reflected, F);

    // ---------- Tone Mapping ----------
    color = pow(color * clampedExposure, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
