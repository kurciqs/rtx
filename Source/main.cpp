#include <io.h>
#include <SauronLT.h>

int main()
{
    chdir("../..");
    SauronLT::Init(1280, 720, "SauronLT");

    while (SauronLT::Running())
    {
        SauronLT::BeginFrame();

        SauronLT::SetClearColor({0.8f, 0.55f, 0.75f, 1.0f});
        ImGui::ShowDemoWindow();

        SauronLT::EndFrame();
    }

    SauronLT::Shutdown();
    return 0;
}