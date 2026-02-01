#version 330 core
out vec4 FragColor;
in vec3 TexCoords;

uniform samplerCube skybox;
uniform float skyboxExposure;

void main()
{
    vec3 color = texture(skybox, TexCoords).rgb;
    color *= skyboxExposure;      
    FragColor = vec4(color, 1.0);
}
