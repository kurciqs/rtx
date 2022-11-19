#ifndef RTX_RENDERER_H
#define RTX_RENDERER_H

#include "SauronLT.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    SauronLT::Image GetImage();
    void Resize(uint32_t width, uint32_t height);

private:
    SauronLT::Image m_Image;
    uint32_t m_ViewportWidth, m_ViewportHeight;

};


#endif //RTX_RENDERER_H
