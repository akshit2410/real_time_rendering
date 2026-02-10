#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ---------- ImGui ----------
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// =======================================================
// Shader helpers
// =======================================================
std::string loadFile(const char* path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint compile(GLenum type, const std::string& src)
{
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    return s;
}

GLuint makeProgram(const char* vs, const char* fs)
{
    GLuint v = compile(GL_VERTEX_SHADER, loadFile(vs));
    GLuint f = compile(GL_FRAGMENT_SHADER, loadFile(fs));

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

// =======================================================
// Texture loader
// =======================================================
GLuint loadTexture(const char* path)
{
    int w, h, c;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &c, 0);

    if (!data)
    {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    GLenum format = (c == 4) ? GL_RGBA : GL_RGB;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0,
                 format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return tex;
}

// =======================================================
// MAIN
// =======================================================
int main()
{
    // ---------- GLFW ----------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(1200, 800, "Normal Mapping – Side-by-Side Comparison", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // ---------- ImGui ----------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // =======================================================
    // Load GLTF (ALL meshes)
    // =======================================================
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        "resources/models/model.gltf",
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace
    );

    if (!scene || !scene->HasMeshes())
    {
        std::cerr << importer.GetErrorString() << std::endl;
        return -1;
    }

    std::vector<float> verts;

    for (unsigned m = 0; m < scene->mNumMeshes; m++)
    {
        aiMesh* mesh = scene->mMeshes[m];

        for (unsigned i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace& f = mesh->mFaces[i];
            for (unsigned j = 0; j < 3; j++)
            {
                unsigned idx = f.mIndices[j];

                aiVector3D p = mesh->mVertices[idx];
                aiVector3D n = mesh->mNormals[idx];

                aiVector3D uv(0, 0, 0);
                if (mesh->HasTextureCoords(0))
                    uv = mesh->mTextureCoords[0][idx];

                aiVector3D t(1, 0, 0);
                if (mesh->HasTangentsAndBitangents())
                    t = mesh->mTangents[idx];

                verts.insert(verts.end(), {
                    p.x, p.y, p.z,
                    n.x, n.y, n.z,
                    uv.x, uv.y,
                    t.x, t.y, t.z
                });
            }
        }
    }

    // ---------- Compute mesh center (for local rotation) ----------
    glm::vec3 meshCenter(0.0f);
    int vCount = verts.size() / 11;
    for (int i = 0; i < vCount; i++)
    {
        meshCenter += glm::vec3(
            verts[i * 11 + 0],
            verts[i * 11 + 1],
            verts[i * 11 + 2]
        );
    }
    meshCenter /= (float)vCount;

    // =======================================================
    // VAO / VBO
    // =======================================================
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(float),
                 verts.data(),
                 GL_STATIC_DRAW);

    int stride = 11 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // =======================================================
    // Shaders & textures
    // =======================================================
    GLuint program = makeProgram(
        "shaders/shader.vs.glsl",
        "shaders/shader.fs.glsl"
    );

    GLuint diffTex = loadTexture("resources/textures/brick_diff.png");
    GLuint normTex = loadTexture("resources/textures/brick_normal.png");

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "diffuseMap"), 0);
    glUniform1i(glGetUniformLocation(program, "normalMap"), 1);

    // =======================================================
    // Scene controls
    // =======================================================
    glm::vec3 cameraPos(0.0f, 0.0f, 6.0f);
    glm::vec3 lightPos(0.0f, 2.0f, 3.0f);

    float rotationSpeed = 0.6f;
    float normalStrength = 1.5f;
    float ambient = 0.35f;
    float separation = 1.5f;

    // =======================================================
    // Render loop
    // =======================================================
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // ---------- ImGui ----------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Normal Mapping Controls");
        ImGui::Text("LEFT  : Normal Map OFF");
        ImGui::Text("RIGHT : Normal Map ON");
        ImGui::Separator();
        ImGui::SliderFloat("Normal Strength", &normalStrength, 0.0f, 3.0f);
        ImGui::SliderFloat("Ambient", &ambient, 0.05f, 0.6f);
        ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.0f, 2.0f);
        ImGui::SliderFloat3("Light Position", &lightPos[0], -5.0f, 5.0f);
        ImGui::End();

        // ---------- Render ----------
        glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float t = (float)glfwGetTime();

        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.f), 1200.f / 800.f, 0.1f, 100.f);

        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, 0, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, 0, &proj[0][0]);
        glUniform3fv(glGetUniformLocation(program, "lightPos"), 1, &lightPos[0]);
        glUniform3fv(glGetUniformLocation(program, "viewPos"), 1, &cameraPos[0]);
        glUniform1f(glGetUniformLocation(program, "ambientStrength"), ambient);
        glUniform1f(glGetUniformLocation(program, "normalStrength"), normalStrength);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, normTex);

        glBindVertexArray(VAO);

        // ---------- LEFT: NO normal map ----------
        glm::mat4 modelLeft(1.0f);
        modelLeft = glm::translate(modelLeft, glm::vec3(-separation, 0, 0));
        modelLeft = glm::translate(modelLeft, meshCenter);
        modelLeft = glm::rotate(modelLeft, t * rotationSpeed, glm::vec3(0, 1, 0));
        modelLeft = glm::translate(modelLeft, -meshCenter);

        glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, 0, &modelLeft[0][0]);
        glUniform1i(glGetUniformLocation(program, "useNormalMap"), false);
        glDrawArrays(GL_TRIANGLES, 0, verts.size() / 11);

        // ---------- RIGHT: WITH normal map ----------
        glm::mat4 modelRight(1.0f);
        modelRight = glm::translate(modelRight, glm::vec3(+separation, 0, 0));
        modelRight = glm::translate(modelRight, meshCenter);
        modelRight = glm::rotate(modelRight, t * rotationSpeed, glm::vec3(0, 1, 0));
        modelRight = glm::translate(modelRight, -meshCenter);

        glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, 0, &modelRight[0][0]);
        glUniform1i(glGetUniformLocation(program, "useNormalMap"), true);
        glDrawArrays(GL_TRIANGLES, 0, verts.size() / 11);

        // ---------- ImGui draw ----------
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ---------- Cleanup ----------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
