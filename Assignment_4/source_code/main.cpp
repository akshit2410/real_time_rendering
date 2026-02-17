// =======================================
// MIPMAPPING DEMO – Procedural Sphere
// =======================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

int SCR_WIDTH = 1280;
int SCR_HEIGHT = 720;

// ================= CAMERA =================
glm::vec3 cameraPos(0, 3, 12);
glm::vec3 cameraFront(0, 0, -1);
glm::vec3 cameraUp(0, 1, 0);

// ================= SHADER LOADER (UNCHANGED) =================
std::string loadShader(const char* path)
{
    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

unsigned int compile(unsigned int type, const char* src)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    return shader;
}

unsigned int makeProgram(const char* vs, const char* fs)
{
    unsigned int program = glCreateProgram();
    unsigned int v = compile(GL_VERTEX_SHADER, vs);
    unsigned int f = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
    return program;
}

// ================= FLOOR =================
float floorVertices[] = {
    -100,0,-100, 0,1,0, 0,0,
     100,0,-100, 0,1,0, 400,0,
     100,0, 100, 0,1,0, 400,400,
    -100,0,-100, 0,1,0, 0,0,
     100,0, 100, 0,1,0, 400,400,
    -100,0, 100, 0,1,0, 0,400
};

// ================= MIP FILTER =================
enum FilterMode { NO_MIPMAP, NEAREST, BILINEAR, TRILINEAR };
int currentMode = TRILINEAR;

void applyFilter(unsigned int tex)
{
    glBindTexture(GL_TEXTURE_2D, tex);

    switch(currentMode)
    {
        case NO_MIPMAP:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            break;
        case NEAREST:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            break;
        case BILINEAR:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
            break;
        case TRILINEAR:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            break;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

// ================= LOAD TEXTURE =================
unsigned int loadTexture(const char* path)
{
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    int w,h,c;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &c, 0);

    if(!data)
    {
        std::cout << "Texture load failed\n";
        return 0;
    }

    GLenum format = (c == 4) ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0,
                 format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    return tex;
}

// ================= UV SPHERE GENERATOR =================
void createSphere(std::vector<float>& vertices, int sectors = 64, int stacks = 64)
{
    float radius = 1.0f;

    for(int i = 0; i <= stacks; ++i)
    {
        float stackAngle = glm::pi<float>()/2 - i * glm::pi<float>()/stacks;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for(int j = 0; j <= sectors; ++j)
        {
            float sectorAngle = j * 2 * glm::pi<float>() / sectors;

            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            float u = (float)j / sectors;
            float v = (float)i / stacks;

            vertices.insert(vertices.end(), {
                x, z, y,
                x, z, y,
                u, v
            });
        }
    }

    std::vector<float> finalVerts;
    int k1, k2;
    for(int i = 0; i < stacks; ++i)
    {
        k1 = i * (sectors + 1);
        k2 = k1 + sectors + 1;

        for(int j = 0; j < sectors; ++j, ++k1, ++k2)
        {
            for(int t = 0; t < 6; t++)
            {
                int index;
                if(t==0) index=k1;
                if(t==1) index=k2;
                if(t==2) index=k1+1;
                if(t==3) index=k1+1;
                if(t==4) index=k2;
                if(t==5) index=k2+1;

                finalVerts.insert(finalVerts.end(),
                    vertices.begin() + index*8,
                    vertices.begin() + index*8 + 8);
            }
        }
    }

    vertices = finalVerts;
}

// ================= MAIN =================
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_CORE_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT,
        "Mipmapping Demo",nullptr,nullptr);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);

    // IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window,true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Create sphere
    std::vector<float> sphereVerts;
    createSphere(sphereVerts);

    unsigned int sphereVAO, sphereVBO;
    glGenVertexArrays(1,&sphereVAO);
    glGenBuffers(1,&sphereVBO);

    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER,sphereVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 sphereVerts.size()*sizeof(float),
                 sphereVerts.data(),
                 GL_STATIC_DRAW);

    int stride = 8*sizeof(float);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);

    int sphereCount = sphereVerts.size()/8;

    // Floor
    unsigned int floorVAO,floorVBO;
    glGenVertexArrays(1,&floorVAO);
    glGenBuffers(1,&floorVBO);

    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER,floorVBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(floorVertices),
                 floorVertices,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int texture = loadTexture("resources/textures/checker.png");

    std::string vs = loadShader("shaders/mipmap.vs.glsl");
    std::string fs = loadShader("shaders/mipmap.fs.glsl");
    unsigned int shader = makeProgram(vs.c_str(), fs.c_str());

    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader,"texture1"),0);

    // LOOP
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        applyFilter(texture);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float t = glfwGetTime();

        glm::mat4 view = glm::lookAt(cameraPos,
                                     glm::vec3(0),
                                     glm::vec3(0,1,0));

        glm::mat4 proj = glm::perspective(glm::radians(45.f),
                                          (float)SCR_WIDTH/SCR_HEIGHT,
                                          0.1f,100.f);

        glUseProgram(shader);
        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
        glUniform3f(glGetUniformLocation(shader,"lightPos"),5,10,5);
        glUniform3fv(glGetUniformLocation(shader,"viewPos"),1,&cameraPos[0]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,texture);

        // Floor
        glm::mat4 model(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);
        glBindVertexArray(floorVAO);
        glDrawArrays(GL_TRIANGLES,0,6);

        // Sphere
        glm::mat4 sphere(1.0f);
        sphere = glm::translate(sphere, glm::vec3(0,2,0));
        sphere = glm::rotate(sphere, t, glm::vec3(0,1,0));
        sphere = glm::scale(sphere, glm::vec3(1.5f));

        glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&sphere[0][0]);
        glBindVertexArray(sphereVAO);
        glDrawArrays(GL_TRIANGLES,0,sphereCount);

        // GUI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Mipmapping Controls");
        ImGui::RadioButton("No Mipmap", &currentMode, NO_MIPMAP);
        ImGui::RadioButton("Nearest", &currentMode, NEAREST);
        ImGui::RadioButton("Bilinear", &currentMode, BILINEAR);
        ImGui::RadioButton("Trilinear", &currentMode, TRILINEAR);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    glfwTerminate();
}
