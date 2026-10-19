#include "geometry.hpp"
#include "gl_support.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <iostream>
#include <string>
#include <vector>

struct Plane {
    glm::vec3 normal;
    float distance;
};

std::array<Plane, 6> extractFrustum(const glm::mat4& matrix) {
    const glm::mat4 transposed = glm::transpose(matrix);
    const std::array<glm::vec4, 6> equations{
        transposed[3] + transposed[0], transposed[3] - transposed[0],
        transposed[3] + transposed[1], transposed[3] - transposed[1],
        transposed[3] + transposed[2], transposed[3] - transposed[2]};
    std::array<Plane, 6> planes{};
    for (std::size_t i = 0; i < equations.size(); ++i) {
        const glm::vec3 normal{equations[i]};
        const float length = glm::length(normal);
        planes[i] = {normal / length, equations[i].w / length};
    }
    return planes;
}

bool visible(const glm::vec3& center, float radius,
             const std::array<Plane, 6>& frustum) {
    for (const Plane& plane : frustum) {
        if (glm::dot(plane.normal, center) + plane.distance < -radius) {
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    const glm::mat4 checkProjection = glm::perspective(glm::radians(60.0f), 1.0f,
                                                       0.1f, 30.0f);
    const glm::mat4 checkView = glm::lookAt(glm::vec3{0, 4, 8}, glm::vec3{0},
                                           glm::vec3{0, 1, 0});
    const auto checkFrustum = extractFrustum(checkProjection * checkView);
    std::cout << "chapter10 check: origin visible = "
              << std::boolalpha << visible({0, 0, 0}, 1.0f, checkFrustum)
              << ", far point visible = " << visible({100, 0, 0}, 1.0f, checkFrustum)
              << '\n';
    if (codea::checkOnly(argc, argv)) {
        return 0;
    }

    constexpr const char* vertexShader = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        uniform mat4 uMvp;
        uniform vec3 uColor;
        out vec3 vColor;
        void main(){ vColor=uColor; gl_Position=uMvp*vec4(aPosition,1.0); }
    )";
    constexpr const char* fragmentShader = R"(
        #version 410 core
        in vec3 vColor;
        layout(location=0) out vec4 outColor;
        void main(){ outColor=vec4(vColor,1.0); }
    )";

    try {
        codea::Window window{1000, 700, "Chapter 10 - scene camera culling"};
        const codea::Program program{vertexShader, fragmentShader};
        const codea::Mesh cube{codea::cubeVertices(), 6, {{0, 3, 0}}, codea::cubeIndices()};
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        std::vector<glm::vec3> centers;
        for (int z = -12; z <= 12; z += 3) {
            for (int x = -15; x <= 15; x += 3) {
                centers.emplace_back(static_cast<float>(x), 0.0f, static_cast<float>(z));
            }
        }

        while (!window.shouldClose()) {
            const auto [width, height] = window.framebufferSize();
            const float aspect = height > 0 ? static_cast<float>(width) / height : 1.0f;
            const float angle = static_cast<float>(window.time()) * 0.22f;
            const glm::vec3 cameraPosition{std::sin(angle) * 18.0f, 9.0f,
                                           std::cos(angle) * 18.0f};
            const glm::mat4 view = glm::lookAt(cameraPosition, glm::vec3{0},
                                              glm::vec3{0, 1, 0});
            const glm::mat4 projection = glm::perspective(glm::radians(50.0f), aspect,
                                                           0.1f, 45.0f);
            const auto frustum = extractFrustum(projection * view);

            glClearColor(0.025f, 0.03f, 0.045f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            program.use();
            int submitted = 0;
            for (const glm::vec3& center : centers) {
                if (!visible(center, 1.75f, frustum)) {
                    continue;
                }
                const glm::mat4 model = glm::translate(glm::mat4{1.0f}, center) *
                                        glm::scale(glm::mat4{1.0f}, glm::vec3{0.65f});
                const glm::mat4 mvp = projection * view * model;
                const glm::vec3 color = glm::vec3{0.25f, 0.65f, 0.95f} *
                                        (0.7f + 0.3f * ((center.x + 15.0f) / 30.0f));
                glUniformMatrix4fv(program.uniform("uMvp"), 1, GL_FALSE, &mvp[0][0]);
                glUniform3fv(program.uniform("uColor"), 1, &color[0]);
                cube.draw();
                ++submitted;
            }
            window.setTitle("Chapter 10 - total " + std::to_string(centers.size()) +
                            ", submitted " + std::to_string(submitted));
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
