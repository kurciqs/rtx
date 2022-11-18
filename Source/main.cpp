#include <io.h>
#include <SauronLT.h>

int main() {
    chdir("../..");
    SauronLT::Init(1280, 720, "rtx");
    SauronLT::SetClearColor({0.6f, 0.55f, 0.75f, 1.0f});

    while (SauronLT::Running()) {
        SauronLT::BeginFrame();

        ImGui::ShowDemoWindow();

        ImGui::Begin("Viewport");
        {
            // TODO
        }
        ImGui::End();

        SauronLT::EndFrame();
    }

    SauronLT::Shutdown();
    return 0;
}