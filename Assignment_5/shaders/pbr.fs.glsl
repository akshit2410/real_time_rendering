#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;

uniform vec3 camPos;
uniform vec3 lightPos;
uniform vec3 lightColor;

uniform vec3 albedo;
uniform float metallic;
uniform float roughness;

uniform int brdfMode;     // 0: Phong, 1: Cook-Torrance
uniform int samplingMode; // 0: Uniform, 1: GGX Importance
uniform int frameCount;
uniform bool useMonteCarlo;
uniform bool splitScreen;
uniform float screenWidth;

const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
// RANDOMNESS (Hash-based PCG)
// ----------------------------------------------------------------------------
float hash(uint n) {
    n = (n << 13U) ^ n;
    n = n * (n * n * 15731U + 789221U) + 1376312589U;
    return float(n & 0x7fffffffU) / float(0x7fffffff);
}

vec2 get_random_2d() {
    uint n = uint(gl_FragCoord.x) * 1973U + uint(gl_FragCoord.y) * 9277U + uint(frameCount) * 26699U;
    return vec2(hash(n), hash(n + 12345U));
}

// ----------------------------------------------------------------------------
// PBR COMPONENTS (MODULAR)
// ----------------------------------------------------------------------------

float D_GGX(float NdotH, float a) {
    float a2 = a*a;
    float denom = (NdotH*NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float G_SchlickGGX(float NdotV, float k) {
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----------------------------------------------------------------------------
// BRDF EVALUATION
// ----------------------------------------------------------------------------

vec3 evalBRDF(vec3 N, vec3 V, vec3 L, vec3 F0) {
    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.001);
    float NdotL = max(dot(N, L), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    if (brdfMode == 0) { // Normalized Blinn-Phong
        float shininess = (2.0 / (roughness * roughness)) - 2.0;
        float spec = ((shininess + 2.0) / (8.0 * PI)) * pow(NdotH, shininess);
        return albedo / PI + vec3(spec);
    }

    // Cook-Torrance
    float D = D_GGX(NdotH, roughness * roughness);
    float G = G_Smith(NdotV, NdotL, roughness);
    vec3  F = F_Schlick(HdotV, F0);

    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    return kD * albedo / PI + specular;
}

// ----------------------------------------------------------------------------
// SAMPLING & PDF
// ----------------------------------------------------------------------------

vec3 sampleCosineHemisphere(vec2 xi, vec3 N) {
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt(1.0 - xi.y);
    float sinTheta = sqrt(xi.y);

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

vec3 sampleGGX(vec2 xi, vec3 N, float a) {
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a*a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float pdfGGX(float NdotH, float VdotH, float a) {
    float D = D_GGX(NdotH, a);
    return (D * NdotH) / (4.0 * VdotH + 0.0001);
}

// ----------------------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------------------

void main() {
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    vec3 color = vec3(0.0);

    int currentSamplingMode = samplingMode;
    if (splitScreen) {
        if (gl_FragCoord.x < screenWidth * 0.5) 
            currentSamplingMode = 0; // Uniform
        else 
            currentSamplingMode = 1; // GGX
    }

    if (splitScreen && abs(gl_FragCoord.x - screenWidth * 0.5) < 1.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    if (useMonteCarlo) {
        vec2 xi = get_random_2d();
        vec3 L;
        float pdf = 1.0;

        if (currentSamplingMode == 0) { // Cosine Weighted Hemisphere
            L = sampleCosineHemisphere(xi, N);
            pdf = max(dot(N, L), 0.0) / PI;
        } else { // GGX Importance Sampling (Half-Vector)
            float a = roughness * roughness;
            vec3 H = sampleGGX(xi, N, a);
            L = reflect(-V, H);
            
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);
            pdf = pdfGGX(NdotH, VdotH, a);
        }

        pdf = max(pdf, 1e-4); // PDF Safety

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            vec3 brdf = evalBRDF(N, V, L, F0);
            
            // LIGHT: Spherical Source Integration
            // To show variance, we treat the light as a small area source
            vec3 lightDir = normalize(lightPos - WorldPos);
            float dist = length(lightPos - WorldPos);
            
            float cosLight = max(dot(L, lightDir), 0.0);
            // Rigorous spherical light approximation
            float softLight = pow(cosLight, 300.0); 
            vec3 radiance = lightColor * (60.0 / (dist * dist)) * softLight;

            color = (brdf * radiance * NdotL) / pdf;
        }

    } else {
        // ANALYTICAL MODE (Standard Direct Lighting)
        vec3 L = normalize(lightPos - WorldPos);
        float dist = length(lightPos - WorldPos);
        vec3 radiance = lightColor * (30.0 / (dist * dist));

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            color = evalBRDF(N, V, L, F0) * radiance * NdotL;
        }
    }

    // HDR & Simple Bloom-like Ambient
    float rim = pow(1.0 - max(dot(N, V), 0.0), 5.0) * 0.05;
    color += rim * albedo;
    color += 0.01 * albedo; // Base ambient

    FragColor = vec4(color, 1.0);
}
