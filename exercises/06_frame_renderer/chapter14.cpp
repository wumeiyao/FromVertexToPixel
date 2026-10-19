#include "geometry.hpp"
#include "gl_support.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

GLuint createLabeledColorCubemap() {
    const std::array<std::array<unsigned char, 3>, 6> colors{{
        {{255, 80, 80}}, {{80, 255, 80}}, {{80, 80, 255}},
        {{255, 220, 80}}, {{80, 230, 230}}, {{230, 80, 230}}}};
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    for (int face = 0; face < 6; ++face) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB8, 1, 1, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, colors[static_cast<std::size_t>(face)].data());
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return texture;
}

int main(int argc, char** argv) {
    const glm::vec3 black{0.0f};
    const glm::vec3 farBlue = glm::vec3{0, 0, 1} * 0.5f + black * 0.5f;
    const glm::vec3 nearRed = glm::vec3{1, 0, 0} * 0.5f + farBlue * 0.5f;
    std::cout << "chapter14 check: far blue then near red = (" << nearRed.r << ", "
              << nearRed.g << ", " << nearRed.b
              << "); 100 instances = 1 draw, unchanged triangle count\n";
    if (codea::checkOnly(argc, argv)) {
        return 0;
    }

    constexpr const char* instanceVertex = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        layout(location=2) in mat4 aModel;
        uniform mat4 uViewProjection;
        out vec3 vColor;
        void main(){
            vec3 world=aModel[3].xyz;
            vColor=vec3(0.18,0.48,0.86)+vec3(world.x*0.015,0.0,world.z*0.012);
            gl_Position=uViewProjection*aModel*vec4(aPosition,1.0);
        }
    )";
    constexpr const char* instanceFragment = R"(
        #version 410 core
        in vec3 vColor;
        layout(location=0) out vec4 outColor;
        void main(){ outColor=vec4(vColor,1.0); }
    )";
    constexpr const char* skyVertex = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        uniform mat4 uSkyViewProjection;
        out vec3 vDirection;
        void main(){
            vDirection=aPosition;
            vec4 clip=uSkyViewProjection*vec4(aPosition,1.0);
            gl_Position=clip.xyww;
        }
    )";
    constexpr const char* skyFragment = R"(
        #version 410 core
        in vec3 vDirection;
        uniform samplerCube uSky;
        layout(location=0) out vec4 outColor;
        void main(){ outColor=vec4(texture(uSky,vDirection).rgb*0.35,1.0); }
    )";
    constexpr const char* transparentVertex = R"(
        #version 410 core
        layout(location=0) in vec3 aPosition;
        uniform mat4 uMvp;
        void main(){ gl_Position=uMvp*vec4(aPosition,1.0); }
    )";
    constexpr const char* transparentFragment = R"(
        #version 410 core
        uniform vec4 uColor;
        layout(location=0) out vec4 outColor;
        void main(){ outColor=uColor; }
    )";

    try {
        codea::Window window{1100, 720, "Chapter 14 - sky, transparency, instancing"};
        const codea::Program instanceProgram{instanceVertex, instanceFragment};
        const codea::Program skyProgram{skyVertex, skyFragment};
        const codea::Program transparentProgram{transparentVertex, transparentFragment};
        const codea::Mesh cube{codea::cubeVertices(), 6, {{0, 3, 0}},
                               codea::cubeIndices()};
        const std::vector<float> quadVertices{
            -2,-2,0,  2,-2,0,  2,2,0,
            -2,-2,0,  2, 2,0, -2,2,0};
        const codea::Mesh quad{quadVertices, 3, {{0, 3, 0}}};

        std::vector<glm::mat4> instanceModels;
        for (int z = -5; z < 5; ++z) {
            for (int x = -5; x < 5; ++x) {
                instanceModels.push_back(
                    glm::translate(glm::mat4{1.0f}, {x * 1.55f, -1.2f, z * 1.55f}) *
                    glm::scale(glm::mat4{1.0f}, glm::vec3{0.35f}));
            }
        }

        GLuint instanceBuffer = 0;
        glGenBuffers(1, &instanceBuffer);
        glBindVertexArray(cube.vao());
        glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(instanceModels.size() * sizeof(glm::mat4)),
                     instanceModels.data(), GL_STATIC_DRAW);
        for (GLuint column = 0; column < 4; ++column) {
            const GLuint location = 2 + column;
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                                  reinterpret_cast<const void*>(column * sizeof(glm::vec4)));
            glVertexAttribDivisor(location, 1);
        }
        glBindVertexArray(0);
        const GLuint cubemap = createLabeledColorCubemap();
        glEnable(GL_DEPTH_TEST);

        while (!window.shouldClose()) {
            const auto [width, height] = window.framebufferSize();
            const float aspect = height > 0 ? static_cast<float>(width) / height : 1.0f;
            const glm::vec3 cameraPosition{0.0f, 4.0f, 13.0f};
            const glm::mat4 view = glm::lookAt(cameraPosition, {0, 0, 0}, {0, 1, 0});
            const glm::mat4 projection = glm::perspective(glm::radians(52.0f), aspect,
                                                           0.1f, 100.0f);
            const glm::mat4 viewProjection = projection * view;

            glClearColor(0.02f, 0.025f, 0.04f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            instanceProgram.use();
            glUniformMatrix4fv(instanceProgram.uniform("uViewProjection"), 1, GL_FALSE,
                               &viewProjection[0][0]);
            glBindVertexArray(cube.vao());
            glDrawElementsInstanced(GL_TRIANGLES, cube.indexCount(), GL_UNSIGNED_INT,
                                    nullptr, static_cast<GLsizei>(instanceModels.size()));

            glDepthMask(GL_FALSE);
            glDepthFunc(GL_LEQUAL);
            glCullFace(GL_FRONT);
            skyProgram.use();
            const glm::mat4 skyViewProjection = projection * glm::mat4{glm::mat3{view}};
            glUniformMatrix4fv(skyProgram.uniform("uSkyViewProjection"), 1, GL_FALSE,
                               &skyViewProjection[0][0]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
            glUniform1i(skyProgram.uniform("uSky"), 0);
            cube.draw();

            glDepthFunc(GL_LESS);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            transparentProgram.use();
            const auto drawGlass = [&](float z, const glm::vec4& color) {
                const glm::mat4 model = glm::translate(glm::mat4{1.0f}, {0, 1.0f, z});
                const glm::mat4 mvp = viewProjection * model;
                glUniformMatrix4fv(transparentProgram.uniform("uMvp"), 1, GL_FALSE,
                                   &mvp[0][0]);
                glUniform4fv(transparentProgram.uniform("uColor"), 1, &color[0]);
                quad.draw();
            };
            drawGlass(0.0f, {0.08f, 0.30f, 1.0f, 0.42f});
            drawGlass(3.0f, {1.0f, 0.12f, 0.08f, 0.42f});
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            window.setTitle("Chapter 14 - 100 cubes in 1 instanced draw");
            window.present();
        }

        glDeleteTextures(1, &cubemap);
        glDeleteBuffers(1, &instanceBuffer);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
