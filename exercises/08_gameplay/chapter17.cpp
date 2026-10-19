#include "geometry.hpp"
#include "gl_support.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

struct PlayerIntent {
    glm::vec2 move{0.0f};
};

struct GameWorld {
    glm::vec3 previousPosition{0.0f};
    glm::vec3 currentPosition{0.0f};

    void fixedUpdate(const PlayerIntent& intent, float fixedDelta) {
        previousPosition = currentPosition;
        glm::vec2 direction = intent.move;
        if (glm::length(direction) > 1.0f) {
            direction = glm::normalize(direction);
        }
        currentPosition += glm::vec3{direction.x, 0.0f, -direction.y} *
                           3.0f * fixedDelta;
        currentPosition.x = glm::clamp(currentPosition.x, -5.0f, 5.0f);
        currentPosition.z = glm::clamp(currentPosition.z, -5.0f, 5.0f);
    }
};

struct RenderObjectSnapshot {
    glm::mat4 model{1.0f};
    glm::vec3 color{1.0f};
};

struct RenderSnapshot {
    glm::mat4 viewProjection{1.0f};
    std::vector<RenderObjectSnapshot> objects;
};

PlayerIntent readIntent(GLFWwindow* window) {
    PlayerIntent intent;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) intent.move.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) intent.move.x += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) intent.move.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) intent.move.y -= 1.0f;
    return intent;
}

RenderSnapshot buildSnapshot(const GameWorld& world, float alpha, float aspect) {
    const glm::vec3 displayedPosition = glm::mix(world.previousPosition,
                                                 world.currentPosition, alpha);
    const glm::mat4 view = glm::lookAt(glm::vec3{0.0f, 8.0f, 10.0f},
                                      glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 projection = glm::perspective(glm::radians(48.0f), aspect,
                                                   0.1f, 100.0f);
    RenderSnapshot snapshot;
    snapshot.viewProjection = projection * view;
    snapshot.objects.push_back({
        glm::translate(glm::mat4{1.0f}, displayedPosition) *
            glm::scale(glm::mat4{1.0f}, glm::vec3{0.5f}),
        {0.15f, 0.72f, 0.95f}});
    snapshot.objects.push_back({
        glm::translate(glm::mat4{1.0f}, {0.0f, -0.65f, 0.0f}) *
            glm::scale(glm::mat4{1.0f}, {6.0f, 0.15f, 6.0f}),
        {0.22f, 0.27f, 0.30f}});
    return snapshot;
}

void printAccumulatorExample() {
    constexpr double fixedDelta = 0.016;
    double accumulator = 0.0;
    for (const double frameDelta : {0.010, 0.025, 0.008}) {
        accumulator += frameDelta;
        int ticks = 0;
        while (accumulator >= fixedDelta) {
            accumulator -= fixedDelta;
            ++ticks;
        }
        std::cout << "delta=" << frameDelta << " ticks=" << ticks
                  << " remaining=" << accumulator
                  << " alpha=" << accumulator / fixedDelta << '\n';
    }
}

int main(int argc, char** argv) {
    printAccumulatorExample();
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
        codea::Window window{1000, 700,
                             "Chapter 17 - WASD, P pause, N single step"};
        const codea::Program program{vertexShader, fragmentShader};
        const codea::Mesh cube{codea::cubeVertices(), 6, {{0, 3, 0}},
                               codea::cubeIndices()};
        GameWorld world;
        constexpr double fixedDelta = 1.0 / 60.0;
        double previousTime = window.time();
        double accumulator = 0.0;
        bool paused = false;
        bool pauseWasPressed = false;
        bool stepWasPressed = false;
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        while (!window.shouldClose()) {
            const double now = window.time();
            const double frameDelta = std::min(now - previousTime, 0.25);
            previousTime = now;
            const bool pausePressed = glfwGetKey(window.handle(), GLFW_KEY_P) == GLFW_PRESS;
            if (pausePressed && !pauseWasPressed) {
                paused = !paused;
                accumulator = 0.0;
            }
            pauseWasPressed = pausePressed;
            const bool stepPressed = glfwGetKey(window.handle(), GLFW_KEY_N) == GLFW_PRESS;
            const bool requestStep = paused && stepPressed && !stepWasPressed;
            stepWasPressed = stepPressed;
            const PlayerIntent intent = readIntent(window.handle());

            if (!paused) {
                accumulator += frameDelta;
            }
            int fixedSteps = 0;
            while (accumulator >= fixedDelta && fixedSteps < 8) {
                world.fixedUpdate(intent, static_cast<float>(fixedDelta));
                accumulator -= fixedDelta;
                ++fixedSteps;
            }
            if (fixedSteps == 8 && accumulator >= fixedDelta) {
                std::cerr << "Fixed-update backlog dropped\n";
                accumulator = 0.0;
            }
            if (requestStep) {
                world.fixedUpdate(intent, static_cast<float>(fixedDelta));
            }

            const auto [width, height] = window.framebufferSize();
            const float aspect = height > 0 ? static_cast<float>(width) / height : 1.0f;
            const float alpha = paused ? 1.0f
                                       : static_cast<float>(accumulator / fixedDelta);
            const RenderSnapshot snapshot = buildSnapshot(world, alpha, aspect);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glClearColor(0.025f, 0.03f, 0.045f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            program.use();
            for (const RenderObjectSnapshot& object : snapshot.objects) {
                const glm::mat4 mvp = snapshot.viewProjection * object.model;
                glUniformMatrix4fv(program.uniform("uMvp"), 1, GL_FALSE, &mvp[0][0]);
                glUniform3fv(program.uniform("uColor"), 1, &object.color[0]);
                cube.draw();
            }
            window.setTitle(std::string{"Chapter 17 - "} +
                            (paused ? "PAUSED" : "PLAYING") +
                            " | WASD, P pause, N step | fixed steps " +
                            std::to_string(fixedSteps));
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
