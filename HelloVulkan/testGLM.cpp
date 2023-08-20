#include <glm/glm.hpp>
#include <iostream>

int main() {
    glm::vec4 vec = glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
    glm::mat4x4 mat = glm::mat4x4(
                        1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 2.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 3.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 4.0f
                      );
    glm::vec4 v = mat * vec;

    std::cout << "vec = " << "("
              << vec.x << ", "
              << vec.y << ", "
              << vec.z << ", "
              << vec.w << ")"
              << std::endl;

    std::cout << "mat = \n" << "["
              << mat[0][0] << ", " << mat[0][1] << ", " << mat[0][2] << ", " << mat[0][3] << ", \n"
              << mat[1][0] << ", " << mat[1][1] << ", " << mat[1][2] << ", " << mat[1][3] << ", \n"
              << mat[2][0] << ", " << mat[2][1] << ", " << mat[2][2] << ", " << mat[2][3] << ", \n"
              << mat[3][0] << ", " << mat[3][1] << ", " << mat[3][2] << ", " << mat[3][3] << "]"
              << std::endl;

    std::cout << "v = mat * vec = " << "("
              << v.x << ", "
              << v.y << ", "
              << v.z << ", "
              << v.w << ")"
              << std::endl;

    return 0;
}
