#ifndef RTX_RENDERER_H
#define RTX_RENDERER_H

#include "SauronLT.h"
#include "Random.h"
#include <memory>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

struct Material {
    glm::vec3 albedo;
    float roughness;
    float metallic;
};

struct Sphere {
    int materialIndex;
    glm::vec3 position{0.0f};
    float radius = 0.5f;
};

struct Scene {
    std::vector<Sphere> spheres;
    std::vector<Material> materials;
};

struct HitPayload {
    glm::vec3 position;
    glm::vec3 normal;
    Sphere hitSphere;
    float distance;
};

class Camera
{
public:
    Camera(float verticalFOV, float nearClip, float farClip);

    void Update(float ts);
    void Resize(uint32_t width, uint32_t height);

    const glm::mat4& GetProjection() const { return m_Projection; }
    const glm::mat4& GetInverseProjection() const { return m_InverseProjection; }
    const glm::mat4& GetView() const { return m_View; }
    const glm::mat4& GetInverseView() const { return m_InverseView; }

    const glm::vec3& GetPosition() const { return m_Position; }
    const glm::vec3& GetDirection() const { return m_ForwardDirection; }

    const std::vector<glm::vec3>& GetRayDirections() const { return m_RayDirections; }

    float GetRotationSpeed();
private:
    void RecalculateProjection();
    void RecalculateView();
    void RecalculateRayDirections();

    glm::mat4 m_Projection{ 1.0f };
    glm::mat4 m_View{ 1.0f };
    glm::mat4 m_InverseProjection{ 1.0f };
    glm::mat4 m_InverseView{ 1.0f };

    float m_VerticalFOV = 45.0f;
    float m_NearClip = 0.1f;
    float m_FarClip = 100.0f;

    glm::vec3 m_Position{0.0f, 0.0f, 0.0f};
    glm::vec3 m_ForwardDirection{0.0f, 0.0f, 0.0f};

    // Cached ray directions
    std::vector<glm::vec3> m_RayDirections;

    glm::vec2 m_LastMousePosition{ 0.0f, 0.0f };
    uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
};

class Renderer {
public:
    struct Settings {
        bool accumulate = true;
    };
public:
    Renderer();
    ~Renderer() = default;

    void Destroy();
    std::shared_ptr<SauronLT::Image> GetImage();
    void Resize(uint32_t width, uint32_t height);
    void Render();
    glm::vec4 PerPixel(uint32_t x, uint32_t y);
    HitPayload TraceRay(Ray ray);
    Scene& GetScene() { return m_Scene; }
    Settings& GetSettings() { return m_Settings; }
    void ResetFrameIndex() { m_FrameIndex = 0; }
private:
    Settings m_Settings;
    std::shared_ptr<SauronLT::Image> m_Image;
    Scene m_Scene;
    Camera m_Camera;

    glm::vec4* m_AccumulationData = nullptr;
    uint32_t* m_ImageData = nullptr;

    uint32_t m_FrameIndex = 1;
};


#endif //RTX_RENDERER_H
