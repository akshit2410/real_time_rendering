#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aTangent;

out VS_OUT {
    vec3 FragPos;
    vec2 UV;
    mat3 TBN;
} vs;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec3 T = normalize(mat3(model) * aTangent);
    vec3 N = normalize(mat3(model) * aNormal);

    // Re-orthogonalize T
    T = normalize(T - dot(T, N) * N);

    vec3 B = cross(N, T);

    vs.TBN = mat3(T, B, N);
    vs.FragPos = vec3(model * vec4(aPos, 1.0));
    vs.UV = aUV;

    gl_Position = projection * view * vec4(vs.FragPos, 1.0);
}
