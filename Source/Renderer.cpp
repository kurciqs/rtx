#include "Renderer.h"

Renderer::Renderer() : m_Camera(45.0f, 0.001f, 1000.0f) {
    m_Scene.spheres.resize(2);

    m_Scene.spheres[0] = {
            glm::vec3(0.1f, 0.5f, 0.4f), glm::vec3(0.0f), 0.5f
    };

    m_Scene.spheres[1] = {
            glm::vec3(0.58f, 0.2f, 0.1f), glm::vec3(0.0f, 2.0f, 0.0f), 1.02f
    };
}

void Renderer::Resize(uint32_t width, uint32_t height) {
    if (m_Image)
    {
        // No resize necessary
        if (m_Image->GetWidth() == width && m_Image->GetHeight() == height)
            return;

        m_Image->Resize(width, height);
    }
    else
    {
        m_Image = std::make_shared<SauronLT::Image>(width, height, SauronLT::ImageFormat::RGBA);
    }

    delete[] m_ImageData;
    m_ImageData = new uint32_t[width * height];

    m_Camera.Resize(width, height);
}

void Renderer::Destroy() {
    m_Image->Release();
    m_Scene.spheres.clear();
}

std::shared_ptr<SauronLT::Image> Renderer::GetImage() {
    return m_Image;
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
    Ray ray{m_Camera.GetPosition(), m_Camera.GetRayDirections()[x + y * m_Image->GetWidth()]};

    glm::vec3 skyColor(0.6f, 0.7f, 0.9f);
    glm::vec3 pixelColor(0.0f);

    int bounces = 2;
    float multiplier = 1.0f;
    for (int i = 0; i < bounces; i++) {
        HitPayload hitPayload = TraceRay(ray);

        if (hitPayload.distance < 0.0f) {
            pixelColor += skyColor * multiplier;
            break;
        }

        glm::vec3 lightDir = glm::normalize(glm::vec3(-1.0f));

        float d = glm::max(glm::dot(hitPayload.normal, -lightDir), 0.0f);
        pixelColor += d * hitPayload.hitSphere.color * multiplier;

        multiplier *= 0.5f;

        ray.origin = hitPayload.position + hitPayload.normal * 0.0001f;
        ray.direction = glm::reflect(ray.direction, hitPayload.normal);
    }

    return {pixelColor, 1.0f};
}

static uint32_t ConvertToRGBA(const glm::vec4& color)
{
    auto r = (uint8_t)(color.r * 255.0f);
    auto g = (uint8_t)(color.g * 255.0f);
    auto b = (uint8_t)(color.b * 255.0f);
    auto a = (uint8_t)(color.a * 255.0f);

    uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
    return result;
}

void Renderer::Render() {
    m_Camera.Update(0.016f);
    for (uint32_t y = 0; y < m_Image->GetHeight(); y++)
    {
        for (uint32_t x = 0; x < m_Image->GetWidth(); x++)
        {
            glm::vec4 color = PerPixel(x, y);
            color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
            m_ImageData[x + y * m_Image->GetWidth()] = ConvertToRGBA(color);
        }
    }

    m_Image->SetData(m_ImageData);
}

HitPayload Renderer::TraceRay(Ray ray) {
    bool hasHit = false;
    HitPayload hit{.distance = FLT_MAX};

    for (int i = 0; i < m_Scene.spheres.size(); i++) {
        Sphere& sphere = m_Scene.spheres[i];
        glm::vec3 origin = ray.origin - sphere.position;

        float a = glm::dot(ray.direction, ray.direction);
        float b = 2.0f * glm::dot(origin, ray.direction);
        float c = glm::dot(origin, origin) - sphere.radius * sphere.radius;

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f)
            continue;

        float closestT = (-b - glm::sqrt(discriminant)) / (2.0f * a);

        if (closestT > 0.0f && closestT < hit.distance) {
            hit.distance = closestT;
            hit.position = origin + ray.direction * hit.distance;
            hit.normal = glm::normalize(hit.position);
            hit.position += sphere.position;
            hit.hitSphere = sphere;
            hasHit = true;
        }
    }
    if (!hasHit) {
        hit.distance = -1.0f;
    }


    return hit;
}


Camera::Camera(float verticalFOV, float nearClip, float farClip)
        : m_VerticalFOV(verticalFOV), m_NearClip(nearClip), m_FarClip(farClip)
{
    m_ForwardDirection = glm::vec3(0, 0, -1);
    m_Position = glm::vec3(0, 0, 6);
}

void Camera::Update(float ts)
{
    glm::vec2 mousePos(0.0f);
    SauronLT::Input::GetCursorPos(&mousePos.x, &mousePos.y);
    glm::vec2 delta = (mousePos - m_LastMousePosition) * 0.002f;
    m_LastMousePosition = mousePos;

    if (!SauronLT::Input::IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
    {
        return;
    }

    bool moved = false;

    constexpr glm::vec3 upDirection(0.0f, 1.0f, 0.0f);
    glm::vec3 rightDirection = glm::cross(m_ForwardDirection, upDirection);

    float speed = 5.0f;

    // Movement
    if (SauronLT::Input::IsKeyDown(GLFW_KEY_W))
    {
        m_Position += m_ForwardDirection * speed * ts;
        moved = true;
    }
    if (SauronLT::Input::IsKeyDown(GLFW_KEY_S))
    {
        m_Position -= m_ForwardDirection * speed * ts;
        moved = true;
    }
    if (SauronLT::Input::IsKeyDown(GLFW_KEY_A))
    {
        m_Position -= rightDirection * speed * ts;
        moved = true;
    }
    if (SauronLT::Input::IsKeyDown(GLFW_KEY_D))
    {
        m_Position += rightDirection * speed * ts;
        moved = true;
    }
    if (SauronLT::Input::IsKeyDown(GLFW_KEY_Q))
    {
        m_Position -= upDirection * speed * ts;
        moved = true;
    }
    if (SauronLT::Input::IsKeyDown(GLFW_KEY_E))
    {
        m_Position += upDirection * speed * ts;
        moved = true;
    }

    // Rotation
    if (delta.x != 0.0f || delta.y != 0.0f)
    {
        float pitchDelta = delta.y * GetRotationSpeed();
        float yawDelta = delta.x * GetRotationSpeed();

        glm::quat q = glm::normalize(glm::cross(glm::angleAxis(-pitchDelta, rightDirection),
                                                glm::angleAxis(-yawDelta, glm::vec3(0.f, 1.0f, 0.0f))));
        m_ForwardDirection = glm::rotate(q, m_ForwardDirection);

        moved = true;
    }

    if (moved)
    {
        RecalculateView();
        RecalculateRayDirections();
    }
}

void Camera::Resize(uint32_t width, uint32_t height)
{
    if (width == m_ViewportWidth && height == m_ViewportHeight)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;

    RecalculateProjection();
    RecalculateRayDirections();
}

float Camera::GetRotationSpeed()
{
    return 0.5f;
}

void Camera::RecalculateProjection()
{
    m_Projection = glm::perspectiveFov(glm::radians(m_VerticalFOV), (float)m_ViewportWidth, (float)m_ViewportHeight, m_NearClip, m_FarClip);
    m_InverseProjection = glm::inverse(m_Projection);
}

void Camera::RecalculateView()
{
    m_View = glm::lookAt(m_Position, m_Position + m_ForwardDirection, glm::vec3(0, 1, 0));
    m_InverseView = glm::inverse(m_View);
}

void Camera::RecalculateRayDirections()
{
    m_RayDirections.resize(m_ViewportWidth * m_ViewportHeight);

    for (uint32_t y = 0; y < m_ViewportHeight; y++)
    {
        for (uint32_t x = 0; x < m_ViewportWidth; x++)
        {
            glm::vec2 coord = { (float)x / (float)m_ViewportWidth, (float)y / (float)m_ViewportHeight };
            coord = coord * 2.0f - 1.0f; // -1 -> 1

            glm::vec4 target = m_InverseProjection * glm::vec4(coord.x, coord.y, 1, 1);
            glm::vec3 rayDirection = glm::vec3(m_InverseView * glm::vec4(glm::normalize(glm::vec3(target) / target.w), 0)); // World space
            m_RayDirections[x + y * m_ViewportWidth] = rayDirection;
        }
    }
}