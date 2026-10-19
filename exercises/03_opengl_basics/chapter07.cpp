#include "gl_support.hpp"

#include <cmath>
#include <iostream>

int main(int argc, char** argv) {
    if (codea::checkOnly(argc, argv)) {
        std::cout << "chapter07 check: GLFW window -> OpenGL context -> clear -> present\n";
        return 0;
    }

    try {
        codea::Window window{800, 600, "Chapter 07 - OpenGL context and clear"};
        codea::printOpenGlInfo();

        while (!window.shouldClose()) {
            const float pulse = static_cast<float>(std::sin(window.time()) * 0.5 + 0.5);
            glClearColor(0.05f + pulse * 0.08f, 0.08f, 0.14f + pulse * 0.18f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            window.present();
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
