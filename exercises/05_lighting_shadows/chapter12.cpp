#include "geometry.hpp"
#include "gl_support.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

struct ShadowMap {
    GLuint framebuffer = 0;
    GLuint texture = 0;
    int size = 1024;

    ShadowMap() {
        glGenFramebuffers(1, &framebuffer);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               texture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Shadow framebuffer is incomplete");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    ~ShadowMap() {
        glDeleteTextures(1, &texture);
        glDeleteFramebuffers(1, &framebuffer);
    }
};

std::vector<float> groundVertices() {
    return {
        -8,0,-8, 0,1,0,   8,0,-8, 0,1,0,   8,0,8, 0,1,0,
        -8,0,-8, 0,1,0,   8,0, 8, 0,1,0,  -8,0,8, 0,1,0};
}

int main(int argc, char** argv) {
    const float currentDepth = 0.63f;
    const float closestDepth = 0.41f;
    std::cout << "chapter12 check: current " << currentDepth << " vs closest "
              << closestDepth << " -> "
              << (currentDepth > closestDepth ? "shadowed" : "visible") << '\n';
    if (codea::checkOnly(argc, argv)) {
        return 0;
    }

    constexpr const char* shadowVertex = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        uniform mat4 uLightViewProjection;
        uniform mat4 uModel;
        void main(){ gl_Position=uLightViewProjection*uModel*vec4(aPosition,1.0); }
    )";
    constexpr const char* shadowFragment = R"(
        #version 410 core
        void main(){}
    )";
    constexpr const char* mainVertex = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        layout(location=1) in vec3 aNormal;
        uniform mat4 uModel;
        uniform mat4 uViewProjection;
        uniform mat4 uLightViewProjection;
        uniform mat3 uNormalMatrix;
        out vec3 vWorldPosition;
        out vec3 vWorldNormal;
        out vec4 vLightClip;
        void main(){
            vec4 world=uModel*vec4(aPosition,1.0);
            vWorldPosition=world.xyz;
            vWorldNormal=normalize(uNormalMatrix*aNormal);
            vLightClip=uLightViewProjection*world;
            gl_Position=uViewProjection*world;
        }
    )";
    constexpr const char* mainFragment = R"(
        #version 410 core
        in vec3 vWorldPosition;
        in vec3 vWorldNormal;
        in vec4 vLightClip;
        uniform sampler2D uShadowMap;
        uniform vec3 uDirectionToLight;
        uniform vec3 uBaseColor;
        layout(location=0) out vec4 outColor;
        float visibility(){
            vec3 projected=vLightClip.xyz/vLightClip.w;
            vec3 shadowCoord=projected*0.5+0.5;
            if(shadowCoord.z<0.0 || shadowCoord.z>1.0 ||
               any(lessThan(shadowCoord.xy,vec2(0.0))) ||
               any(greaterThan(shadowCoord.xy,vec2(1.0)))) return 1.0;
            float closest=texture(uShadowMap,shadowCoord.xy).r;
            float nDotL=max(dot(normalize(vWorldNormal),normalize(uDirectionToLight)),0.0);
            float bias=max(0.0025*(1.0-nDotL),0.0005);
            return shadowCoord.z-bias>closest ? 0.22 : 1.0;
        }
        void main(){
            float diffuse=max(dot(normalize(vWorldNormal),normalize(uDirectionToLight)),0.0);
            vec3 color=uBaseColor*(0.08+diffuse*visibility());
            outColor=vec4(color,1.0);
        }
    )";

    try {
        codea::Window window{1000, 700, "Chapter 12 - directional shadow map"};
        const codea::Program shadowProgram{shadowVertex, shadowFragment};
        const codea::Program mainProgram{mainVertex, mainFragment};
        const codea::Mesh cube{codea::cubeVertices(), 6,
                               {{0, 3, 0}, {1, 3, 3 * sizeof(float)}},
                               codea::cubeIndices()};
        const codea::Mesh ground{groundVertices(), 6,
                                 {{0, 3, 0}, {1, 3, 3 * sizeof(float)}}};
        ShadowMap shadow;
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        const glm::vec3 directionToLight = glm::normalize(glm::vec3{0.55f, 1.0f, 0.35f});
        const glm::mat4 lightView = glm::lookAt(directionToLight * 12.0f,
                                               glm::vec3{0}, glm::vec3{0, 1, 0});
        const glm::mat4 lightProjection = glm::ortho(-8.0f, 8.0f, -8.0f, 8.0f,
                                                      0.1f, 30.0f);
        const glm::mat4 lightViewProjection = lightProjection * lightView;

        while (!window.shouldClose()) {
            const glm::mat4 cubeModel = glm::translate(glm::mat4{1.0f}, {0, 1, 0}) *
                                        glm::rotate(glm::mat4{1.0f},
                                                    static_cast<float>(window.time()) * 0.35f,
                                                    glm::vec3{0, 1, 0});
            const glm::mat4 groundModel{1.0f};

            glBindFramebuffer(GL_FRAMEBUFFER, shadow.framebuffer);
            glViewport(0, 0, shadow.size, shadow.size);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            shadowProgram.use();
            glUniformMatrix4fv(shadowProgram.uniform("uLightViewProjection"), 1,
                               GL_FALSE, &lightViewProjection[0][0]);
            glUniformMatrix4fv(shadowProgram.uniform("uModel"), 1, GL_FALSE,
                               &cubeModel[0][0]);
            cube.draw();
            glUniformMatrix4fv(shadowProgram.uniform("uModel"), 1, GL_FALSE,
                               &groundModel[0][0]);
            ground.draw();

            const auto [width, height] = window.framebufferSize();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            glClearColor(0.025f, 0.03f, 0.045f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            const float aspect = height > 0 ? static_cast<float>(width) / height : 1.0f;
            const glm::vec3 cameraPosition{5.5f, 4.0f, 7.0f};
            const glm::mat4 view = glm::lookAt(cameraPosition, glm::vec3{0, 0.5f, 0},
                                              glm::vec3{0, 1, 0});
            const glm::mat4 projection = glm::perspective(glm::radians(52.0f), aspect,
                                                           0.1f, 100.0f);
            const glm::mat4 viewProjection = projection * view;

            mainProgram.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, shadow.texture);
            glUniform1i(mainProgram.uniform("uShadowMap"), 0);
            glUniformMatrix4fv(mainProgram.uniform("uViewProjection"), 1, GL_FALSE,
                               &viewProjection[0][0]);
            glUniformMatrix4fv(mainProgram.uniform("uLightViewProjection"), 1, GL_FALSE,
                               &lightViewProjection[0][0]);
            glUniform3fv(mainProgram.uniform("uDirectionToLight"), 1,
                         &directionToLight[0]);

            const auto drawObject = [&](const codea::Mesh& mesh, const glm::mat4& model,
                                        const glm::vec3& color) {
                const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3{model});
                glUniformMatrix4fv(mainProgram.uniform("uModel"), 1, GL_FALSE,
                                   &model[0][0]);
                glUniformMatrix3fv(mainProgram.uniform("uNormalMatrix"), 1, GL_FALSE,
                                   &normalMatrix[0][0]);
                glUniform3fv(mainProgram.uniform("uBaseColor"), 1, &color[0]);
                mesh.draw();
            };
            drawObject(ground, groundModel, {0.32f, 0.36f, 0.34f});
            drawObject(cube, cubeModel, {0.18f, 0.56f, 0.88f});
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
