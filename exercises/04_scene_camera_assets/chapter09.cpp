#include "gl_support.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct ObjMeshData {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
};

ObjMeshData loadObj(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open model: " + path);
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    ObjMeshData result;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string type;
        stream >> type;
        if (type == "v") {
            glm::vec3 value;
            stream >> value.x >> value.y >> value.z;
            positions.push_back(value);
        } else if (type == "vn") {
            glm::vec3 value;
            stream >> value.x >> value.y >> value.z;
            normals.push_back(value);
        } else if (type == "f") {
            std::vector<std::uint32_t> face;
            std::string token;
            while (stream >> token) {
                const auto separator = token.find("//");
                if (separator == std::string::npos) {
                    throw std::runtime_error("This sample expects OBJ position//normal faces");
                }
                const int positionIndex = std::stoi(token.substr(0, separator)) - 1;
                const int normalIndex = std::stoi(token.substr(separator + 2)) - 1;
                const glm::vec3 position = positions.at(positionIndex);
                const glm::vec3 normal = normals.at(normalIndex);
                result.vertices.insert(result.vertices.end(),
                                       {position.x, position.y, position.z,
                                        normal.x, normal.y, normal.z});
                face.push_back(static_cast<std::uint32_t>(result.vertices.size() / 6 - 1));
            }
            for (std::size_t i = 1; i + 1 < face.size(); ++i) {
                result.indices.insert(result.indices.end(), {face[0], face[i], face[i + 1]});
            }
        }
    }
    if (result.indices.empty()) {
        throw std::runtime_error("Model contains no drawable faces: " + path);
    }
    return result;
}

int main(int argc, char** argv) {
    const std::string modelPath = std::string{CODEA_EXERCISE_DIR} +
                                  "/04_scene_camera_assets/chapter09_pyramid.obj";
    const ObjMeshData cpuMesh = loadObj(modelPath);
    std::cout << "Loaded " << modelPath << ": " << cpuMesh.vertices.size() / 6
              << " vertices, " << cpuMesh.indices.size() / 3 << " triangles\n";
    if (codea::checkOnly(argc, argv)) {
        return 0;
    }

    constexpr const char* vertexShader = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        layout(location=1) in vec3 aNormal;
        uniform mat4 uMvp;
        out vec3 vNormal;
        void main() {
            vNormal = aNormal;
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";
    constexpr const char* fragmentShader = R"(
        #version 410 core
        in vec3 vNormal;
        layout(location=0) out vec4 outColor;
        void main() {
            outColor = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
        }
    )";

    try {
        codea::Window window{900, 650, "Chapter 09 - model asset to draw call"};
        const codea::Program program{vertexShader, fragmentShader};
        const codea::Mesh mesh{cpuMesh.vertices, 6,
                               {{0, 3, 0}, {1, 3, 3 * sizeof(float)}}, cpuMesh.indices};
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        while (!window.shouldClose()) {
            const auto [width, height] = window.framebufferSize();
            const float aspect = height > 0 ? static_cast<float>(width) / height : 1.0f;
            const glm::mat4 model = glm::rotate(glm::mat4{1.0f},
                                                static_cast<float>(window.time()) * 0.5f,
                                                glm::vec3{0.0f, 1.0f, 0.0f});
            const glm::mat4 view = glm::lookAt(glm::vec3{0.0f, 1.7f, 4.0f},
                                              glm::vec3{0.0f, 0.5f, 0.0f},
                                              glm::vec3{0.0f, 1.0f, 0.0f});
            const glm::mat4 projection = glm::perspective(glm::radians(55.0f), aspect,
                                                           0.1f, 100.0f);
            const glm::mat4 mvp = projection * view * model;

            glClearColor(0.025f, 0.035f, 0.055f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            program.use();
            glUniformMatrix4fv(program.uniform("uMvp"), 1, GL_FALSE, &mvp[0][0]);
            mesh.draw();
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
