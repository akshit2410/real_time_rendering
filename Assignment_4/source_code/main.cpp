#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

int SCR_WIDTH = 1280;
int SCR_HEIGHT = 720;

// =================================================
// CAMERA
// =================================================
glm::vec3 cameraPos(0.0f, 5.0f, 25.0f);
glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;
float cameraSpeed = 12.0f;

void processInput(GLFWwindow* window)
{
    float velocity = cameraSpeed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += velocity * cameraFront;

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= velocity * cameraFront;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * velocity;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * velocity;
}

// =================================================
// SHADER LOADER
// =================================================
std::string loadShader(const char* path)
{
    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

unsigned int compile(unsigned int type, const std::string& src)
{
    unsigned int shader = glCreateShader(type);
    const char* s = src.c_str();
    glShaderSource(shader, 1, &s, nullptr);
    glCompileShader(shader);
    return shader;
}

unsigned int makeProgram(const std::string& vs, const std::string& fs)
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

// =================================================
// LOAD MODEL (ALL MESHES)
// =================================================
void loadModel(const char* path, std::vector<float>& vertices)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_FlipUVs);

    for(unsigned m = 0; m < scene->mNumMeshes; m++)
    {
        aiMesh* mesh = scene->mMeshes[m];

        for(unsigned i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];

            for(int j = 0; j < 3; j++)
            {
                unsigned idx = face.mIndices[j];

                aiVector3D pos = mesh->mVertices[idx];
                aiVector3D norm = mesh->mNormals[idx];

                vertices.insert(vertices.end(),
                {
                    pos.x,pos.y,pos.z,
                    norm.x,norm.y,norm.z,
                    0.0f,0.0f   // no texture
                });
            }
        }
    }
}

// =================================================
// LOAD TEXTURE
// =================================================
unsigned int loadTexture(const char* path)
{
    unsigned int tex;
    glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_2D,tex);

    int w,h,c;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path,&w,&h,&c,0);

    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,
                 GL_RGB,GL_UNSIGNED_BYTE,data);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

    stbi_image_free(data);
    return tex;
}

// =================================================
// EXTREME FLOOR
// =================================================
float floorVertices[] =
{
    -100,0,-100, 0,1,0, 0,0,
     100,0,-100, 0,1,0, 400,0,
     100,0, 100, 0,1,0, 400,400,
    -100,0,-100, 0,1,0, 0,0,
     100,0, 100, 0,1,0, 400,400,
    -100,0, 100, 0,1,0, 0,400
};

// =================================================
// FILTER MODES
// =================================================
enum FilterMode { NO_MIPMAP=0, NEAREST, BILINEAR, TRILINEAR };
int currentMode = TRILINEAR;

void applyFilter(unsigned tex)
{
    glBindTexture(GL_TEXTURE_2D, tex);

    if(currentMode==NO_MIPMAP)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    if(currentMode==NEAREST)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

    if(currentMode==BILINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

    if(currentMode==TRILINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

// =================================================
// MAIN
// =================================================
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_CORE_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT,
        "Mipmapping Interactive Demo",nullptr,nullptr);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window,true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();

    std::string vs = loadShader("shaders/mipmap.vs.glsl");
    std::string fs = loadShader("shaders/mipmap.fs.glsl");
    unsigned int shader = makeProgram(vs, fs);

    std::vector<float> modelData;
    loadModel("resources/models/teapot.glb", modelData);
    int modelCount = modelData.size()/8;

    unsigned modelVAO,modelVBO;
    glGenVertexArrays(1,&modelVAO);
    glGenBuffers(1,&modelVBO);

    glBindVertexArray(modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER,modelVBO);
    glBufferData(GL_ARRAY_BUFFER,
        modelData.size()*sizeof(float),
        modelData.data(),GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned floorVAO,floorVBO;
    glGenVertexArrays(1,&floorVAO);
    glGenBuffers(1,&floorVBO);

    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER,floorVBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(floorVertices),floorVertices,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned checkerTex = loadTexture("resources/textures/checker.png");

    while(!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Mip Mapping");
        ImGui::RadioButton("No Mipmap",&currentMode,0);
        ImGui::RadioButton("Nearest",&currentMode,1);
        ImGui::RadioButton("Bilinear",&currentMode,2);
        ImGui::RadioButton("Trilinear",&currentMode,3);
        ImGui::End();

        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glm::mat4 view =
            glm::lookAt(cameraPos,
                        cameraPos + cameraFront,
                        cameraUp);

        glm::mat4 projection =
            glm::perspective(glm::radians(45.f),
            (float)SCR_WIDTH/SCR_HEIGHT,0.1f,500.f);

        glUseProgram(shader);

        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),
            1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),
            1,GL_FALSE,glm::value_ptr(projection));

        applyFilter(checkerTex);
        glBindTexture(GL_TEXTURE_2D,checkerTex);

        // Floor
        glm::mat4 model = glm::mat4(1);
        glUniformMatrix4fv(glGetUniformLocation(shader,"model"),
            1,GL_FALSE,glm::value_ptr(model));
        glUniform1i(glGetUniformLocation(shader,"useTexture"),1);
        glBindVertexArray(floorVAO);
        glDrawArrays(GL_TRIANGLES,0,6);

        // Rotating clean teapot
        model = glm::translate(glm::mat4(1),glm::vec3(0,3,0));
        model = glm::rotate(model,currentFrame,glm::vec3(0,1,0));
        model = glm::scale(model,glm::vec3(3));

        glUniformMatrix4fv(glGetUniformLocation(shader,"model"),
            1,GL_FALSE,glm::value_ptr(model));
        glUniform1i(glGetUniformLocation(shader,"useTexture"),0);
        glBindVertexArray(modelVAO);
        glDrawArrays(GL_TRIANGLES,0,modelCount);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    glfwTerminate();
}
