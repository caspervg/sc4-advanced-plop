#include <raylib.h>
#include <rlImGui.h>

#include "Application.hpp"

int main()
{
    // Initialize Raylib window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "SC4 Advanced Lot Plop");
    SetTargetFPS(60);

    // Initialize rlImGui
    rlImGuiSetup(true);

    // Create application
    Application app;

    // Main loop
    while (!WindowShouldClose()) {
        // Update
        app.Update();

        // Render
        BeginDrawing();
        ClearBackground(DARKGRAY);

        // Begin ImGui frame
        rlImGuiBegin();

        // Render application UI
        app.Render();

        // End ImGui frame
        rlImGuiEnd();

        EndDrawing();
    }

    // Cleanup
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
