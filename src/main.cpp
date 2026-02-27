#include "raylib_compat.h"

#include <algorithm>
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

struct IsoPrism { Vector3 origin; Vector3 size; Color top; Color left; Color right; };

struct Scene {
    std::string id;
    Color background;
    Rectangle walkArea;
    std::vector<Rectangle> blockers;
    std::vector<Hotspot> hotspots;
    std::vector<IsoPrism> geometry;
    std::string flavorText;
};

enum class GameState { FreeRoam, Dialogue, Transition };

static Vector2 Rotate2D(Vector2 p, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return Vector2{p.x * c - p.y * s, p.x * s + p.y * c};
}

static Vector2 WorldToScreen(Vector2 world, float z, Vector2 origin, float scale, float angle, Vector2 pan) {
    Vector2 r = Rotate2D(world, angle);
    return Vector2{origin.x + pan.x + (r.x - r.y) * scale, origin.y + pan.y + (r.x + r.y) * scale * 0.5f - z * scale};
}

static Vector2 ScreenToWorld(Vector2 screen, Vector2 origin, float scale, float angle, Vector2 pan) {
    const float u = (screen.x - origin.x - pan.x) / scale;
    const float v = (screen.y - origin.y - pan.y) / (scale * 0.5f);
    Vector2 rotated{(u + v) * 0.5f, (v - u) * 0.5f};
    return Rotate2D(rotated, -angle);
}

static void DrawQuad(const Vector2& a, const Vector2& b, const Vector2& c, const Vector2& d, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, d, col);
}

static void DrawIsoPrism(const IsoPrism& g, Vector2 origin, float scale, float angle, Vector2 pan) {
    Vector3 o = g.origin, s = g.size;
    Vector2 p000 = WorldToScreen(Vector2{o.x, o.y}, o.z, origin, scale, angle, pan);
    Vector2 p100 = WorldToScreen(Vector2{o.x + s.x, o.y}, o.z, origin, scale, angle, pan);
    Vector2 p010 = WorldToScreen(Vector2{o.x, o.y + s.y}, o.z, origin, scale, angle, pan);
    Vector2 p110 = WorldToScreen(Vector2{o.x + s.x, o.y + s.y}, o.z, origin, scale, angle, pan);
    Vector2 p001 = WorldToScreen(Vector2{o.x, o.y}, o.z + s.z, origin, scale, angle, pan);
    Vector2 p101 = WorldToScreen(Vector2{o.x + s.x, o.y}, o.z + s.z, origin, scale, angle, pan);
    Vector2 p011 = WorldToScreen(Vector2{o.x, o.y + s.y}, o.z + s.z, origin, scale, angle, pan);
    Vector2 p111 = WorldToScreen(Vector2{o.x + s.x, o.y + s.y}, o.z + s.z, origin, scale, angle, pan);
    DrawQuad(p001, p101, p111, p011, g.top);
    DrawQuad(p001, p011, p010, p000, g.left);
    DrawQuad(p101, p111, p110, p100, g.right);
}

static bool InRect(Vector2 p, const Rectangle& r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}

static bool IsBlocked(Vector2 p, const Scene& s) {
    if (!InRect(p, s.walkArea)) return true;
    for (const auto& b : s.blockers) if (InRect(p, b)) return true;
    return false;
}

static Vector2 FindValidTarget(Vector2 desired, const Scene& scene, Vector2 fallback) {
    if (!IsBlocked(desired, scene)) return desired;
    for (float r = 0.2f; r <= 2.8f; r += 0.2f) {
        for (int i = 0; i < 16; ++i) {
            float a = 6.283185f * (static_cast<float>(i) / 16.0f);
            Vector2 c{desired.x + std::cos(a) * r, desired.y + std::sin(a) * r};
            if (!IsBlocked(c, scene)) return c;
        }
    }
    return fallback;
}

static float DepthOf(const IsoPrism& g) { return g.origin.x + g.origin.y + (g.size.x + g.size.y) * 0.5f; }

int main() {
    const int screenWidth = 1366;
    const int screenHeight = 768;
    const Vector2 baseOrigin{680, 185};
    const float isoScale = 34.0f;

    InitWindow(screenWidth, screenHeight, "Submarine Noir - 2.5D Isometric");
    SetTargetFPS(60);

    std::unordered_map<std::string, Scene> scenes;
    scenes["control_room"] = Scene{
        "control_room", Color{15, 20, 26, 255}, Rectangle{-7, -6, 14, 12},
        {Rectangle{-7, -6, 14, 0.9f}, Rectangle{-7, 5.1f, 14, 0.9f}, Rectangle{-7, -6, 0.9f, 12}, Rectangle{6.1f, -6, 0.9f, 12}, Rectangle{-1.1f, -1.8f, 2.2f, 3.6f}},
        {
            {Rectangle{2.6f, -2.8f, 2.0f, 2.1f}, "Command Console", 1, "", {0, 0}},
            {Rectangle{-6.6f, -0.7f, 0.8f, 2.2f}, "Bulkhead Door", -1, "engine_corridor", {5.0f, 0.5f}},
            {Rectangle{-1.8f, 2.0f, 1.2f, 1.4f}, "Captain's Chair", 4, "", {0, 0}},
        },
        {
            {{-7, -6, 0}, {14, 12, 0.2f}, Color{44, 50, 56, 255}, Color{31, 35, 41, 255}, Color{26, 30, 36, 255}},
            {{-7, -6, 0.2f}, {14, 0.9f, 2.9f}, Color{122, 128, 130, 255}, Color{85, 90, 94, 255}, Color{103, 109, 113, 255}},
            {{-7, 5.1f, 0.2f}, {14, 0.9f, 2.9f}, Color{122, 128, 130, 255}, Color{85, 90, 94, 255}, Color{103, 109, 113, 255}},
            {{-7, -5.1f, 0.2f}, {0.9f, 10.2f, 2.9f}, Color{114, 119, 123, 255}, Color{74, 80, 86, 255}, Color{93, 99, 105, 255}},
            {{6.1f, -5.1f, 0.2f}, {0.9f, 10.2f, 2.9f}, Color{114, 119, 123, 255}, Color{74, 80, 86, 255}, Color{93, 99, 105, 255}},
            {{2.4f, -3.2f, 0.2f}, {2.1f, 1.3f, 2.1f}, Color{165, 144, 125, 255}, Color{112, 95, 80, 255}, Color{134, 114, 96, 255}},
            {{-2.3f, 1.7f, 0.2f}, {2.5f, 1.7f, 1.2f}, Color{150, 125, 99, 255}, Color{100, 80, 64, 255}, Color{121, 98, 79, 255}},
            {{-0.9f, -1.8f, 0.2f}, {1.8f, 3.5f, 0.8f}, Color{215, 52, 64, 255}, Color{143, 31, 40, 255}, Color{173, 38, 48, 255}},
        },
        "CONTROL ROOM // painterly steel, grain, damp air"
    };

    scenes["engine_corridor"] = Scene{
        "engine_corridor", Color{24, 13, 16, 255}, Rectangle{-7.5f, -5, 15, 10.5f},
        {Rectangle{-7.5f, -5, 15, 0.8f}, Rectangle{-7.5f, 4.7f, 15, 0.8f}, Rectangle{-7.5f, -5, 0.8f, 10.5f}, Rectangle{6.7f, -5, 0.8f, 10.5f}, Rectangle{-1.3f, -0.5f, 1.8f, 3.1f}},
        {
            {Rectangle{6.1f, -0.4f, 1.2f, 2.0f}, "Return to Control", -1, "control_room", {-5.2f, 0.6f}},
            {Rectangle{-2.8f, -1.9f, 2.3f, 1.9f}, "Maintenance Hatch", 7, "", {0, 0}},
            {Rectangle{1.2f, 2.0f, 2.0f, 1.6f}, "Crew Journal", 10, "", {0, 0}},
        },
        {
            {{-7.5f, -5, 0}, {15, 10.5f, 0.2f}, Color{52, 28, 32, 255}, Color{35, 19, 22, 255}, Color{42, 23, 27, 255}},
            {{-7.5f, -5, 0.2f}, {15, 0.8f, 2.5f}, Color{137, 90, 71, 255}, Color{88, 56, 46, 255}, Color{109, 70, 58, 255}},
            {{-7.5f, 4.7f, 0.2f}, {15, 0.8f, 2.5f}, Color{137, 90, 71, 255}, Color{88, 56, 46, 255}, Color{109, 70, 58, 255}},
            {{-7.5f, -4.2f, 0.2f}, {0.8f, 8.9f, 2.5f}, Color{128, 81, 64, 255}, Color{78, 50, 40, 255}, Color{99, 63, 50, 255}},
            {{6.7f, -4.2f, 0.2f}, {0.8f, 8.9f, 2.5f}, Color{128, 81, 64, 255}, Color{78, 50, 40, 255}, Color{99, 63, 50, 255}},
            {{-4.2f, -2.9f, 0.2f}, {2.7f, 1.5f, 2.0f}, Color{176, 116, 86, 255}, Color{120, 79, 60, 255}, Color{141, 93, 71, 255}},
            {{-1.2f, -0.5f, 0.2f}, {1.6f, 3.1f, 0.8f}, Color{220, 36, 49, 255}, Color{148, 22, 32, 255}, Color{179, 27, 39, 255}},
        },
        "ENGINE CORRIDOR // blood-red strips and heavy shadows"
    };

    std::unordered_map<int, DialogueNode> dialogue{
        {1, {"Ops AI", "Captain, sonar picks up movement outside the hull. Want a full scan?", {{"Do a silent scan.", 2, "silent_scan"}, {"Ping active sonar.", 3, "loud_scan"}, {"Ignore it.", -1, ""}}}},
        {2, {"Ops AI", "Silent sweep complete. Heat signatures uncertain.", {{"Log it.", -1, "prep_security"}, {"Open crew channel.", 5, ""}}}},
        {3, {"Ops AI", "Active ping answered. Something answered back.", {{"Seal doors.", 6, "lockdown"}, {"Keep pinging.", -1, ""}}}},
        {4, {"Inner Voice", "The chair is warm, but nobody sits here anymore.", {{"Sit.", -1, "memory_echo"}, {"Step away.", -1, ""}}}},
        {5, {"Deck Chief", "Metal scratching through vents.", {{"Arm everyone.", -1, "crew_armed"}, {"Stay calm.", -1, "crew_calm"}}}},
        {6, {"System", "LOCKDOWN INITIATED // Some doors failed to close.", {{"Route power to seals.", -1, "reroute_power"}}}},
        {7, {"Mechanic", "Hatch wheel is stuck.", {{"Force it.", 8, "force_hatch"}, {"Leave it.", -1, ""}}}},
        {8, {"Narrator", "Warm air exhales like a living thing.", {{"Flashlight inside.", 9, "light_check"}, {"Close it.", -1, ""}}}},
        {9, {"Narrator", "Wet footprints stop mid-corridor.", {{"Mark for investigation.", -1, "trace_marked"}}}},
        {10, {"Journal", "'Day 41. One chamber appears only on old blueprints.'", {{"Take page.", -1, "journal_page"}, {"Leave it.", -1, ""}}}},
    };

    std::vector<std::string> flags;
    std::vector<std::string> dialogueLog;
    GameState state = GameState::FreeRoam;
    std::string currentSceneId = "control_room";
    Vector2 playerWorld{0.0f, 0.0f};
    Vector2 targetWorld = playerWorld;
    float playerSpeed = 4.5f;
    float clickCooldown = 0.0f;

    int activeDialogueNode = -1;
    bool isFading = false;
    float fadeAlpha = 0.0f;
    float fadeDirection = 1.0f;
    std::string pendingScene;
    Vector2 pendingSpawn{0, 0};

    Vector2 cameraPan{0.0f, 0.0f};
    float cameraAngle = 0.0f;
    Vector2 prevMouse = GetMousePosition();
    int frameCounter = 0;

    while (!WindowShouldClose()) {
        frameCounter++;
        float dt = GetFrameTime();
        Scene& scene = scenes[currentSceneId];
        if (clickCooldown > 0.0f) clickCooldown = std::max(0.0f, clickCooldown - dt);

        Vector2 mouseNow = GetMousePosition();
        Vector2 mouseDelta = Vector2Subtract(mouseNow, prevMouse);
        prevMouse = mouseNow;

        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            cameraPan = Vector2Add(cameraPan, mouseDelta);
        }
        if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            cameraAngle += mouseDelta.x * 0.0065f;
        }

        if (state == GameState::FreeRoam) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && clickCooldown <= 0.0f) {
                Vector2 mouseWorld = ScreenToWorld(mouseNow, baseOrigin, isoScale, cameraAngle, cameraPan);
                bool clickedHotspot = false;

                for (const auto& h : scene.hotspots) {
                    if (InRect(mouseWorld, h.worldRect)) {
                        clickedHotspot = true;
                        targetWorld = FindValidTarget(Vector2{h.worldRect.x + h.worldRect.width * 0.5f, h.worldRect.y + h.worldRect.height * 0.5f}, scene, playerWorld);
                        if (!h.transitionTo.empty()) {
                            pendingScene = h.transitionTo;
                            pendingSpawn = h.spawnWorld;
                            state = GameState::Transition;
                            isFading = true;
                            fadeDirection = 1.0f;
                        } else if (h.dialogueNode >= 0) {
                            activeDialogueNode = h.dialogueNode;
                            state = GameState::Dialogue;
                        }
                        break;
                    }
                }

                if (!clickedHotspot) targetWorld = FindValidTarget(mouseWorld, scene, playerWorld);
                clickCooldown = 0.12f;
            }

            Vector2 delta = Vector2Subtract(targetWorld, playerWorld);
            float dist = Vector2Length(delta);
            if (dist > 0.03f) {
                Vector2 step = Vector2Scale(Vector2Normalize(delta), playerSpeed * dt);
                if (Vector2Length(step) > dist) step = delta;
                Vector2 candidate = Vector2Add(playerWorld, step);
                if (!IsBlocked(candidate, scene)) {
                    playerWorld = candidate;
                } else {
                    Vector2 xOnly{playerWorld.x + step.x, playerWorld.y};
                    Vector2 yOnly{playerWorld.x, playerWorld.y + step.y};
                    if (!IsBlocked(xOnly, scene)) playerWorld = xOnly;
                    else if (!IsBlocked(yOnly, scene)) playerWorld = yOnly;
                }
            }
        } else if (state == GameState::Dialogue && activeDialogueNode >= 0) {
            const DialogueNode& node = dialogue[activeDialogueNode];
            for (size_t i = 0; i < node.choices.size(); ++i) {
                Rectangle btn{54, static_cast<float>(screenHeight - 138 + static_cast<int>(i) * 38), 930, 30};
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouseNow, btn)) {
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

        for (int i = -10; i <= 10; ++i) {
            Vector2 a = WorldToScreen(Vector2{-9.5f, static_cast<float>(i)}, 0.21f, baseOrigin, isoScale, cameraAngle, cameraPan);
            Vector2 b = WorldToScreen(Vector2{9.5f, static_cast<float>(i)}, 0.21f, baseOrigin, isoScale, cameraAngle, cameraPan);
            DrawLineEx(a, b, 1.0f, Color{150, 155, 160, 32});
            Vector2 c = WorldToScreen(Vector2{static_cast<float>(i), -9.5f}, 0.21f, baseOrigin, isoScale, cameraAngle, cameraPan);
            Vector2 d = WorldToScreen(Vector2{static_cast<float>(i), 9.5f}, 0.21f, baseOrigin, isoScale, cameraAngle, cameraPan);
            DrawLineEx(c, d, 1.0f, Color{150, 155, 160, 32});
        }

        float playerDepth = playerWorld.x + playerWorld.y;
        for (const auto& g : scene.geometry) if (DepthOf(g) <= playerDepth) DrawIsoPrism(g, baseOrigin, isoScale, cameraAngle, cameraPan);
        Vector2 p = WorldToScreen(playerWorld, 0.25f, baseOrigin, isoScale, cameraAngle, cameraPan);
        DrawCircleV(Vector2{p.x + 9, p.y + 8}, 12.0f, Color{8, 8, 12, 125});
        DrawCircleV(p, 12.5f, Color{235, 230, 220, 255});
        DrawCircleV(Vector2{p.x, p.y - 2}, 8.3f, Color{20, 20, 25, 255});
        for (const auto& g : scene.geometry) if (DepthOf(g) > playerDepth) DrawIsoPrism(g, baseOrigin, isoScale, cameraAngle, cameraPan);

        Vector2 mouseWorld = ScreenToWorld(mouseNow, baseOrigin, isoScale, cameraAngle, cameraPan);
        for (const auto& h : scene.hotspots) {
            Vector2 c{h.worldRect.x + h.worldRect.width * 0.5f, h.worldRect.y + h.worldRect.height * 0.5f};
            Vector2 cs = WorldToScreen(c, 0.25f, baseOrigin, isoScale, cameraAngle, cameraPan);
            bool hover = InRect(mouseWorld, h.worldRect);
            DrawCircleV(cs, hover ? 16.0f : 10.0f, hover ? Color{248, 228, 120, 130} : Color{140, 220, 180, 70});
            if (hover) DrawText(h.label.c_str(), static_cast<int>(cs.x + 12), static_cast<int>(cs.y - 9), 16, RAYWHITE);
        }

        for (int y = 0; y < screenHeight; y += 3) DrawRectangle(0, y, screenWidth, 1, Color{0, 0, 0, static_cast<unsigned char>(9 + (y + frameCounter) % 10)});
        DrawRectangle(0, 0, screenWidth, 110, Color{86, 25, 28, static_cast<unsigned char>(10 + std::abs((frameCounter % 90) - 45))});

        DrawRectangle(0, 0, screenWidth, 40, Color{6, 10, 14, 230});
        DrawRectangle(10, 6, 740, 28, Color{18, 24, 30, 200});
        DrawRectangleLinesEx(Rectangle{10, 6, 740, 28}, 1.5f, Color{120, 140, 155, 170});
        DrawText(scene.flavorText.c_str(), 18, 13, 14, Color{210, 220, 224, 240});
        DrawText(TextFormat("Flags: %i", static_cast<int>(flags.size())), screenWidth - 150, 12, 14, Color{180, 230, 190, 255});

        DrawRectangle(0, screenHeight - 156, screenWidth, 156, Color{8, 10, 14, 190});
        DrawRectangle(14, screenHeight - 150, 460, 144, Color{12, 18, 24, 180});
        DrawRectangleLinesEx(Rectangle{14, static_cast<float>(screenHeight - 150), 460, 144}, 1.5f, Color{110, 135, 150, 170});
        DrawText("DIALOG LOG", 26, screenHeight - 142, 16, Color{233, 204, 149, 255});
        for (size_t i = 0; i < dialogueLog.size(); ++i) DrawText(dialogueLog[i].c_str(), 26, screenHeight - 120 + static_cast<int>(i) * 16, 14, Color{205, 215, 218, 245});

        if (state == GameState::Dialogue && activeDialogueNode >= 0) {
            const DialogueNode& node = dialogue[activeDialogueNode];
            DrawRectangle(500, screenHeight - 238, screenWidth - 520, 226, Color{12, 14, 18, 236});
            DrawRectangleLinesEx(Rectangle{500, static_cast<float>(screenHeight - 238), static_cast<float>(screenWidth - 520), 226}, 2.0f, Color{132, 156, 178, 255});
            DrawText(node.speaker.c_str(), 520, screenHeight - 220, 20, Color{247, 183, 124, 255});
            DrawText(node.line.c_str(), 520, screenHeight - 191, 19, RAYWHITE);
            for (size_t i = 0; i < node.choices.size(); ++i) {
                Rectangle btn{520, static_cast<float>(screenHeight - 144 + static_cast<int>(i) * 38), static_cast<float>(screenWidth - 560), 30};
                bool hover = CheckCollisionPointRec(mouseNow, btn);
                DrawRectangleRec(btn, hover ? Color{58, 78, 96, 255} : Color{31, 43, 54, 255});
                DrawRectangleLinesEx(btn, 1.0f, Color{150, 173, 191, 255});
                DrawText(node.choices[i].text.c_str(), static_cast<int>(btn.x + 10), static_cast<int>(btn.y + 7), 15, RAYWHITE);
            }
        }

        DrawText("LMB move/interact | RMB drag pan | MMB drag rotate", screenWidth - 452, screenHeight - 20, 12, Color{180, 186, 194, 230});
        if (isFading) DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, fadeAlpha));
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
