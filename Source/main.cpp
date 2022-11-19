#include <io.h>
#include <Renderer.h>

int main() {
    chdir("../..");
    SauronLT::Init(1280, 720, "rtx");
    SauronLT::SetBackground({0.6f, 0.55f, 0.75f, 1.0f});

    Renderer renderer;

    while (SauronLT::Running()) {
        SauronLT::BeginFrame();

        ImGui::Begin("Settings");

        ImGui::End();

        float viewportWidth = ImGui::GetContentRegionAvail().x;
        float viewportHeight = ImGui::GetContentRegionAvail().y;
        renderer.Resize((uint32_t)viewportWidth, (uint32_t)viewportHeight);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        SauronLT::Image image = renderer.GetImage();
        ImGui::Image(image.GetDescriptorSet(), { (float)image.GetWidth(), (float)image.GetHeight() }, ImVec2(1,0), ImVec2(0,1));

        ImGui::End();
        ImGui::PopStyleVar();

        SauronLT::EndFrame();
    }

    SauronLT::Shutdown();
    return 0;
}