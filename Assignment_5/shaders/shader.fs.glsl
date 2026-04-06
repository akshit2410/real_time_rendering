#version 330 core

in vec3 vPos;
in vec3 vNormal;
out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 baseColor;

uniform int shadingModel;
uniform float shininess;
uniform float roughness;
uniform float toonThreshold1; // e.g. 0.9
uniform float toonThreshold2; // e.g. 0.6

uniform float toonLevel1;     // e.g. 1.0
uniform float toonLevel2;     // e.g. 0.4
uniform float toonLevel3;     // e.g. 0.2

const float PI = 3.14159265;

// Blinn–Phong
vec3 blinnPhong(vec3 N, vec3 L, vec3 V)
{
    vec3 H = normalize(normalize(L) + normalize(V));

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), shininess);

    return baseColor * diff + vec3(1.0) * spec;
}

// Cook–Torrance (simplified microfacet)

vec3 cookTorrance(vec3 N, vec3 L, vec3 V)
{
    vec3 H = normalize(normalize(L) + normalize(V));

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * denom * denom);

    vec3 F0 = vec3(0.04);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    vec3 spec = (D * F) / max(4.0 * NdotL * NdotV, 0.001);
    vec3 diff = baseColor / PI;

    return (diff + spec) * NdotL;
}


// Toon shading

vec3 toon(vec3 N, vec3 L)
{
    float i = max(dot(N, L), 0.0);

    if (i > toonThreshold1)
        i = toonLevel1;
    else if (i > toonThreshold2)
        i = toonLevel2;
    else
        i = toonLevel3;

    return baseColor * i;
}


void main()
{
    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPos - vPos);
    vec3 V = normalize(viewPos - vPos);

    vec3 color;

    if (shadingModel == 0)
        color = blinnPhong(N, L, V);
    else if (shadingModel == 1)
        color = cookTorrance(N, L, V) * vec3(3);
    else
        color = toon(N, L);

    FragColor = vec4(color, 1.0);
}
