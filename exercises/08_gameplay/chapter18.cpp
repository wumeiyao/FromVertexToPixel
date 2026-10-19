#include "audio.hpp"
#include "geometry.hpp"
#include "gl_support.hpp"
#include "image.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

class MeshCache {
public:
    std::shared_ptr<codea::Mesh> loadCube(const std::string& key) {
        const auto found = meshes_.find(key);
        if (found != meshes_.end()) {
            return found->second;
        }
        auto mesh = std::make_shared<codea::Mesh>(
            codea::cubeVertices(), 6, std::vector<codea::Attribute>{{0, 3, 0}},
            codea::cubeIndices());
        meshes_.emplace(key, mesh);
        ++createdResources_;
        return mesh;
    }

    std::size_t size() const { return meshes_.size(); }
    int createdResources() const { return createdResources_; }

private:
    std::unordered_map<std::string, std::shared_ptr<codea::Mesh>> meshes_;
    int createdResources_ = 0;
};

struct DynamicBody {
    glm::vec3 previousPosition{0.0f, 5.0f, 0.0f};
    glm::vec3 position{0.0f, 5.0f, 0.0f};
    glm::vec3 velocity{0.8f, 0.0f, 0.3f};
};

bool fixedPhysicsStep(DynamicBody& body, float delta) {
    body.previousPosition = body.position;
    body.velocity.y += -9.81f * delta;
    body.position += body.velocity * delta;
    bool collided = false;
    if (body.position.y < 0.5f) {
        body.position.y = 0.5f;
        if (body.velocity.y < -0.8f) collided = true;
        body.velocity.y = -body.velocity.y * 0.62f;
        body.velocity.x *= 0.94f;
        body.velocity.z *= 0.94f;
    }
    for (int axis : {0, 2}) {
        if (body.position[axis] < -4.5f || body.position[axis] > 4.5f) {
            body.position[axis] = glm::clamp(body.position[axis], -4.5f, 4.5f);
            body.velocity[axis] = -body.velocity[axis];
        }
    }
    return collided;
}

int main(int argc, char** argv) {
    DynamicBody checkBody;
    int ticks = 0;
    while (checkBody.position.y > 0.5f && ticks < 600) {
        fixedPhysicsStep(checkBody, 1.0f / 60.0f);
        ++ticks;
    }
    std::cout << "chapter18 check: dynamic box first contacts ground after " << ticks
              << " fixed ticks; render mesh and collision extent are separate data\n";
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
        codea::Window window{1000, 700, "Chapter 18 - physics, audio, shared asset"};
        const codea::Program program{vertexShader, fragmentShader};
        MeshCache assets;
        const auto dynamicMesh = assets.loadCube("shared-cube");
        const auto groundMesh = assets.loadCube("shared-cube");
        codea::AudioSystem audio{codea::outputPath("chapter18_collision.wav")};
        std::cout << "Two entities share " << assets.size() << " GPU mesh; created resources = "
                  << assets.createdResources() << '\n';

        DynamicBody body;
        constexpr double fixedDelta = 1.0 / 60.0;
        double previousTime = window.time();
        double accumulator = 0.0;
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        while (!window.shouldClose()) {
            const double now = window.time();
            accumulator += std::min(now - previousTime, 0.25);
            previousTime = now;
            while (accumulator >= fixedDelta) {
                if (fixedPhysicsStep(body, static_cast<float>(fixedDelta))) {
                    audio.playOneShot();
                }
                accumulator -= fixedDelta;
            }
            const float alpha = static_cast<float>(accumulator / fixedDelta);
            const glm::vec3 displayedPosition = glm::mix(body.previousPosition,
                                                         body.position, alpha);
            const auto [width, height] = window.framebufferSize();
            const float aspect = height > 0 ? static_cast<float>(width) / height : 1.0f;
            const glm::mat4 view = glm::lookAt(glm::vec3{7.0f, 5.5f, 9.0f},
                                              glm::vec3{0}, glm::vec3{0, 1, 0});
            const glm::mat4 projection = glm::perspective(glm::radians(50.0f), aspect,
                                                           0.1f, 100.0f);
            const glm::mat4 viewProjection = projection * view;

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glClearColor(0.025f, 0.032f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            program.use();

            const auto draw = [&](const codea::Mesh& mesh, const glm::mat4& model,
                                  const glm::vec3& color) {
                const glm::mat4 mvp = viewProjection * model;
                glUniformMatrix4fv(program.uniform("uMvp"), 1, GL_FALSE, &mvp[0][0]);
                glUniform3fv(program.uniform("uColor"), 1, &color[0]);
                mesh.draw();
            };
            draw(*groundMesh,
                 glm::translate(glm::mat4{1.0f}, {0, -0.3f, 0}) *
                     glm::scale(glm::mat4{1.0f}, {5.0f, 0.3f, 5.0f}),
                 {0.25f, 0.30f, 0.32f});
            draw(*dynamicMesh,
                 glm::translate(glm::mat4{1.0f}, displayedPosition) *
                     glm::scale(glm::mat4{1.0f}, glm::vec3{0.5f}),
                 {0.92f, 0.38f, 0.12f});
            window.setTitle("Chapter 18 - shared meshes " +
                            std::to_string(assets.size()) + " | collision plays WAV");
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
