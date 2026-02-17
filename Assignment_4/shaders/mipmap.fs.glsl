#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture1;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform int useTexture;

void main()
{
    vec3 baseColor;

    if(useTexture == 1)
        baseColor = texture(texture1, TexCoord).rgb;
    else
        baseColor = vec3(0.8,0.8,0.85);

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    // Stronger ambient
    vec3 ambient = 0.7 * baseColor;

    // Stronger diffuse
    vec3 diffuse = 1.3 * diff * baseColor;

    // Specular (Blinn-Phong for nicer highlight)
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 64.0);
    vec3 specular = vec3(1.0) * spec * 1.2;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
