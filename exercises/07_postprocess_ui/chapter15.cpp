#include "geometry.hpp"
#include "gl_support.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

class SceneTargets {
public:
    ~SceneTargets() { destroy(); }

    void resize(int width, int height) {
        if (width <= 0 || height <= 0 || (width == width_ && height == height_)) {
            return;
        }
        destroy();
        width_ = width;
        height_ = height;

        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

        glGenTextures(1, &colorTexture_);
        glBindTexture(GL_TEXTURE_2D, colorTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA,
                     GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               colorTexture_, 0);

        glGenRenderbuffers(1, &depthBuffer_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                  depthBuffer_);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Scene framebuffer is incomplete after resize");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::cout << "Scene targets resized to " << width_ << 'x' << height_ << '\n';
    }

    GLuint framebuffer() const { return framebuffer_; }
    GLuint colorTexture() const { return colorTexture_; }

private:
    void destroy() {
        if (depthBuffer_ != 0) glDeleteRenderbuffers(1, &depthBuffer_);
        if (colorTexture_ != 0) glDeleteTextures(1, &colorTexture_);
        if (framebuffer_ != 0) glDeleteFramebuffers(1, &framebuffer_);
        depthBuffer_ = 0;
        colorTexture_ = 0;
        framebuffer_ = 0;
        width_ = 0;
        height_ = 0;
    }

    GLuint framebuffer_ = 0;
    GLuint colorTexture_ = 0;
    GLuint depthBuffer_ = 0;
    int width_ = 0;
    int height_ = 0;
};

int main(int argc, char** argv) {
    std::cout << "chapter15 check: Reinhard(2.0) = " << 2.0f / 3.0f
              << "; Copy reads sceneColor and writes the default framebuffer\n";
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
        void main(){
            float light=0.15+max(dot(normalize(vNormal),normalize(vec3(0.5,1.0,0.3))),0.0)*3.5;
            outColor=vec4(vec3(0.12,0.48,0.95)*light,1.0);
        }
    )";
    constexpr const char* fullscreenVertex = R"(
        #version 410 core
        out vec2 vUv;
        void main(){
            vec2 position=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
            vUv=position;
            gl_Position=vec4(position*2.0-1.0,0.0,1.0);
        }
    )";
    constexpr const char* postFragment = R"(
        #version 410 core
        in vec2 vUv;
        uniform sampler2D uSceneColor;
        uniform int uMode;
        uniform float uExposure;
        layout(location=0) out vec4 outColor;
        void main(){
            vec3 hdr=texture(uSceneColor,vUv).rgb;
            vec3 color=hdr;
            if(uMode==1){
                float gray=dot(hdr,vec3(0.2126,0.7152,0.0722));
                color=vec3(gray);
            }else if(uMode==2){
                vec3 exposed=hdr*uExposure;
                color=exposed/(vec3(1.0)+exposed);
            }else{
                color=clamp(hdr,0.0,1.0);
            }
            outColor=vec4(color,1.0);
        }
    )";

    try {
        codea::Window window{1000, 700, "Chapter 15 - FBO post process (1 gray, 2 tone, 3 copy)"};
        const codea::Program sceneProgram{sceneVertex, sceneFragment};
        const codea::Program postProgram{fullscreenVertex, postFragment};
        const codea::Mesh cube{codea::cubeVertices(), 6,
                               {{0, 3, 0}, {1, 3, 3 * sizeof(float)}},
                               codea::cubeIndices()};
        SceneTargets targets;
        GLuint fullscreenVao = 0;
        glGenVertexArrays(1, &fullscreenVao);
        int mode = 2;

        while (!window.shouldClose()) {
            if (glfwGetKey(window.handle(), GLFW_KEY_1) == GLFW_PRESS) mode = 1;
            if (glfwGetKey(window.handle(), GLFW_KEY_2) == GLFW_PRESS) mode = 2;
            if (glfwGetKey(window.handle(), GLFW_KEY_3) == GLFW_PRESS) mode = 0;
            const auto [width, height] = window.framebufferSize();
            if (width <= 0 || height <= 0) {
                window.present();
                continue;
            }
            targets.resize(width, height);

            glBindFramebuffer(GL_FRAMEBUFFER, targets.framebuffer());
            glViewport(0, 0, width, height);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glClearColor(0.01f, 0.015f, 0.028f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const float aspect = static_cast<float>(width) / height;
            const glm::mat4 model = glm::rotate(glm::mat4{1.0f},
                                                static_cast<float>(window.time()) * 0.6f,
                                                glm::vec3{0, 1, 0});
            const glm::mat4 view = glm::lookAt(glm::vec3{0, 2.4f, 5.5f},
                                              glm::vec3{0}, glm::vec3{0, 1, 0});
            const glm::mat4 projection = glm::perspective(glm::radians(52.0f), aspect,
                                                           0.1f, 100.0f);
            const glm::mat4 mvp = projection * view * model;
            sceneProgram.use();
            glUniformMatrix4fv(sceneProgram.uniform("uMvp"), 1, GL_FALSE, &mvp[0][0]);
            cube.draw();

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);
            glClear(GL_COLOR_BUFFER_BIT);
            postProgram.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, targets.colorTexture());
            glUniform1i(postProgram.uniform("uSceneColor"), 0);
            glUniform1i(postProgram.uniform("uMode"), mode);
            glUniform1f(postProgram.uniform("uExposure"), 1.0f);
            glBindVertexArray(fullscreenVao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glDepthMask(GL_TRUE);

            const char* label = mode == 1 ? "Gray" : mode == 2 ? "HDR + Reinhard" : "Copy";
            window.setTitle(std::string{"Chapter 15 - mode: "} + label +
                            " (1 gray, 2 tone, 3 copy)");
            window.present();
        }
        glDeleteVertexArrays(1, &fullscreenVao);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
