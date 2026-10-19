#include "gl_support.hpp"

#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    if (codea::checkOnly(argc, argv)) {
        std::cout << "chapter08 check: vertex buffer -> attributes -> shaders -> draw call\n";
        return 0;
    }

    constexpr const char* vertexShader = R"(
        #version 410 core
        layout(location = 0) in vec2 aPosition;
        layout(location = 1) in vec3 aColor;
        out vec3 vColor;
        void main() {
            vColor = aColor;
            gl_Position = vec4(aPosition, 0.0, 1.0);
        }
    )";
    constexpr const char* fragmentShader = R"(
        #version 410 core
        in vec3 vColor;
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(vColor, 1.0);
        }
    )";

    try {
        codea::Window window{800, 600, "Chapter 08 - first GPU triangle"};
        const codea::Program program{vertexShader, fragmentShader};
        const std::vector<float> vertices{
             0.0f,  0.75f, 1.0f, 0.15f, 0.12f,
            -0.7f, -0.55f, 0.1f, 0.85f, 0.25f,
             0.7f, -0.55f, 0.1f, 0.35f, 1.0f};
        const codea::Mesh triangle{vertices, 5, {{0, 2, 0}, {1, 3, 2 * sizeof(float)}}};

        while (!window.shouldClose()) {
            glClearColor(0.025f, 0.03f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            program.use();
            triangle.draw();
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
