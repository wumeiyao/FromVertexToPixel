#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iomanip>
#include <iostream>

void print(const char* label, const glm::vec4& value) {
    std::cout << std::left << std::setw(14) << label << " = ("
              << value.x << ", " << value.y << ", " << value.z << ", "
              << value.w << ")\n";
}

int main() {
    const glm::vec4 localPosition{1.0f, 1.0f, 0.0f, 1.0f};
    const glm::mat4 model = glm::translate(glm::mat4{1.0f}, {2.0f, 0.0f, 0.0f});
    const glm::mat4 view = glm::lookAt(glm::vec3{0.0f, 0.0f, 5.0f},
                                      glm::vec3{0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f,
                                                   0.1f, 100.0f);

    const glm::vec4 worldPosition = model * localPosition;
    const glm::vec4 viewPosition = view * worldPosition;
    const glm::vec4 clipPosition = projection * viewPosition;
    const glm::vec3 ndc = glm::vec3{clipPosition} / clipPosition.w;

    std::cout << std::fixed << std::setprecision(3);
    print("Local", localPosition);
    print("World", worldPosition);
    print("View", viewPosition);
    print("Clip", clipPosition);
    std::cout << "NDC            = (" << ndc.x << ", " << ndc.y << ", " << ndc.z
              << ")\nScreen 1280x720 = (" << (ndc.x * 0.5f + 0.5f) * 1280.0f
              << ", " << (1.0f - (ndc.y * 0.5f + 0.5f)) * 720.0f << ")\n";
    return 0;
}
