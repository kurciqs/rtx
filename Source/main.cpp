#include <io.h>
#include <SauronLT.h>

int main() {
    chdir("../..");
    SauronLT::Init(1280, 720, "rtx");
    SauronLT::SetBackground({0.6f, 0.55f, 0.75f, 1.0f});

    while (SauronLT::Running()) {
        SauronLT::BeginFrame();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

//        ImGui::Image(image->GetDescriptorSet(), { (float)image->GetWidth(), (float)image->GetHeight() },ImVec2(0, 1), ImVec2(1, 0));

        ImGui::End();
        ImGui::PopStyleVar();

        SauronLT::EndFrame();
    }

    SauronLT::Shutdown();
    return 0;
}