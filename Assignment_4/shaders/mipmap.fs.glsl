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

    vec3 ambient = 0.3 * baseColor;
    vec3 diffuse = diff * baseColor;

    FragColor = vec4(ambient + diffuse, 1.0);
}
