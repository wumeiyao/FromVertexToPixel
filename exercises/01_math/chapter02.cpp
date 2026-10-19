#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

struct Vec3 {
    float x;
    float y;
    float z;
};

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

float length(const Vec3& value) {
    return std::sqrt(dot(value, value));
}

Vec3 normalize(const Vec3& value) {
    const float magnitude = length(value);
    if (magnitude < 1e-6f) {
        throw std::invalid_argument("Cannot normalize a zero vector");
    }
    return {value.x / magnitude, value.y / magnitude, value.z / magnitude};
}

struct Vec4 {
    float x;
    float y;
    float z;
    float w;
};

struct Mat4 {
    // m[row][column]. The program uses column vectors: result = matrix * vector.
    float m[4][4]{};
};

Mat4 identity() {
    Mat4 result{};
    for (int i = 0; i < 4; ++i) {
        result.m[i][i] = 1.0f;
    }
    return result;
}

Mat4 translation(float tx, float ty, float tz) {
    Mat4 result = identity();
    result.m[0][3] = tx;
    result.m[1][3] = ty;
    result.m[2][3] = tz;
    return result;
}

Mat4 scale(float sx, float sy, float sz) {
    Mat4 result = identity();
    result.m[0][0] = sx;
    result.m[1][1] = sy;
    result.m[2][2] = sz;
    return result;
}

Mat4 rotateZ(float radians) {
    Mat4 result = identity();
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;
    return result;
}

Vec4 multiply(const Mat4& matrix, const Vec4& vector) {
    return {
        matrix.m[0][0] * vector.x + matrix.m[0][1] * vector.y +
            matrix.m[0][2] * vector.z + matrix.m[0][3] * vector.w,
        matrix.m[1][0] * vector.x + matrix.m[1][1] * vector.y +
            matrix.m[1][2] * vector.z + matrix.m[1][3] * vector.w,
        matrix.m[2][0] * vector.x + matrix.m[2][1] * vector.y +
            matrix.m[2][2] * vector.z + matrix.m[2][3] * vector.w,
        matrix.m[3][0] * vector.x + matrix.m[3][1] * vector.y +
            matrix.m[3][2] * vector.z + matrix.m[3][3] * vector.w};
}

Mat4 multiply(const Mat4& left, const Mat4& right) {
    Mat4 result{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            for (int k = 0; k < 4; ++k) {
                result.m[row][column] += left.m[row][k] * right.m[k][column];
            }
        }
    }
    return result;
}

void print(const char* label, const Vec4& value) {
    std::cout << label << " = (" << value.x << ", " << value.y << ", "
              << value.z << ", " << value.w << ")\n";
}

void printMatrix(const char* label, const Mat4& matrix) {
    std::cout << label << " (rows shown left to right):\n";
    for (const auto& row : matrix.m) {
        std::cout << "  [ " << row[0] << ' ' << row[1] << ' ' << row[2]
                  << ' ' << row[3] << " ]\n";
    }
}

void print(const char* label, const Vec3& value) {
    std::cout << label << " = (" << value.x << ", " << value.y << ", "
              << value.z << ")\n";
}

int main() {
    const Vec3 right{1.0f, 0.0f, 0.0f};
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 diagonal{3.0f, 4.0f, 0.0f};

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "dot(right, up) = " << dot(right, up)
              << "  (zero means perpendicular)\n";
    print("cross(right, up)", cross(right, up));
    print("normalize(3,4,0)", normalize(diagonal));
    std::cout << "dot(normalized diagonal, up) = "
              << dot(normalize(diagonal), up)
              << "  (projection length on up)\n";

    const Vec4 point{1.0f, 2.0f, 3.0f, 1.0f};
    const Vec4 direction{1.0f, 2.0f, 3.0f, 0.0f};
    const Mat4 scaleMatrix = scale(2.0f, 3.0f, 1.0f);
    const Mat4 rotationMatrix = rotateZ(3.14159265358979323846f / 2.0f);
    const Mat4 translationMatrix = translation(5.0f, 2.0f, -1.0f);
    const Mat4 model = multiply(translationMatrix,
                                multiply(rotationMatrix, scaleMatrix));
    printMatrix("Scale", scaleMatrix);
    printMatrix("Rotation Z 90 degrees", rotationMatrix);
    printMatrix("Translation", translationMatrix);
    printMatrix("Model = Translation * Rotation * Scale", model);
    print("Model * point (w=1)", multiply(model, point));
    print("Model * direction (w=0)", multiply(model, direction));
    return 0;
}
