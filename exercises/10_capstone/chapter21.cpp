#include "audio.hpp"
#include "geometry.hpp"
#include "gl_support.hpp"
#include "image.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

enum class GameState {
    Title,
    Playing,
    Paused,
    Finished
};

const char* stateName(GameState state) {
    switch (state) {
        case GameState::Title: return "TITLE - press Enter";
        case GameState::Playing: return "PLAYING";
        case GameState::Paused: return "PAUSED";
        case GameState::Finished: return "FINISHED - press R";
    }
    return "UNKNOWN";
}

struct PlayerIntent {
    glm::vec2 move{0.0f};
};

struct Collectible {
    glm::vec3 position;
    bool active = true;
};

struct Obstacle {
    glm::vec3 center;
    glm::vec2 halfExtent;
};

class GameWorld {
public:
    GameWorld() { reset(); }

    void reset() {
        state = GameState::Title;
        previousPlayer = {-4.5f, 0.5f, -4.5f};
        player = previousPlayer;
        score = 0;
        collectibles = {
            {{-3.5f, 0.45f,  3.5f}, true},
            {{ 0.0f, 0.45f, -3.5f}, true},
            {{ 3.7f, 0.45f,  3.5f}, true},
            {{ 4.0f, 0.45f, -1.8f}, true},
            {{-1.5f, 0.45f,  1.8f}, true}};
        obstacles = {
            {{0.0f, 0.6f, 0.0f}, {0.8f, 2.4f}},
            {{2.2f, 0.6f, 0.8f}, {0.7f, 1.2f}},
            {{-2.5f, 0.6f, -1.0f}, {1.1f, 0.7f}}};
    }

    void start() {
        if (state == GameState::Title) state = GameState::Playing;
    }

    void togglePause() {
        if (state == GameState::Playing) state = GameState::Paused;
        else if (state == GameState::Paused) state = GameState::Playing;
    }

    int fixedUpdate(const PlayerIntent& intent, float delta) {
        if (state != GameState::Playing) return 0;
        previousPlayer = player;
        glm::vec2 direction = intent.move;
        if (glm::length(direction) > 1.0f) direction = glm::normalize(direction);
        glm::vec3 candidate = player + glm::vec3{direction.x, 0.0f, -direction.y} *
                                      3.2f * delta;
        candidate.x = glm::clamp(candidate.x, -5.3f, 5.3f);
        candidate.z = glm::clamp(candidate.z, -5.3f, 5.3f);
        if (!collidesWithObstacle(candidate)) player = candidate;

        int collectedThisTick = 0;
        for (Collectible& collectible : collectibles) {
            const glm::vec2 offset{player.x - collectible.position.x,
                                   player.z - collectible.position.z};
            if (collectible.active && glm::dot(offset, offset) < 0.55f * 0.55f) {
                collectible.active = false;
                ++score;
                ++collectedThisTick;
            }
        }
        if (score == static_cast<int>(collectibles.size())) state = GameState::Finished;
        return collectedThisTick;
    }

    bool collidesWithObstacle(const glm::vec3& candidate) const {
        constexpr float playerRadius = 0.38f;
        for (const Obstacle& obstacle : obstacles) {
            if (std::abs(candidate.x - obstacle.center.x) <
                    obstacle.halfExtent.x + playerRadius &&
                std::abs(candidate.z - obstacle.center.z) <
                    obstacle.halfExtent.y + playerRadius) {
                return true;
            }
        }
        return false;
    }

    GameState state = GameState::Title;
    glm::vec3 previousPlayer{0.0f};
    glm::vec3 player{0.0f};
    int score = 0;
    std::vector<Collectible> collectibles;
    std::vector<Obstacle> obstacles;
};

struct RenderObject {
    glm::mat4 model{1.0f};
    glm::vec3 color{1.0f};
};

struct RenderSnapshot {
    glm::mat4 viewProjection{1.0f};
    std::vector<RenderObject> objects;
    GameState state = GameState::Title;
    int score = 0;
    int scoreTarget = 0;
};

RenderSnapshot buildSnapshot(const GameWorld& world, float alpha, float aspect,
                             float animationTime) {
    RenderSnapshot snapshot;
    const glm::mat4 view = glm::lookAt(glm::vec3{0.0f, 10.5f, 11.0f},
                                      glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 projection = glm::perspective(glm::radians(50.0f), aspect,
                                                   0.1f, 100.0f);
    snapshot.viewProjection = projection * view;
    snapshot.state = world.state;
    snapshot.score = world.score;
    snapshot.scoreTarget = static_cast<int>(world.collectibles.size());

    snapshot.objects.push_back({
        glm::translate(glm::mat4{1.0f}, {0, -0.35f, 0}) *
            glm::scale(glm::mat4{1.0f}, {6.0f, 0.35f, 6.0f}),
        {0.16f, 0.23f, 0.20f}});
    for (const Obstacle& obstacle : world.obstacles) {
        snapshot.objects.push_back({
            glm::translate(glm::mat4{1.0f}, obstacle.center) *
                glm::scale(glm::mat4{1.0f},
                           {obstacle.halfExtent.x, 0.65f, obstacle.halfExtent.y}),
            {0.38f, 0.42f, 0.46f}});
    }

    const glm::vec3 displayedPlayer = glm::mix(world.previousPlayer, world.player, alpha);
    snapshot.objects.push_back({
        glm::translate(glm::mat4{1.0f}, displayedPlayer) *
            glm::scale(glm::mat4{1.0f}, glm::vec3{0.42f}),
        {0.12f, 0.62f, 0.96f}});
    for (const Collectible& collectible : world.collectibles) {
        if (!collectible.active) continue;
        snapshot.objects.push_back({
            glm::translate(glm::mat4{1.0f}, collectible.position) *
                glm::rotate(glm::mat4{1.0f}, animationTime * 1.8f,
                            glm::vec3{0, 1, 0}) *
                glm::scale(glm::mat4{1.0f}, glm::vec3{0.28f}),
            {1.0f, 0.64f, 0.08f}});
    }
    return snapshot;
}

PlayerIntent readPlayerIntent(GLFWwindow* window) {
    PlayerIntent intent;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) intent.move.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) intent.move.x += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) intent.move.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) intent.move.y -= 1.0f;
    return intent;
}

bool pressedOnce(GLFWwindow* window, int key, bool& previous) {
    const bool current = glfwGetKey(window, key) == GLFW_PRESS;
    const bool result = current && !previous;
    previous = current;
    return result;
}

void verifyGameLoop() {
    GameWorld world;
    world.start();
    const std::vector<Collectible> checkCollectibles = world.collectibles;
    for (const Collectible& collectible : checkCollectibles) {
        world.previousPlayer = collectible.position;
        world.player = collectible.position;
        world.fixedUpdate({}, 1.0f / 60.0f);
    }
    std::cout << "chapter21 check: collected=" << world.score
              << " state=" << stateName(world.state) << '\n';
    if (world.state != GameState::Finished) {
        throw std::runtime_error("Capstone state-loop verification failed");
    }
}

int main(int argc, char** argv) {
    try {
        verifyGameLoop();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    if (codea::checkOnly(argc, argv)) return 0;

    constexpr const char* sceneVertex = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        layout(location=1) in vec3 aNormal;
        uniform mat4 uModel;
        uniform mat4 uViewProjection;
        uniform mat3 uNormalMatrix;
        out vec3 vNormal;
        void main(){
            vNormal=normalize(uNormalMatrix*aNormal);
            gl_Position=uViewProjection*uModel*vec4(aPosition,1.0);
        }
    )";
    constexpr const char* sceneFragment = R"(
        #version 410 core
        in vec3 vNormal;
        uniform vec3 uColor;
        layout(location=0) out vec4 outColor;
        void main(){
            float light=0.18+max(dot(normalize(vNormal),normalize(vec3(0.45,1.0,0.25))),0.0);
            outColor=vec4(uColor*light,1.0);
        }
    )";
    constexpr const char* uiVertex = R"(
        #version 410 core
        layout(location=0) in vec2 aUnitPosition;
        uniform vec4 uRect;
        void main(){ gl_Position=vec4(uRect.xy+aUnitPosition*uRect.zw,0.0,1.0); }
    )";
    constexpr const char* uiFragment = R"(
        #version 410 core
        uniform vec4 uColor;
        layout(location=0) out vec4 outColor;
        void main(){ outColor=uColor; }
    )";

    try {
        codea::Window window{1100, 760,
                             "RenderQuest - Enter start, WASD move, P pause, R restart"};
        const codea::Program sceneProgram{sceneVertex, sceneFragment};
        const codea::Program uiProgram{uiVertex, uiFragment};
        const codea::Mesh cube{codea::cubeVertices(), 6,
                               {{0, 3, 0}, {1, 3, 3 * sizeof(float)}},
                               codea::cubeIndices()};
        const std::vector<float> unitQuad{0,0, 1,0, 1,1, 0,0, 1,1, 0,1};
        const codea::Mesh quad{unitQuad, 2, {{0, 2, 0}}};
        codea::AudioSystem audio{codea::outputPath("chapter21_collect.wav")};
        GameWorld world;
        constexpr double fixedDelta = 1.0 / 60.0;
        double previousTime = window.time();
        double accumulator = 0.0;
        bool enterWasPressed = false;
        bool pauseWasPressed = false;
        bool restartWasPressed = false;

        while (!window.shouldClose()) {
            const double now = window.time();
            const double frameDelta = std::min(now - previousTime, 0.25);
            previousTime = now;
            if (pressedOnce(window.handle(), GLFW_KEY_ENTER, enterWasPressed)) world.start();
            if (pressedOnce(window.handle(), GLFW_KEY_P, pauseWasPressed)) {
                world.togglePause();
                accumulator = 0.0;
            }
            if (pressedOnce(window.handle(), GLFW_KEY_R, restartWasPressed)) {
                world.reset();
                accumulator = 0.0;
            }

            if (world.state == GameState::Playing) accumulator += frameDelta;
            const PlayerIntent intent = readPlayerIntent(window.handle());
            int fixedSteps = 0;
            while (accumulator >= fixedDelta && fixedSteps < 8) {
                const int collected = world.fixedUpdate(intent, static_cast<float>(fixedDelta));
                for (int i = 0; i < collected; ++i) audio.playOneShot();
                accumulator -= fixedDelta;
                ++fixedSteps;
            }
            if (fixedSteps == 8 && accumulator >= fixedDelta) accumulator = 0.0;

            const auto [width, height] = window.framebufferSize();
            if (width <= 0 || height <= 0) {
                window.present();
                continue;
            }
            const float aspect = static_cast<float>(width) / height;
            const float alpha = world.state == GameState::Playing
                                    ? static_cast<float>(accumulator / fixedDelta) : 1.0f;
            const RenderSnapshot snapshot = buildSnapshot(
                world, alpha, aspect, static_cast<float>(now));

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glClearColor(0.018f, 0.026f, 0.038f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            sceneProgram.use();
            glUniformMatrix4fv(sceneProgram.uniform("uViewProjection"), 1, GL_FALSE,
                               &snapshot.viewProjection[0][0]);
            for (const RenderObject& object : snapshot.objects) {
                const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3{object.model});
                glUniformMatrix4fv(sceneProgram.uniform("uModel"), 1, GL_FALSE,
                                   &object.model[0][0]);
                glUniformMatrix3fv(sceneProgram.uniform("uNormalMatrix"), 1, GL_FALSE,
                                   &normalMatrix[0][0]);
                glUniform3fv(sceneProgram.uniform("uColor"), 1, &object.color[0]);
                cube.draw();
            }

            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            uiProgram.use();
            const glm::vec4 stateColors[] = {
                {0.95f, 0.72f, 0.10f, 0.92f}, {0.12f, 0.78f, 0.38f, 0.92f},
                {0.90f, 0.22f, 0.12f, 0.92f}, {0.10f, 0.75f, 0.90f, 0.92f}};
            const int stateIndex = static_cast<int>(snapshot.state);
            glm::vec4 rect{-0.96f, 0.86f, 0.28f, 0.08f};
            glUniform4fv(uiProgram.uniform("uRect"), 1, &rect[0]);
            glUniform4fv(uiProgram.uniform("uColor"), 1, &stateColors[stateIndex][0]);
            quad.draw();
            for (int i = 0; i < snapshot.scoreTarget; ++i) {
                rect = {-0.96f + i * 0.075f, 0.73f, 0.055f, 0.075f};
                const glm::vec4 color = i < snapshot.score
                                            ? glm::vec4{1.0f, 0.65f, 0.08f, 0.95f}
                                            : glm::vec4{0.25f, 0.28f, 0.32f, 0.8f};
                glUniform4fv(uiProgram.uniform("uRect"), 1, &rect[0]);
                glUniform4fv(uiProgram.uniform("uColor"), 1, &color[0]);
                quad.draw();
            }
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);

            window.setTitle(std::string{"RenderQuest | "} + stateName(snapshot.state) +
                            " | score " + std::to_string(snapshot.score) + "/" +
                            std::to_string(snapshot.scoreTarget) +
                            " | Enter/WASD/P/R");
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
