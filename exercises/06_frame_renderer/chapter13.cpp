#include "geometry.hpp"
#include "gl_support.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

struct DrawItem {
    glm::vec3 position;
    glm::vec3 color;
    int materialId;
    float cameraDepth = 0.0f;
};

std::vector<DrawItem> buildOpaqueQueue(const glm::vec3& cameraPosition) {
    std::vector<DrawItem> queue{
        {{-4, 0, -3}, {0.85f, 0.22f, 0.18f}, 2},
        {{ 2, 0, -1}, {0.20f, 0.72f, 0.35f}, 1},
        {{-1, 0,  2}, {0.18f, 0.48f, 0.90f}, 0},
        {{ 4, 0,  3}, {0.85f, 0.22f, 0.18f}, 2},
        {{ 0, 0, -5}, {0.20f, 0.72f, 0.35f}, 1},
        {{-5, 0,  3}, {0.18f, 0.48f, 0.90f}, 0}};
    for (DrawItem& item : queue) {
        const glm::vec3 offset = item.position - cameraPosition;
        item.cameraDepth = glm::dot(offset, offset);
    }

    std::stable_sort(queue.begin(), queue.end(), [](const DrawItem& a, const DrawItem& b) {
        if (a.materialId != b.materialId) {
            return a.materialId < b.materialId;
        }
        return a.cameraDepth < b.cameraDepth;
    });
    return queue;
}

int countMaterialTransitions(const std::vector<DrawItem>& queue) {
    int transitions = 0;
    int previous = -1;
    for (const DrawItem& item : queue) {
        if (item.materialId != previous) {
            ++transitions;
            previous = item.materialId;
        }
    }
    return transitions;
}

void beginOpaquePass(int width, int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.025f, 0.032f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

int main(int argc, char** argv) {
    const glm::vec3 cameraPosition{0.0f, 5.0f, 12.0f};
    const std::vector<DrawItem> queue = buildOpaqueQueue(cameraPosition);
    std::cout << "chapter13 opaque queue order:";
    for (const DrawItem& item : queue) {
        std::cout << " M" << item.materialId;
    }
    std::cout << "; material binds = " << countMaterialTransitions(queue) << '\n';
    if (codea::checkOnly(argc, argv)) {
        return 0;
    }

    constexpr const char* vertexShader = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        uniform mat4 uMvp;
        void main(){ gl_Position=uMvp*vec4(aPosition,1.0); }
    )";
    constexpr const char* fragmentShader = R"(
        #version 410 core
        uniform vec3 uColor;
        layout(location=0) out vec4 outColor;
        void main(){ outColor=vec4(uColor,1.0); }
    )";

    try {
        codea::Window window{1000, 700, "Chapter 13 - explicit opaque pass and queue"};
        const codea::Program program{vertexShader, fragmentShader};
        const codea::Mesh cube{codea::cubeVertices(), 6, {{0, 3, 0}},
                               codea::cubeIndices()};

        while (!window.shouldClose()) {
            const auto [width, height] = window.framebufferSize();
            beginOpaquePass(width, height);
            const float aspect = height > 0 ? static_cast<float>(width) / height : 1.0f;
            const glm::mat4 view = glm::lookAt(cameraPosition, glm::vec3{0},
                                              glm::vec3{0, 1, 0});
            const glm::mat4 projection = glm::perspective(glm::radians(52.0f), aspect,
                                                           0.1f, 100.0f);
            program.use();
            for (const DrawItem& item : queue) {
                const glm::mat4 model = glm::translate(glm::mat4{1.0f}, item.position) *
                                        glm::rotate(glm::mat4{1.0f},
                                                    static_cast<float>(window.time()) * 0.25f,
                                                    glm::vec3{0, 1, 0}) *
                                        glm::scale(glm::mat4{1.0f}, glm::vec3{0.8f});
                const glm::mat4 mvp = projection * view * model;
                glUniformMatrix4fv(program.uniform("uMvp"), 1, GL_FALSE, &mvp[0][0]);
                glUniform3fv(program.uniform("uColor"), 1, &item.color[0]);
                cube.draw();
            }
            window.setTitle("Chapter 13 - Opaque pass: " +
                            std::to_string(queue.size()) + " draws, " +
                            std::to_string(countMaterialTransitions(queue)) +
                            " material binds");
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
