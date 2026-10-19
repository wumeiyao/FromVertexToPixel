#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iomanip>
#include <iostream>

int main() {
    const glm::vec4 local{1.0f, 0.0f, 0.0f, 1.0f};
    const glm::mat4 model = glm::translate(glm::mat4{1.0f}, {0.0f, 0.0f, -2.0f}) *
                            glm::rotate(glm::mat4{1.0f}, glm::radians(90.0f),
                                        glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 view = glm::lookAt(glm::vec3{0.0f, 1.0f, 5.0f},
                                      glm::vec3{0.0f, 0.0f, -2.0f},
                                      glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 4.0f / 3.0f,
                                                   0.1f, 100.0f);

    const glm::vec4 world = model * local;
    const glm::vec4 camera = view * world;
    const glm::vec4 clip = projection * camera;
    const glm::vec3 ndc = glm::vec3{clip} / clip.w;

    std::cout << std::fixed << std::setprecision(3)
              << "Local -> World -> View -> Clip -> NDC\n"
              << "World: (" << world.x << ", " << world.y << ", " << world.z << ")\n"
              << "View : (" << camera.x << ", " << camera.y << ", " << camera.z << ")\n"
              << "Clip : (" << clip.x << ", " << clip.y << ", " << clip.z << ", "
              << clip.w << ")\n"
              << "NDC  : (" << ndc.x << ", " << ndc.y << ", " << ndc.z << ")\n";
    return 0;
}
