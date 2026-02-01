#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

std::string loadShaderSource(const char* path)
{
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int compileShader(unsigned int type, const std::string& src)
{
    unsigned int s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    return s;
}

unsigned int createProgram(const std::string& vs, const std::string& fs)
{
    unsigned int v = compileShader(GL_VERTEX_SHADER, vs);
    unsigned int f = compileShader(GL_FRAGMENT_SHADER, fs);

    unsigned int p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window =
        glfwCreateWindow(800, 600, "GLB Teapot + ImGui", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        "resources/models/teapot.glb",
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices
    );

    if (!scene || !scene->HasMeshes())
    {
        std::cerr << "Failed to load GLB\n";
        return -1;
    }

    std::vector<float> vertices;

    for (unsigned m = 0; m < scene->mNumMeshes; m++)
    {
        aiMesh* mesh = scene->mMeshes[m];

        for (unsigned i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];

            for (unsigned j = 0; j < 3; j++)
            {
                unsigned idx = face.mIndices[j];

                aiVector3D p = mesh->mVertices[idx];
                aiVector3D n = mesh->mNormals[idx];

                vertices.insert(vertices.end(),
                {
                    p.x, p.y, p.z,
                    n.x, n.y, n.z
                });
            }
        }
    }

    int vertexCount = vertices.size() / 6;

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float),
        vertices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    unsigned int program = createProgram(
        loadShaderSource("shaders/shader.vs.glsl"),
        loadShaderSource("shaders/shader.fs.glsl")
    );

    float roughness = 0.9f;
    float shininess = 32.0f;
    float toonThreshold1 = 0.9f;
    float toonThreshold2 = 0.6f;

    float toonLevel1 = 1.0f;
    float toonLevel2 = 0.4f;
    float toonLevel3 = 0.2f;

    while (!glfwWindowShouldClose(window))
    {
        float t = (float)glfwGetTime();

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Material Controls");
        ImGui::SliderFloat("Shininess", &shininess, 1.0f, 1000.0f);
        ImGui::SliderFloat("Roughness", &roughness, 0.05f, 2.0f);
        ImGui::Separator();
        ImGui::Text("Toon Shading");

        ImGui::SliderFloat("Threshold High", &toonThreshold1, 0.0f, 1.0f);
        ImGui::SliderFloat("Threshold Mid", &toonThreshold2, 0.0f, toonThreshold1);

        ImGui::SliderFloat("Light Level High", &toonLevel1, 0.0f, 1.0f);
        ImGui::SliderFloat("Light Level Mid", &toonLevel2, 0.0f, 1.0f);
        ImGui::SliderFloat("Light Level Low", &toonLevel3, 0.0f, 1.0f);
        ImGui::Text("0 = Phong | 1 = cook | 2 = toon");
        ImGui::End();

        glClearColor(0.8f, 0.8f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);

        glm::mat4 view = glm::lookAt(
            glm::vec3(0, 0, 10),
            glm::vec3(0, 0, 0),
            glm::vec3(0, 1, 0)
        );

        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            800.0f / 600.0f,
            0.1f, 100.0f
        );

        glUniformMatrix4fv(glGetUniformLocation(program, "view"),
            1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"),
            1, GL_FALSE, &projection[0][0]);

        glUniform3f(glGetUniformLocation(program, "lightPos"), 2, 3, 2);
        glUniform3f(glGetUniformLocation(program, "viewPos"), 0, 0, 10);
        glUniform3f(glGetUniformLocation(program, "baseColor"), 0.8, 0.5, 0.3);
        glUniform1f(glGetUniformLocation(program, "shininess"), shininess);
        glUniform1f(glGetUniformLocation(program, "roughness"), roughness);
        glUniform1f(glGetUniformLocation(program, "toonThreshold1"), toonThreshold1);
        glUniform1f(glGetUniformLocation(program, "toonThreshold2"), toonThreshold2);

        glUniform1f(glGetUniformLocation(program, "toonLevel1"), toonLevel1);
        glUniform1f(glGetUniformLocation(program, "toonLevel2"), toonLevel2);
        glUniform1f(glGetUniformLocation(program, "toonLevel3"), toonLevel3);
        glBindVertexArray(VAO);

        for (int i = 0; i < 3; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3((i - 1) * 3.5f, 0, 0));
            model = glm::rotate(model, t, glm::vec3(0, 1, 0));
            model = glm::scale(model, glm::vec3(0.75f));

            glUniformMatrix4fv(glGetUniformLocation(program, "model"),
                1, GL_FALSE, &model[0][0]);

            glUniform1i(glGetUniformLocation(program, "shadingModel"), i);

            glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
