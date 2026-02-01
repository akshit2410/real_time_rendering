#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// ================= ImGui =================
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
// ========================================

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

// =====================================================
// SHADER HELPERS
// =====================================================
std::string loadShader(const char* path)
{
    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

unsigned int compile(unsigned int type, const std::string& src)
{
    unsigned int s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);

    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << log << std::endl;
    }
    return s;
}

unsigned int makeProgram(const std::string& vs, const std::string& fs)
{
    unsigned int p = glCreateProgram();
    unsigned int v = compile(GL_VERTEX_SHADER, vs);
    unsigned int f = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

// =====================================================
// LOAD TEAPOT
// =====================================================
void loadGLB(const char* path, std::vector<float>& verts)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices
    );

    if (!scene || !scene->HasMeshes())
    {
        std::cerr << "Failed to load model\n";
        exit(1);
    }

    for (unsigned m = 0; m < scene->mNumMeshes; m++)
    {
        aiMesh* mesh = scene->mMeshes[m];
        for (unsigned i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (int j = 0; j < 3; j++)
            {
                unsigned idx = face.mIndices[j];
                aiVector3D p = mesh->mVertices[idx];
                aiVector3D n = mesh->mNormals[idx];

                verts.insert(verts.end(),
                {
                    p.x, p.y, p.z,
                    n.x, n.y, n.z
                });
            }
        }
    }
}

// =====================================================
// LOAD CUBEMAP
// =====================================================
unsigned int loadCubemap(const std::vector<std::string>& faces)
{
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    int w, h, c;
    stbi_set_flip_vertically_on_load(false);

    for (unsigned i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &c, 0);
        if (!data)
        {
            std::cerr << "Cubemap failed: " << faces[i] << "\n";
            continue;
        }

        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            0, GL_RGB, w, h, 0,
            GL_RGB, GL_UNSIGNED_BYTE, data
        );
        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return tex;
}

// =====================================================
// SKYBOX GEOMETRY
// =====================================================
float skyboxVertices[] =
{
    -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1,
    -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
    -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1,
     1, 1, 1,  1, 1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1,  1, 1, 1,
    -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1,
    -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1
};

// =====================================================
// MAIN
// =====================================================
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window =
        glfwCreateWindow(800, 600, "Glass Teapot with Skybox", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // ================= ImGui Init =================
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();
    // ==============================================

    // ---------- Load Teapot ----------
    std::vector<float> verts;
    loadGLB("resources/models/teapot.glb", verts);
    int vertexCount = verts.size() / 6;

    unsigned VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        verts.size() * sizeof(float),
        verts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ---------- Skybox VAO ----------
    unsigned skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);

    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(skyboxVertices),
        skyboxVertices,
        GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // ---------- Cubemap ----------
    unsigned cubemap = loadCubemap({
        "resources/cubemap/right.png",
        "resources/cubemap/left.png",
        "resources/cubemap/top.png",
        "resources/cubemap/bottom.png",
        "resources/cubemap/front.png",
        "resources/cubemap/back.png"
    });

    // ---------- Shaders ----------
    unsigned glassProgram = makeProgram(
        loadShader("shaders/shader.vs.glsl"),
        loadShader("shaders/shader.fs.glsl")
    );

    unsigned skyboxProgram = makeProgram(
        loadShader("shaders/skybox.vs.glsl"),
        loadShader("shaders/skybox.fs.glsl")
    );

    glUseProgram(glassProgram);
    glUniform1i(glGetUniformLocation(glassProgram, "skybox"), 0);

    glUseProgram(skyboxProgram);
    glUniform1i(glGetUniformLocation(skyboxProgram, "skybox"), 0);

    glm::vec3 cameraPos(0, 0, 6);

    // ===== ImGui-controlled values =====
    float ior = 1.52f;
    float fresnelPower = 5.0f;
    float dispersion = 0.02f;
    float exposure = 1.4f;
    float teapotScale = 1.0f;
    // ==================================

    // =====================================================
    // RENDER LOOP
    // =====================================================
    while (!glfwWindowShouldClose(window))
    {
        float t = (float)glfwGetTime();

        glfwPollEvents();

        // ---------- ImGui Frame ----------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Glass Controls");
        ImGui::SliderFloat("IOR", &ior, 1.0f, 2.5f);
        ImGui::SliderFloat("Fresnel Power", &fresnelPower, 1.0f, 10.0f);
        ImGui::SliderFloat("Dispersion", &dispersion, 0.0f, 0.05f);
        ImGui::SliderFloat("Exposure", &exposure, 0.5f, 2.5f);
        ImGui::SliderFloat("Teapot Scale", &teapotScale, 0.1f, 3.0f);
        ImGui::End();
        // --------------------------------

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(
            cameraPos, glm::vec3(0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(
            glm::radians(45.f), 800.f / 600.f, 0.1f, 100.f);

        // ---------- SKYBOX ----------
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);

        glUseProgram(skyboxProgram);

        glm::mat4 viewNoTranslate = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(
            glGetUniformLocation(skyboxProgram, "view"),
            1, GL_FALSE, glm::value_ptr(viewNoTranslate));
        glUniformMatrix4fv(
            glGetUniformLocation(skyboxProgram, "projection"),
            1, GL_FALSE, glm::value_ptr(proj));

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        // ---------- GLASS TEAPOT ----------
        glUseProgram(glassProgram);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -0.5f, 0.0f));
        model = glm::scale(model, glm::vec3(teapotScale));
        model = glm::rotate(model, t * 0.6f, glm::vec3(0, 1, 0));

        glUniformMatrix4fv(
            glGetUniformLocation(glassProgram, "model"),
            1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(
            glGetUniformLocation(glassProgram, "view"),
            1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(
            glGetUniformLocation(glassProgram, "projection"),
            1, GL_FALSE, glm::value_ptr(proj));

        glUniform3fv(
            glGetUniformLocation(glassProgram, "cameraPos"),
            1, glm::value_ptr(cameraPos));

        // 🔴 ONLY CHANGE HERE (from constants → ImGui)
        glUniform1f(glGetUniformLocation(glassProgram, "ior"), ior);
        glUniform1f(glGetUniformLocation(glassProgram, "fresnelPower"), fresnelPower);
        glUniform1f(glGetUniformLocation(glassProgram, "dispersion"), dispersion);
        glUniform1f(glGetUniformLocation(glassProgram, "exposure"), exposure);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        // ---------- ImGui Render ----------
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ================= ImGui Shutdown =================
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // ================================================

    glfwTerminate();
    return 0;
}
