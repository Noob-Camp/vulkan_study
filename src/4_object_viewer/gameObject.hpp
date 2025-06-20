#ifndef GAMEOBJECT_H_
#define GAMEOBJECT_H_


#include <model.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>


namespace RealTimeBox {

struct TransformComponent {
    glm::vec3 translation { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale { 1.0f, 1.0f, 1.0f };
    glm::vec3 rotation { 0.0f, 0.0f, 0.0f };

    // Matrix corrsponds to Translate * Ry * Rx * Rz * Scale
    // Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
    // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
    glm::mat4 mat4();

    glm::mat3 normalMatrix();
};




struct PointLightComponent {
    float lightIntensity = 1.0f;
};




struct GameObject {
    using id_t = unsigned int;
    using Map = std::unordered_map<id_t, GameObject>;

    static GameObject createGameObject() {
        static id_t currentId = 0;
        return GameObject { currentId++ };
    }

    static GameObject makePointLight(
        float intensity = 10.0f,
        float radius = 0.1f,
        glm::vec3 color = glm::vec3(1.0f)
    );

    GameObject(const GameObject &) = delete;
    GameObject &operator=(const GameObject &) = delete;
    GameObject(GameObject &&) = default;
    GameObject &operator=(GameObject &&) = default;

    id_t getId() { return id; }

    glm::vec3 color { 0.0f, 0.0f, 0.0f };
    TransformComponent transform {};

    // Optional pointer components
    std::shared_ptr<Model> model {};
    std::unique_ptr<PointLightComponent> pointLight = nullptr;

private:
    GameObject(id_t objId) : id { objId } {}

    id_t id;
};

}// namespace RealTimeBox

#endif// GAMEOBJECT_H_