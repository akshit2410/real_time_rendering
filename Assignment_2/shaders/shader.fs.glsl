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
    vec3 N = normalize(Normal);
    vec3 V = normalize(cameraPos - WorldPos);

    // ---------------- Reflection ----------------
    vec3 R = reflect(-V, N);
    vec3 reflected = texture(skybox, R).rgb;

    // ---------------- Refraction + Dispersion ----------------
    float disp = clamp(dispersion, 0.0, 0.01);

    vec3 refrR = refract(-V, N, 1.0 / (ior - disp));
    vec3 refrG = refract(-V, N, 1.0 / ior);
    vec3 refrB = refract(-V, N, 1.0 / (ior + disp));

    vec3 refracted;
    refracted.r = texture(skybox, refrR).r;
    refracted.g = texture(skybox, refrG).g;
    refracted.b = texture(skybox, refrB).b;

    // ---------------- Fresnel (Schlick-like) ----------------
    float cosTheta = clamp(dot(N, V), 0.0, 1.0);
    float fresnel = pow(1.0 - cosTheta, fresnelPower);

    vec3 glassTint = vec3(0.96, 0.98, 1.0);
    float thickness = pow(1.0 - cosTheta, 1.5);
    vec3 transmission = refracted * glassTint * (1.0 - thickness * 0.4);

    vec3 color = mix(transmission, reflected, fresnel);
    color *= exposure;

    FragColor = vec4(color, 1.0);
}
