#include <io.h>
#include <Renderer.h>

int main() {
    chdir("../..");
    SauronLT::Init(1280, 720, "rtx");
    SauronLT::SetBackground({0.6f, 0.55f, 0.75f, 1.0f});

    Renderer renderer;
    double lastRenderTime = 0.0f;

    while (SauronLT::Running()) {
        SauronLT::BeginFrame();
        double beginTime = glfwGetTime();

        ImGui::Begin("Settings");
        ImGui::Text("Last render: %.3fms", (float)lastRenderTime * 1000.0f);
        ImGui::End();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        float viewportWidth = ImGui::GetContentRegionAvail().x;
        float viewportHeight = ImGui::GetContentRegionAvail().y;

        renderer.Resize((uint32_t)viewportWidth, (uint32_t)viewportHeight);
        renderer.Render();

        auto image = renderer.GetImage();
        if (image)
            ImGui::Image(image->GetDescriptorSet(), {(float) image->GetWidth(), (float) image->GetHeight()}, ImVec2(0, 1), ImVec2(1, 0));

        ImGui::End();
        ImGui::PopStyleVar();

        SauronLT::EndFrame();
        lastRenderTime = glfwGetTime() - beginTime;
    }

    renderer.Destroy();

    SauronLT::Shutdown();
    return 0;
}