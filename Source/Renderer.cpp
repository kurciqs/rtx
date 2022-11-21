#include "Renderer.h"

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
}

void Renderer::Destroy() {
    m_Image->Release();
}

std::shared_ptr<SauronLT::Image> Renderer::GetImage() {
    return m_Image;
}

static glm::vec4 PerPixel(uint32_t x, uint32_t y)
{
    glm::vec2 uv = glm::vec2((float)x, (float)y) / 255.0f;
    uv = (uv - glm::vec2(0.5)) * 2.0f;
    Ray ray{glm::vec3(0.0f), glm::vec3(uv, -1.0f)};
    return glm::vec4((float)x, (float)y, 0.0f, 255.0f) / 255.0f;
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
    float begin = glfwGetTime();
    for (uint32_t y = 0; y < m_Image->GetHeight(); y++)
    {
        for (uint32_t x = 0; x < m_Image->GetWidth(); x++)
        {
            glm::vec4 color = PerPixel(x, y);
            m_ImageData[x + y * m_Image->GetWidth()] = ConvertToRGBA(color);
        }
    }
    printf("%f\n", float(glfwGetTime() - begin) * 1000.0f);

    m_Image->SetData(m_ImageData);
}
