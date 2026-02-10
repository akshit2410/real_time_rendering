#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 UV;
    mat3 TBN;
} fs;

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform bool useNormalMap;
uniform float normalStrength;
uniform float ambientStrength;

void main()
{
    vec3 albedo = texture(diffuseMap, fs.UV).rgb;

    vec3 N = normalize(fs.TBN[2]);
    if (useNormalMap)
    {
        vec3 n = texture(normalMap, fs.UV).rgb;
        n = normalize(n * 2.0 - 1.0);
        n.xy *= normalStrength;
        N = normalize(fs.TBN * n);
    }

    vec3 L = lightPos - fs.FragPos;
    float dist = length(L);
    L = normalize(L);

    float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    float diff = max(dot(N, L), 0.0);

    vec3 V = normalize(viewPos - fs.FragPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.2;

    vec3 ambient = ambientStrength * albedo;
    vec3 color = ambient + attenuation * (diff * albedo + vec3(spec));

    FragColor = vec4(color, 1.0);
}
