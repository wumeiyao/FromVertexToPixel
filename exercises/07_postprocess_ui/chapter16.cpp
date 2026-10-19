#include "geometry.hpp"
#include "gl_support.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct DebugControls {
    bool depth = true;
    bool culling = true;
    bool rotation = true;
    bool overlay = true;
};

int main(int argc, char** argv) {
    const glm::vec2 centerNdc = glm::vec2{640.0f / 1280.0f, 360.0f / 720.0f} *
                                2.0f - 1.0f;
    std::cout << "chapter16 check: pixel (640,360) in 1280x720 -> NDC ("
              << centerNdc.x << ", " << centerNdc.y << ")\n";
    if (codea::checkOnly(argc, argv)) {
        return 0;
    }

    constexpr const char* sceneVertex = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        layout(location=1) in vec3 aNormal;
        uniform mat4 uMvp;
        out vec3 vNormal;
        void main(){ vNormal=aNormal; gl_Position=uMvp*vec4(aPosition,1.0); }
    )";
    constexpr const char* sceneFragment = R"(
        #version 410 core
        in vec3 vNormal;
        layout(location=0) out vec4 outColor;
        void main(){ outColor=vec4(normalize(vNormal)*0.5+0.5,1.0); }
    )";
    constexpr const char* uiVertex = R"(
        #version 410 core
        layout(location=0) in vec2 aUnitPosition;
        uniform vec4 uRect;
        void main(){
            vec2 pixel=uRect.xy+aUnitPosition*uRect.zw;
            gl_Position=vec4(pixel,0.0,1.0);
        }
    )";
    constexpr const char* uiFragment = R"(
        #version 410 core
        uniform vec4 uColor;
        layout(location=0) out vec4 outColor;
        void main(){ outColor=uColor; }
    )";

    try {
        codea::Window window{1000, 700, "Chapter 16 - debug overlay (keys 1-4)"};
        const codea::Program sceneProgram{sceneVertex, sceneFragment};
        const codea::Program uiProgram{uiVertex, uiFragment};
        const codea::Mesh cube{codea::cubeVertices(), 6,
                               {{0, 3, 0}, {1, 3, 3 * sizeof(float)}},
                               codea::cubeIndices()};
        const std::vector<float> unitQuad{0,0, 1,0, 1,1, 0,0, 1,1, 0,1};
        const codea::Mesh quad{unitQuad, 2, {{0, 2, 0}}};
        DebugControls controls;
        std::array<bool, 4> wasPressed{};
        double previousTime = window.time();
        double titleTimer = 0.0;
        int frameCount = 0;
        double fps = 0.0;

        while (!window.shouldClose()) {
            const double now = window.time();
            const double delta = now - previousTime;
            previousTime = now;
            titleTimer += delta;
            ++frameCount;

            const std::array<int, 4> keys{GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4};
            std::array<bool*, 4> values{&controls.depth, &controls.culling,
                                        &controls.rotation, &controls.overlay};
            const std::array<const char*, 4> names{"Depth", "Cull", "Rotate", "Overlay"};
            for (std::size_t i = 0; i < keys.size(); ++i) {
                const bool pressed = glfwGetKey(window.handle(), keys[i]) == GLFW_PRESS;
                if (pressed && !wasPressed[i]) {
                    *values[i] = !*values[i];
                    std::cout << "Debug control " << names[i] << " = "
                              << std::boolalpha << *values[i] << '\n';
                }
                wasPressed[i] = pressed;
            }

            const auto [width, height] = window.framebufferSize();
            if (width <= 0 || height <= 0) {
                window.present();
                continue;
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            controls.depth ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            controls.culling ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0.025f, 0.03f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const float aspect = static_cast<float>(width) / height;
            const float angle = controls.rotation ? static_cast<float>(now) * 0.55f : 0.5f;
            const glm::mat4 model = glm::rotate(glm::mat4{1.0f}, angle,
                                                glm::vec3{0, 1, 0});
            const glm::mat4 view = glm::lookAt(glm::vec3{0, 2.5f, 6.0f},
                                              glm::vec3{0}, glm::vec3{0, 1, 0});
            const glm::mat4 projection = glm::perspective(glm::radians(52.0f), aspect,
                                                           0.1f, 100.0f);
            const glm::mat4 mvp = projection * view * model;
            sceneProgram.use();
            glUniformMatrix4fv(sceneProgram.uniform("uMvp"), 1, GL_FALSE, &mvp[0][0]);
            cube.draw();

            if (controls.overlay) {
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_SCISSOR_TEST);
                glScissor(12, height - 78, 310, 66);
                uiProgram.use();
                for (std::size_t i = 0; i < values.size(); ++i) {
                    const float x = -0.97f + static_cast<float>(i) * 0.145f;
                    const glm::vec4 rect{x, 0.82f, 0.11f, 0.11f};
                    const glm::vec4 color = *values[i]
                                                ? glm::vec4{0.15f, 0.85f, 0.42f, 0.88f}
                                                : glm::vec4{0.85f, 0.16f, 0.12f, 0.88f};
                    glUniform4fv(uiProgram.uniform("uRect"), 1, &rect[0]);
                    glUniform4fv(uiProgram.uniform("uColor"), 1, &color[0]);
                    quad.draw();
                }
                glDisable(GL_SCISSOR_TEST);
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }

            if (titleTimer >= 0.5) {
                fps = frameCount / titleTimer;
                titleTimer = 0.0;
                frameCount = 0;
                std::ostringstream title;
                title << std::fixed << std::setprecision(1)
                      << "Chapter 16 | FPS " << fps << " | 1 Depth " << controls.depth
                      << " 2 Cull " << controls.culling << " 3 Rotate "
                      << controls.rotation << " 4 UI " << controls.overlay;
                window.setTitle(title.str());
            }
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
