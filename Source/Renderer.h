#ifndef RTX_RENDERER_H
#define RTX_RENDERER_H

#include "SauronLT.h"
#include <memory>

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    void Destroy();
    std::shared_ptr<SauronLT::Image> GetImage();
    void Resize(uint32_t width, uint32_t height);
    void Render();

private:
    std::shared_ptr<SauronLT::Image> m_Image;
    uint32_t* m_ImageData = nullptr;
};


#endif //RTX_RENDERER_H
