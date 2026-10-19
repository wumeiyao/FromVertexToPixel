#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#error "These learning samples currently use the macOS OpenGL 4.1 headers."
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace codea {

inline bool checkOnly(int argc, char** argv) {
    return argc > 1 && std::string{argv[1]} == "--check";
}

class Window {
public:
    Window(int width, int height, const char* title) {
        glfwSetErrorCallback([](int, const char* message) {
            std::cerr << "GLFW: " << message << '\n';
        });
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("glfwInit failed");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!window_) {
            glfwTerminate();
            throw std::runtime_error("glfwCreateWindow failed");
        }

        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow*, int w, int h) {
            glViewport(0, 0, w, h);
        });
        glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int, int action, int) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        });

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        if (const char* value = std::getenv("CODEA_MAX_FRAMES")) {
            maxFrames_ = std::max(0, std::stoi(value));
        }
    }

    ~Window() {
        if (window_) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* handle() const { return window_; }
    bool shouldClose() const { return glfwWindowShouldClose(window_) == GLFW_TRUE; }
    double time() const { return glfwGetTime(); }

    std::pair<int, int> framebufferSize() const {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        return {width, height};
    }

    void setTitle(const std::string& title) {
        glfwSetWindowTitle(window_, title.c_str());
    }

    void present() {
        glfwSwapBuffers(window_);
        glfwPollEvents();
        ++presentedFrames_;
        if (maxFrames_ > 0 && presentedFrames_ >= maxFrames_) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
    }

private:
    GLFWwindow* window_ = nullptr;
    int maxFrames_ = 0;
    int presentedFrames_ = 0;
};

inline GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(length), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed:\n" + log);
    }
    return shader;
}

class Program {
public:
    Program(const char* vertexSource, const char* fragmentSource) {
        const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
        const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        id_ = glCreateProgram();
        glAttachShader(id_, vertex);
        glAttachShader(id_, fragment);
        glLinkProgram(id_);
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        GLint linked = GL_FALSE;
        glGetProgramiv(id_, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            GLint length = 0;
            glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &length);
            std::string log(static_cast<std::size_t>(length), '\0');
            glGetProgramInfoLog(id_, length, nullptr, log.data());
            glDeleteProgram(id_);
            id_ = 0;
            throw std::runtime_error("Program link failed:\n" + log);
        }
    }

    ~Program() {
        if (id_ != 0) {
            glDeleteProgram(id_);
        }
    }

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    GLuint id() const { return id_; }
    void use() const { glUseProgram(id_); }
    GLint uniform(const char* name) const { return glGetUniformLocation(id_, name); }

private:
    GLuint id_ = 0;
};

struct Attribute {
    GLuint location;
    GLint components;
    std::size_t byteOffset;
};

class Mesh {
public:
    Mesh(const std::vector<float>& vertices, int strideFloats,
         const std::vector<Attribute>& attributes,
         const std::vector<std::uint32_t>& indices = {})
        : vertexCount_(static_cast<GLsizei>(vertices.size() /
                                           static_cast<std::size_t>(strideFloats))),
          indexCount_(static_cast<GLsizei>(indices.size())) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vertexBuffer_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                     vertices.data(), GL_STATIC_DRAW);

        for (const Attribute& attribute : attributes) {
            glEnableVertexAttribArray(attribute.location);
            glVertexAttribPointer(attribute.location, attribute.components, GL_FLOAT,
                                  GL_FALSE, strideFloats * sizeof(float),
                                  reinterpret_cast<const void*>(attribute.byteOffset));
        }

        if (!indices.empty()) {
            glGenBuffers(1, &indexBuffer_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                         indices.data(), GL_STATIC_DRAW);
        }
        glBindVertexArray(0);
    }

    ~Mesh() {
        if (indexBuffer_ != 0) {
            glDeleteBuffers(1, &indexBuffer_);
        }
        if (vertexBuffer_ != 0) {
            glDeleteBuffers(1, &vertexBuffer_);
        }
        if (vao_ != 0) {
            glDeleteVertexArrays(1, &vao_);
        }
    }

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    GLuint vao() const { return vao_; }
    GLsizei indexCount() const { return indexCount_; }

    void draw() const {
        glBindVertexArray(vao_);
        if (indexCount_ > 0) {
            glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
        } else {
            glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
        }
    }

private:
    GLuint vao_ = 0;
    GLuint vertexBuffer_ = 0;
    GLuint indexBuffer_ = 0;
    GLsizei vertexCount_ = 0;
    GLsizei indexCount_ = 0;
};

inline void printOpenGlInfo() {
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n'
              << "GLSL  : " << glGetString(GL_SHADING_LANGUAGE_VERSION) << '\n'
              << "GPU   : " << glGetString(GL_RENDERER) << '\n';
}

} // namespace codea
