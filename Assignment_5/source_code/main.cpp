#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Camera
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 10.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
float fov = 45.0f;

// Light
glm::vec3 lightPos = glm::vec3(5.0f, 5.0f, 5.0f);
glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
float lightIntensity = 30.0f;

// Material & Sampling
float roughness = 0.3f;
int samplingMode = 1; // 0: Uniform, 1: GGX
int frameCount = 0;
bool useMonteCarlo = false;
int maxSamples = 1000;
bool limitSamples = false;
int samplesPerFrame = 1;
float zoomLevel = 1.0f;
glm::vec2 zoomOffset = glm::vec2(0.0f, 0.0f);
bool splitScreen = false;

// ----------------------------------------------------------------------------------
// Shader Class
// ----------------------------------------------------------------------------------
class Shader {
public:
    unsigned int ID;
    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try {
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            vShaderFile.close();
            fShaderFile.close();
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        } catch (std::ifstream::failure& e) {
            std::cout << "ERROR::SHADER::FILE_NOT_READ" << std::endl;
        }
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();
        unsigned int vertex, fragment;
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }
    void use() { glUseProgram(ID); }
    void setBool(const std::string &name, bool value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value); }
    void setInt(const std::string &name, int value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), value); }
    void setFloat(const std::string &name, float value) const { glUniform1f(glGetUniformLocation(ID, name.c_str()), value); }
    void setVec3(const std::string &name, const glm::vec3 &value) const { glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }
    void setMat4(const std::string &name, const glm::mat4 &mat) const { glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]); }

private:
    void checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};

// ----------------------------------------------------------------------------------
// Quad Buffer for Blitting
// ----------------------------------------------------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad() {
    if (quadVAO == 0) {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// ----------------------------------------------------------------------------------
// Sphere Geometry
// ----------------------------------------------------------------------------------
unsigned int sphereVAO = 0;
unsigned int indexCount;
void renderSphere() {
    if (sphereVAO == 0) {
        glGenVertexArrays(1, &sphereVAO);
        unsigned int vbo, ebo;
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;

        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
            for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);
                float yPos = std::cos(ySegment * M_PI);
                float zPos = std::sin(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);

                positions.push_back(glm::vec3(xPos, yPos, zPos));
                normals.push_back(glm::vec3(xPos, yPos, zPos));
            }
        }

        for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
            for (unsigned int x = 0; x < X_SEGMENTS; ++x) {
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x + 1);

                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x + 1);
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x + 1);
            }
        }
        indexCount = (unsigned int)indices.size();

        std::vector<float> data;
        for (unsigned int i = 0; i < positions.size(); ++i) {
            data.push_back(positions[i].x);
            data.push_back(positions[i].y);
            data.push_back(positions[i].z);
            data.push_back(normals[i].x);
            data.push_back(normals[i].y);
            data.push_back(normals[i].z);
        }
        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        float stride = 6 * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "MSc BRDF Analysis", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // DPI FIX: Get actual framebuffer size
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glViewport(0, 0, width, height);

    glEnable(GL_DEPTH_TEST);

    // Shaders
    Shader pbrShader("../shaders/pbr.vs.glsl", "../shaders/pbr.fs.glsl");
    
    // Simple screen-space shader for blitting accumulation texture
    const char* screenVS = R"(#version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoords;
    out vec2 TexCoords;
    void main() { TexCoords = aTexCoords; gl_Position = vec4(aPos, 1.0); })";
    
    const char* screenFS = R"(#version 330 core
    out vec4 FragColor;
    in vec2 TexCoords;
    uniform sampler2D screenTexture;
    uniform int frameCount;
    uniform float zoom;
    uniform vec2 offset;
    uniform bool splitScreen;
    void main() {
        vec2 uv = (TexCoords - 0.5) / zoom + 0.5 + offset;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) discard;
        vec3 color = texture(screenTexture, uv).rgb;
        color /= max(float(frameCount), 1.0);
        color = color / (color + vec3(1.0));
        color = color *  2.0f;
        color = pow(color, vec3(1.0/2.2));
        FragColor = vec4(color, 1.0);
    })";

    unsigned int sv = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(sv, 1, &screenVS, NULL); glCompileShader(sv);
    unsigned int sf = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sf, 1, &screenFS, NULL); glCompileShader(sf);
    unsigned int screenShader = glCreateProgram();
    glAttachShader(screenShader, sv); glAttachShader(screenShader, sf); glLinkProgram(screenShader);

    // --- Accumulation Buffer Setup ---
    unsigned int accumFBO;
    glGenFramebuffers(1, &accumFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, accumFBO);
    
    unsigned int accumTexture;
    glGenTextures(1, &accumTexture);
    glBindTexture(GL_TEXTURE_2D, accumTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTexture, 0);

    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ImGui Setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui Window
        {
            ImGui::Begin("BRDF Analysis Controls");
            ImGui::Text("Mode Selection");
            if(ImGui::Checkbox("Use Monte Carlo Sampling", &useMonteCarlo)) frameCount = 0;
            
            if (useMonteCarlo) {
                ImGui::Separator();
                ImGui::Text("Comparison Mode");
                if (ImGui::Checkbox("Split-Screen (Uniform vs GGX)", &splitScreen)) frameCount = 0;

                ImGui::Separator();
                ImGui::Text("Monte Carlo Parameters");
                if (!splitScreen) {
                    if(ImGui::RadioButton("Uniform Sampling", &samplingMode,0)) frameCount = 0;
                    if(ImGui::RadioButton("GGX Importance Sampling", &samplingMode,1)) frameCount = 0;
                } else {
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Active: Uniform (Left) | GGX (Right)");
                }
                
                ImGui::Checkbox("Limit Samples", &limitSamples);
                if (limitSamples) {
                    ImGui::SliderInt("Max Samples", &maxSamples, 1, 10000);
                }
                
                if (limitSamples && frameCount >= maxSamples) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Sampling Stopped at %d", frameCount);
                } else {
                    ImGui::Text("Samples: %d", frameCount);
                }
                if(ImGui::Button("Reset Accumulation")) frameCount = 0;
                
                ImGui::Separator();
                ImGui::SliderInt("Samples per Frame", &samplesPerFrame, 1, 16);
                ImGui::TextDisabled("Increase to speed up convergence demo.");
            } else {
                ImGui::Separator();
                ImGui::Text("Analytical Mode");
            }
            
            ImGui::Separator();
            ImGui::Text("Experiment Presets");
            if (ImGui::Button("Low Roughness (High Variance)")) {
                roughness = 0.05f;
                frameCount = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("High Roughness (Low Variance)")) {
                roughness = 0.5f;
                frameCount = 0;
            }

            ImGui::Separator();
            ImGui::Text("Global Parameters");
            if(ImGui::SliderFloat("Roughness", &roughness, 0.01f, 1.0f)) frameCount = 0;
            if(ImGui::DragFloat3("Light Position", (float*)&lightPos, 0.1f)) frameCount = 0;
            if(ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 300.0f)) frameCount = 0;
            if(ImGui::SliderFloat("Camera FOV (Optical Zoom)", &fov, 5.0f, 90.0f)) frameCount = 0;

            ImGui::Separator();
            ImGui::Text("Detail View (Digital Post-Zoom)");
            if (ImGui::Button("Overall View")) { zoomLevel = 1.0f; zoomOffset = glm::vec2(0.0f); }
            ImGui::SameLine();
            if (ImGui::Button("Left Sphere"))  { zoomLevel = 3.5f; zoomOffset = glm::vec2(-0.27f, 0.0f); }
            ImGui::SameLine();
            if (ImGui::Button("Middle Sphere")){ zoomLevel = 3.5f; zoomOffset = glm::vec2(0.0f, 0.0f); }
            ImGui::SameLine();
            if (ImGui::Button("Right Sphere")) { zoomLevel = 3.5f; zoomOffset = glm::vec2(0.27f, 0.0f); }
            
            ImGui::SliderFloat("Manual Zoom", &zoomLevel, 1.0f, 10.0f);
            ImGui::DragFloat2("Pan Offset", (float*)&zoomOffset, 0.005f, -0.5f, 0.5f);

            ImGui::End();
        }

        if (useMonteCarlo) {
            glBindFramebuffer(GL_FRAMEBUFFER, accumFBO);
            glViewport(0, 0, width, height);

            if (frameCount == 0) {
                glDisable(GL_BLEND);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            } else {
                glClear(GL_DEPTH_BUFFER_BIT);
            }

            for (int s = 0; s < samplesPerFrame; ++s) {
                bool shouldSample = !limitSamples || (frameCount < maxSamples);
                if (!shouldSample) break;
                
                frameCount++;
                
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);

                pbrShader.use();
                glm::mat4 projection = glm::perspective(glm::radians(fov), (float)width / (float)height, 0.1f, 100.0f);
                glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
                pbrShader.setMat4("projection", projection);
                pbrShader.setMat4("view", view);
                pbrShader.setVec3("camPos", cameraPos);
                pbrShader.setVec3("lightPos", lightPos);
                pbrShader.setVec3("lightColor", lightColor * lightIntensity);
                pbrShader.setFloat("roughness", roughness);
                pbrShader.setInt("frameCount", frameCount);
                pbrShader.setInt("samplingMode", samplingMode);
                pbrShader.setBool("useMonteCarlo", useMonteCarlo);
                pbrShader.setBool("splitScreen", splitScreen);
                pbrShader.setFloat("screenWidth", (float)width);

                // Render 3 Spheres
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(-3.5f, 0.0f, 0.0f));
                pbrShader.setMat4("model", model);
                pbrShader.setVec3("albedo", glm::vec3(1.0f, 0.3f, 0.1f));
                pbrShader.setFloat("metallic", 0.0f);
                pbrShader.setInt("brdfMode", 0);
                renderSphere();

                model = glm::mat4(1.0f);
                pbrShader.setMat4("model", model);
                pbrShader.setVec3("albedo", glm::vec3(0.5f, 0.5f, 0.5f));
                pbrShader.setFloat("metallic", 0.0f);
                pbrShader.setInt("brdfMode", 1);
                renderSphere();

                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(3.5f, 0.0f, 0.0f));
                pbrShader.setMat4("model", model);
                pbrShader.setVec3("albedo", glm::vec3(1.0f, 0.71f, 0.29f));
                pbrShader.setFloat("metallic", 1.0f);
                pbrShader.setInt("brdfMode", 1);
                renderSphere();
                
                // Note: Clear depth *within* the loop if we had overlapping objects, 
                // but since it's just 3 spheres, once per frame is enough.
            }
            glDisable(GL_BLEND);
        } else {
            frameCount = 1;
            glBindFramebuffer(GL_FRAMEBUFFER, accumFBO); 
            glViewport(0, 0, width, height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            pbrShader.use();
            glm::mat4 projection = glm::perspective(glm::radians(fov), (float)width / (float)height, 0.1f, 100.0f);
            glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
            pbrShader.setMat4("projection", projection);
            pbrShader.setMat4("view", view);
            pbrShader.setVec3("camPos", cameraPos);
            pbrShader.setVec3("lightPos", lightPos);
            pbrShader.setVec3("lightColor", lightColor * lightIntensity);
            pbrShader.setFloat("roughness", roughness);
            pbrShader.setInt("frameCount", frameCount);
            pbrShader.setInt("samplingMode", samplingMode);
            pbrShader.setBool("useMonteCarlo", useMonteCarlo);
            pbrShader.setBool("splitScreen", splitScreen);
            pbrShader.setFloat("screenWidth", (float)width);

            // Render 3 Spheres (Analytical)
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-3.5f, 0.0f, 0.0f));
            pbrShader.setMat4("model", model);
            pbrShader.setVec3("albedo", glm::vec3(1.0f, 0.3f, 0.1f));
            pbrShader.setFloat("metallic", 0.0f);
            pbrShader.setInt("brdfMode", 0);
            renderSphere();

            model = glm::mat4(1.0f);
            pbrShader.setMat4("model", model);
            pbrShader.setVec3("albedo", glm::vec3(0.5f, 0.5f, 0.5f));
            pbrShader.setFloat("metallic", 0.0f);
            pbrShader.setInt("brdfMode", 1);
            renderSphere();

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(3.5f, 0.0f, 0.0f));
            pbrShader.setMat4("model", model);
            pbrShader.setVec3("albedo", glm::vec3(1.0f, 0.71f, 0.29f));
            pbrShader.setFloat("metallic", 1.0f);
            pbrShader.setInt("brdfMode", 1);
            renderSphere();
        }

        // --- Post-Processing / Blit Pass ---
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(screenShader);
        glUniform1i(glGetUniformLocation(screenShader, "screenTexture"), 0);
        glUniform1i(glGetUniformLocation(screenShader, "frameCount"), frameCount);
        glUniform1f(glGetUniformLocation(screenShader, "zoom"), zoomLevel);
        glUniform2f(glGetUniformLocation(screenShader, "offset"), zoomOffset.x, zoomOffset.y);
        glUniform1i(glGetUniformLocation(screenShader, "splitScreen"), (int)splitScreen);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, accumTexture);
        renderQuad();

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