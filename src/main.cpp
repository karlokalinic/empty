#include "raylib_compat.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

struct Choice { std::string text; int nextNode; std::string setFlag; };
struct DialogueNode { std::string speaker; std::string line; std::vector<Choice> choices; };

struct Hotspot {
    Rectangle worldRect;
    std::string label;
    int dialogueNode;
    std::string transitionTo;
    Vector2 spawnWorld;
};

struct IsoPrism {
    Vector3 origin;
    Vector3 size;
    Color top;
    Color left;
    Color right;
};

struct Scene {
    std::string id;
    Color background;
    Rectangle walkArea;
    std::vector<Hotspot> hotspots;
    std::vector<IsoPrism> geometry;
    std::string flavorText;
};

enum class GameState { FreeRoam, Dialogue, Transition };

static Vector2 IsoToScreen(const Vector3& p, const Vector2& origin, float scale) {
    return Vector2{origin.x + (p.x - p.y) * scale, origin.y + (p.x + p.y) * scale * 0.5f - p.z * scale};
}

static Vector2 ScreenToIsoGround(const Vector2& s, const Vector2& origin, float scale) {
    const float a = (s.x - origin.x) / scale;
    const float b = (s.y - origin.y) / (scale * 0.5f);
    return Vector2{(a + b) * 0.5f, (b - a) * 0.5f};
}

static void DrawQuad(const Vector2& a, const Vector2& b, const Vector2& c, const Vector2& d, Color color) {
    DrawTriangle(a, b, c, color);
    DrawTriangle(a, c, d, color);
}

static void DrawIsoPrism(const IsoPrism& prism, const Vector2& origin, float scale) {
    const Vector3 o = prism.origin;
    const Vector3 s = prism.size;

    Vector2 p000 = IsoToScreen(Vector3{o.x, o.y, o.z}, origin, scale);
    Vector2 p100 = IsoToScreen(Vector3{o.x + s.x, o.y, o.z}, origin, scale);
    Vector2 p010 = IsoToScreen(Vector3{o.x, o.y + s.y, o.z}, origin, scale);
    Vector2 p110 = IsoToScreen(Vector3{o.x + s.x, o.y + s.y, o.z}, origin, scale);
    Vector2 p001 = IsoToScreen(Vector3{o.x, o.y, o.z + s.z}, origin, scale);
    Vector2 p101 = IsoToScreen(Vector3{o.x + s.x, o.y, o.z + s.z}, origin, scale);
    Vector2 p011 = IsoToScreen(Vector3{o.x, o.y + s.y, o.z + s.z}, origin, scale);
    Vector2 p111 = IsoToScreen(Vector3{o.x + s.x, o.y + s.y, o.z + s.z}, origin, scale);

    DrawQuad(p001, p101, p111, p011, prism.top);
    DrawQuad(p001, p011, p010, p000, prism.left);
    DrawQuad(p101, p111, p110, p100, prism.right);
}

static float PrismDepth(const IsoPrism& prism) {
    return (prism.origin.x + prism.origin.y) + (prism.size.x + prism.size.y) * 0.5f;
}

static Vector2 ClampToWalkArea(Vector2 p, const Rectangle& r) {
    if (p.x < r.x) p.x = r.x;
    if (p.x > r.x + r.width) p.x = r.x + r.width;
    if (p.y < r.y) p.y = r.y;
    if (p.y > r.y + r.height) p.y = r.y + r.height;
    return p;
}

static bool PointInWorldRect(Vector2 p, const Rectangle& r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}

int main() {
    const int screenWidth = 1366;
    const int screenHeight = 768;
    const Vector2 isoOrigin{680, 190};
    const float isoScale = 34.0f;

    InitWindow(screenWidth, screenHeight, "Submarine Noir - 2.5D Isometric Prototype");
    SetTargetFPS(60);

    std::unordered_map<std::string, Scene> scenes;

    scenes["control_room"] = Scene{
        "control_room", Color{9, 18, 31, 255}, Rectangle{-7.0f, -6.0f, 14.0f, 12.0f},
        {
            {Rectangle{2.6f, -2.8f, 2.0f, 2.1f}, "Command Console", 1, "", {0, 0}},
            {Rectangle{-6.8f, -0.5f, 1.2f, 2.4f}, "Bulkhead Door", -1, "engine_corridor", {5.0f, 0.5f}},
            {Rectangle{-1.3f, 2.0f, 2.2f, 1.8f}, "Captain's Chair", 4, "", {0, 0}},
        },
        {
            {{-7, -6, 0}, {14, 12, 0.2f}, Color{32, 57, 82, 255}, Color{21, 36, 54, 255}, Color{18, 30, 46, 255}},
            {{-7, -6, 0.2f}, {14, 0.3f, 2.8f}, Color{70, 94, 108, 255}, Color{42, 58, 68, 255}, Color{55, 75, 88, 255}},
            {{-7, 5.7f, 0.2f}, {14, 0.3f, 2.8f}, Color{70, 94, 108, 255}, Color{42, 58, 68, 255}, Color{55, 75, 88, 255}},
            {{-7, -5.7f, 0.2f}, {0.3f, 11.4f, 2.8f}, Color{64, 84, 98, 255}, Color{36, 50, 60, 255}, Color{48, 66, 79, 255}},
            {{6.7f, -5.7f, 0.2f}, {0.3f, 11.4f, 2.8f}, Color{64, 84, 98, 255}, Color{36, 50, 60, 255}, Color{48, 66, 79, 255}},
            {{-4.8f, -4.5f, 0.2f}, {1.6f, 1.0f, 1.8f}, Color{130, 144, 152, 255}, Color{78, 90, 102, 255}, Color{99, 115, 125, 255}},
            {{2.5f, -3.4f, 0.2f}, {2.1f, 1.3f, 2.2f}, Color{140, 154, 160, 255}, Color{84, 95, 107, 255}, Color{105, 120, 133, 255}},
            {{-2.0f, 1.8f, 0.2f}, {2.3f, 1.6f, 1.2f}, Color{152, 118, 97, 255}, Color{93, 68, 61, 255}, Color{117, 87, 75, 255}},
            {{-0.6f, -1.4f, 0.2f}, {1.2f, 2.6f, 0.6f}, Color{210, 44, 58, 255}, Color{130, 28, 40, 255}, Color{162, 34, 49, 255}},
        },
        "CONTROL ROOM // wet steel, dead lights, whispering sonar"
    };

    scenes["engine_corridor"] = Scene{
        "engine_corridor", Color{27, 10, 12, 255}, Rectangle{-7.5f, -5.0f, 15.0f, 10.5f},
        {
            {Rectangle{6.1f, -0.4f, 1.2f, 2.0f}, "Return to Control", -1, "control_room", {-5.2f, 0.6f}},
            {Rectangle{-2.8f, -1.9f, 2.3f, 1.9f}, "Maintenance Hatch", 7, "", {0, 0}},
            {Rectangle{1.2f, 2.0f, 2.0f, 1.6f}, "Crew Journal", 10, "", {0, 0}},
        },
        {
            {{-7.5f, -5, 0}, {15, 10.5f, 0.2f}, Color{61, 24, 29, 255}, Color{41, 15, 19, 255}, Color{51, 19, 24, 255}},
            {{-7.5f, -5, 0.2f}, {15, 0.3f, 2.4f}, Color{115, 58, 48, 255}, Color{70, 36, 31, 255}, Color{87, 45, 38, 255}},
            {{-7.5f, 5.2f, 0.2f}, {15, 0.3f, 2.4f}, Color{115, 58, 48, 255}, Color{70, 36, 31, 255}, Color{87, 45, 38, 255}},
            {{-7.5f, -4.7f, 0.2f}, {0.3f, 9.9f, 2.4f}, Color{103, 48, 40, 255}, Color{62, 31, 27, 255}, Color{79, 39, 33, 255}},
            {{7.2f, -4.7f, 0.2f}, {0.3f, 9.9f, 2.4f}, Color{103, 48, 40, 255}, Color{62, 31, 27, 255}, Color{79, 39, 33, 255}},
            {{-4.0f, -2.7f, 0.2f}, {2.6f, 1.4f, 2.4f}, Color{168, 96, 69, 255}, Color{108, 63, 47, 255}, Color{130, 75, 57, 255}},
            {{0.8f, -3.1f, 0.2f}, {2.5f, 1.1f, 1.8f}, Color{140, 103, 83, 255}, Color{88, 63, 52, 255}, Color{111, 79, 64, 255}},
            {{2.0f, 1.5f, 0.2f}, {2.2f, 1.8f, 1.3f}, Color{112, 104, 122, 255}, Color{70, 64, 81, 255}, Color{86, 79, 96, 255}},
            {{-1.1f, 0.4f, 0.2f}, {1.0f, 2.8f, 0.7f}, Color{220, 38, 48, 255}, Color{136, 24, 32, 255}, Color{169, 30, 40, 255}},
        },
        "ENGINE CORRIDOR // the red striplight hums like a wound"
    };

    std::unordered_map<int, DialogueNode> dialogue{
        {1, {"Ops AI", "Captain, sonar picks up movement outside the hull. Want a full scan?", {{"Do a silent scan.", 2, "silent_scan"}, {"Ping active sonar. I want certainty.", 3, "loud_scan"}, {"Ignore it. We stay dark.", -1, ""}}}},
        {2, {"Ops AI", "Silent sweep complete. Heat signatures: uncertain. The ocean is lying tonight.", {{"Log it. Prepare security team.", -1, "prep_security"}, {"Open channel to crew deck.", 5, ""}}}},
        {3, {"Ops AI", "Active ping answered. Something answered back.", {{"Seal all doors.", 6, "lockdown"}, {"Keep pinging.", -1, ""}}}},
        {4, {"Inner Voice", "The chair is still warm, but nobody sits here anymore.", {{"Sit for a moment.", -1, "memory_echo"}, {"Step away.", -1, ""}}}},
        {5, {"Deck Chief", "We're hearing metal scratching through the vents.", {{"Arm everyone.", -1, "crew_armed"}, {"No panic. Stay put.", -1, "crew_calm"}}}},
        {6, {"System", "LOCKDOWN INITIATED // Some doors failed to close.", {{"Route power to magnetic seals.", -1, "reroute_power"}}}},
        {7, {"Mechanic", "Hatch wheel is stuck. Either rust... or pressure from the other side.", {{"Force it open.", 8, "force_hatch"}, {"Leave it.", -1, ""}}}},
        {8, {"Narrator", "The hatch opens two centimeters. Warm air exhales like a living thing.", {{"Shine flashlight inside.", 9, "light_check"}, {"Close it now.", -1, ""}}}},
        {9, {"Narrator", "Wet footprints lead inward, then stop mid-corridor.", {{"Mark area for investigation.", -1, "trace_marked"}}}},
        {10, {"Journal", "'Day 41. We mapped every chamber except one that appears only on old blueprints.'", {{"Take journal page.", -1, "journal_page"}, {"Leave it.", -1, ""}}}},
    };

    std::vector<std::string> flags;
    std::vector<std::string> dialogueLog;

    GameState state = GameState::FreeRoam;
    std::string currentSceneId = "control_room";
    Vector2 playerWorld{0.0f, 0.0f};
    Vector2 targetWorld = playerWorld;
    float playerSpeed = 4.7f;
    int activeDialogueNode = -1;
    bool isFading = false;
    float fadeAlpha = 0.0f;
    float fadeDirection = 1.0f;
    std::string pendingScene;
    Vector2 pendingSpawn{0, 0};
    int frameCounter = 0;

    while (!WindowShouldClose()) {
        frameCounter++;
        const float dt = GetFrameTime();
        Scene& scene = scenes[currentSceneId];

        if (state == GameState::FreeRoam) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                const Vector2 mouseWorld = ScreenToIsoGround(GetMousePosition(), isoOrigin, isoScale);
                bool clickedHotspot = false;
                for (const auto& hotspot : scene.hotspots) {
                    if (PointInWorldRect(mouseWorld, hotspot.worldRect)) {
                        clickedHotspot = true;
                        targetWorld = ClampToWalkArea(Vector2{hotspot.worldRect.x + hotspot.worldRect.width * 0.5f, hotspot.worldRect.y + hotspot.worldRect.height * 0.5f}, scene.walkArea);
                        if (!hotspot.transitionTo.empty()) {
                            pendingScene = hotspot.transitionTo;
                            pendingSpawn = hotspot.spawnWorld;
                            state = GameState::Transition;
                            isFading = true;
                            fadeDirection = 1.0f;
                        } else if (hotspot.dialogueNode >= 0) {
                            activeDialogueNode = hotspot.dialogueNode;
                            state = GameState::Dialogue;
                        }
                        break;
                    }
                }
                if (!clickedHotspot) targetWorld = ClampToWalkArea(mouseWorld, scene.walkArea);
            }

            const Vector2 delta = Vector2Subtract(targetWorld, playerWorld);
            const float dist = Vector2Length(delta);
            if (dist > 0.04f) {
                const Vector2 step = Vector2Scale(Vector2Normalize(delta), playerSpeed * dt);
                playerWorld = (Vector2Length(step) > dist) ? targetWorld : Vector2Add(playerWorld, step);
            }
        } else if (state == GameState::Dialogue && activeDialogueNode >= 0) {
            const DialogueNode& node = dialogue[activeDialogueNode];
            for (size_t i = 0; i < node.choices.size(); ++i) {
                Rectangle btn{40, static_cast<float>(screenHeight - 140 + static_cast<int>(i) * 35), 860, 28};
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), btn)) {
                    const Choice& pick = node.choices[i];
                    if (!pick.setFlag.empty()) flags.push_back(pick.setFlag);
                    dialogueLog.push_back(node.speaker + ": " + node.line);
                    dialogueLog.push_back("YOU: " + pick.text);
                    if (dialogueLog.size() > 14) dialogueLog.erase(dialogueLog.begin(), dialogueLog.begin() + 2);
                    activeDialogueNode = pick.nextNode;
                    if (activeDialogueNode < 0) state = GameState::FreeRoam;
                    break;
                }
            }
        }

        if (state == GameState::Transition && isFading) {
            fadeAlpha += fadeDirection * dt;
            if (fadeDirection > 0.0f && fadeAlpha >= 1.0f) {
                fadeAlpha = 1.0f;
                currentSceneId = pendingScene;
                playerWorld = pendingSpawn;
                targetWorld = pendingSpawn;
                fadeDirection = -1.0f;
            } else if (fadeDirection < 0.0f && fadeAlpha <= 0.0f) {
                fadeAlpha = 0.0f;
                isFading = false;
                state = GameState::FreeRoam;
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawRectangle(0, 0, screenWidth, screenHeight, scene.background);

        for (int i = -9; i <= 9; ++i) {
            Vector2 a = IsoToScreen(Vector3{-8.0f, static_cast<float>(i), 0.21f}, isoOrigin, isoScale);
            Vector2 b = IsoToScreen(Vector3{8.0f, static_cast<float>(i), 0.21f}, isoOrigin, isoScale);
            DrawLineEx(a, b, 1.0f, Color{120, 145, 160, 38});
            Vector2 c = IsoToScreen(Vector3{static_cast<float>(i), -8.0f, 0.21f}, isoOrigin, isoScale);
            Vector2 d = IsoToScreen(Vector3{static_cast<float>(i), 8.0f, 0.21f}, isoOrigin, isoScale);
            DrawLineEx(c, d, 1.0f, Color{120, 145, 160, 38});
        }

        const float playerDepth = playerWorld.x + playerWorld.y;
        for (const auto& g : scene.geometry) if (PrismDepth(g) <= playerDepth) DrawIsoPrism(g, isoOrigin, isoScale);

        Vector2 playerScreen = IsoToScreen(Vector3{playerWorld.x, playerWorld.y, 0.25f}, isoOrigin, isoScale);
        DrawCircleV(Vector2{playerScreen.x + 9, playerScreen.y + 7}, 12.0f, Color{8, 8, 12, 120});
        DrawCircleV(playerScreen, 12.8f, Color{232, 228, 220, 255});
        DrawCircleV(Vector2{playerScreen.x, playerScreen.y - 2}, 8.8f, Color{20, 20, 28, 255});

        for (const auto& g : scene.geometry) if (PrismDepth(g) > playerDepth) DrawIsoPrism(g, isoOrigin, isoScale);

        for (const auto& hotspot : scene.hotspots) {
            Vector2 cWorld{hotspot.worldRect.x + hotspot.worldRect.width * 0.5f, hotspot.worldRect.y + hotspot.worldRect.height * 0.5f};
            Vector2 cScreen = IsoToScreen(Vector3{cWorld.x, cWorld.y, 0.25f}, isoOrigin, isoScale);
            const bool hover = PointInWorldRect(ScreenToIsoGround(GetMousePosition(), isoOrigin, isoScale), hotspot.worldRect);
            DrawCircleV(cScreen, hover ? 16.0f : 10.0f, hover ? Color{250, 230, 120, 120} : Color{140, 220, 180, 75});
            if (hover) DrawText(hotspot.label.c_str(), static_cast<int>(cScreen.x + 12), static_cast<int>(cScreen.y - 10), 16, RAYWHITE);
        }

        for (int y = 0; y < screenHeight; y += 3) {
            unsigned char alpha = static_cast<unsigned char>(10 + (y + frameCounter) % 9);
            DrawRectangle(0, y, screenWidth, 1, Color{0, 0, 0, alpha});
        }
        DrawRectangle(0, 0, screenWidth, 110, Color{110, 20, 30, static_cast<unsigned char>(12 + std::abs((frameCounter % 90) - 45))});

        DrawRectangle(0, 0, screenWidth, 34, Color{5, 8, 12, 225});
        DrawText(scene.flavorText.c_str(), 14, 9, 16, Color{190, 210, 220, 220});
        DrawText(TextFormat("Flags: %i", static_cast<int>(flags.size())), screenWidth - 160, 8, 16, Color{160, 230, 190, 255});

        DrawRectangle(0, screenHeight - 145, screenWidth, 145, Color{10, 10, 14, 172});
        DrawText("LOG", 14, screenHeight - 138, 16, Color{225, 200, 140, 255});
        for (size_t i = 0; i < dialogueLog.size(); ++i) {
            DrawText(dialogueLog[i].c_str(), 14, screenHeight - 115 + static_cast<int>(i) * 16, 14, Color{200, 210, 210, 240});
        }

        if (state == GameState::Dialogue && activeDialogueNode >= 0) {
            const DialogueNode& node = dialogue[activeDialogueNode];
            DrawRectangle(24, screenHeight - 250, screenWidth - 48, 220, Color{8, 8, 11, 235});
            DrawRectangleLinesEx(Rectangle{24, static_cast<float>(screenHeight - 250), static_cast<float>(screenWidth - 48), 220}, 2.0f, Color{130, 160, 190, 255});
            DrawText(node.speaker.c_str(), 40, screenHeight - 234, 20, Color{240, 180, 120, 255});
            DrawText(node.line.c_str(), 40, screenHeight - 204, 20, RAYWHITE);
            for (size_t i = 0; i < node.choices.size(); ++i) {
                Rectangle btn{40, static_cast<float>(screenHeight - 140 + static_cast<int>(i) * 35), 860, 28};
                bool hover = CheckCollisionPointRec(GetMousePosition(), btn);
                DrawRectangleRec(btn, hover ? Color{50, 65, 78, 255} : Color{28, 35, 44, 255});
                DrawRectangleLinesEx(btn, 1.0f, Color{130, 150, 168, 255});
                DrawText(node.choices[i].text.c_str(), static_cast<int>(btn.x + 8), static_cast<int>(btn.y + 6), 16, RAYWHITE);
            }
        }

        if (isFading) DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, fadeAlpha));
        DrawText("LMB: move/interact | Esc: quit", screenWidth - 320, screenHeight - 22, 12, Color{180, 180, 180, 190});
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
