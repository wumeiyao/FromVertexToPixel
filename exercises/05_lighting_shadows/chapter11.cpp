#include "geometry.hpp"
#include "gl_support.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

int main(int argc, char** argv) {
    const glm::vec3 normal = glm::normalize(glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::vec3 directionToLight = glm::normalize(glm::vec3{0.5f, 1.0f, 0.3f});
    std::cout << "chapter11 check: N dot L = "
              << glm::max(glm::dot(normal, directionToLight), 0.0f) << '\n';
    if (codea::checkOnly(argc, argv)) {
        return 0;
    }

    constexpr const char* vertexShader = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        layout(location=1) in vec3 aNormal;
        uniform mat4 uModel;
        uniform mat4 uViewProjection;
        uniform mat3 uNormalMatrix;
        out vec3 vWorldPosition;
        out vec3 vWorldNormal;
        void main() {
            vec4 world = uModel * vec4(aPosition, 1.0);
            vWorldPosition = world.xyz;
            vWorldNormal = normalize(uNormalMatrix * aNormal);
            gl_Position = uViewProjection * world;
        }
    )";
    constexpr const char* fragmentShader = R"(
        #version 410 core
        in vec3 vWorldPosition;
        in vec3 vWorldNormal;
        uniform vec3 uBaseColor;
        uniform vec3 uDirectionToLight;
        uniform vec3 uLightColor;
        uniform vec3 uCameraPosition;
        layout(location=0) out vec4 outColor;
        void main() {
            vec3 n = normalize(vWorldNormal);
            vec3 l = normalize(uDirectionToLight);
            vec3 v = normalize(uCameraPosition - vWorldPosition);
            vec3 h = normalize(l + v);
            float diffuse = max(dot(n, l), 0.0);
            float specular = diffuse > 0.0 ? pow(max(dot(n, h), 0.0), 48.0) : 0.0;
            vec3 linearColor = uBaseColor * (0.08 + uLightColor * diffuse)
                             + uLightColor * specular * 0.35;
            outColor = vec4(linearColor, 1.0);
        }
    )";

    try {
        codea::Window window{900, 650, "Chapter 11 - directional light"};
        const codea::Program program{vertexShader, fragmentShader};
        const codea::Mesh cube{codea::cubeVertices(), 6,
                               {{0, 3, 0}, {1, 3, 3 * sizeof(float)}},
                               codea::cubeIndices()};
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        while (!window.shouldClose()) {
            const auto [width, height] = window.framebufferSize();
            const float aspect = height > 0 ? static_cast<float>(width) / height : 1.0f;
            const glm::vec3 cameraPosition{0.0f, 2.5f, 6.0f};
            const glm::mat4 view = glm::lookAt(cameraPosition, glm::vec3{0},
                                              glm::vec3{0, 1, 0});
            const glm::mat4 projection = glm::perspective(glm::radians(52.0f), aspect,
                                                           0.1f, 100.0f);
            const glm::mat4 model = glm::rotate(glm::mat4{1.0f},
                                                static_cast<float>(window.time()) * 0.55f,
                                                glm::vec3{0, 1, 0}) *
                                    glm::scale(glm::mat4{1.0f}, {1.4f, 0.8f, 0.6f});
            const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3{model});
            const glm::mat4 viewProjection = projection * view;

            glClearColor(0.018f, 0.025f, 0.04f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            program.use();
            glUniformMatrix4fv(program.uniform("uModel"), 1, GL_FALSE, &model[0][0]);
            glUniformMatrix4fv(program.uniform("uViewProjection"), 1, GL_FALSE,
                               &viewProjection[0][0]);
            glUniformMatrix3fv(program.uniform("uNormalMatrix"), 1, GL_FALSE,
                               &normalMatrix[0][0]);
            glUniform3fv(program.uniform("uDirectionToLight"), 1, &directionToLight[0]);
            glUniform3f(program.uniform("uLightColor"), 1.0f, 0.92f, 0.78f);
            glUniform3fv(program.uniform("uCameraPosition"), 1, &cameraPosition[0]);
            glUniform3f(program.uniform("uBaseColor"), 0.13f, 0.48f, 0.82f);
            cube.draw();
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
