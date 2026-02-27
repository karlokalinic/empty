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
    std::vector<IsoPrism> solids;
    std::vector<IsoPrism> wallsAlways;
    std::string flavorText;
};

enum class AppMode { MainMenu, InGame };
enum class GameState { FreeRoam, Dialogue, Transition };

static Vector2 Rotate2D(Vector2 p, float a) {
    float c = std::cos(a), s = std::sin(a);
    return Vector2{p.x * c - p.y * s, p.x * s + p.y * c};
}

static Vector2 WorldToScreen(Vector2 world, float z, Vector2 origin, float scale, float angle, Vector2 pan) {
    Vector2 r = Rotate2D(world, angle);
    return Vector2{origin.x + pan.x + (r.x - r.y) * scale, origin.y + pan.y + (r.x + r.y) * scale * 0.5f - z * scale};
}

static Vector2 ScreenToWorld(Vector2 screen, Vector2 origin, float scale, float angle, Vector2 pan) {
    float u = (screen.x - origin.x - pan.x) / scale;
    float v = (screen.y - origin.y - pan.y) / (scale * 0.5f);
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
    for (float r = 0.2f; r <= 3.2f; r += 0.2f) {
        for (int i = 0; i < 20; ++i) {
            float a = 6.283185f * (static_cast<float>(i) / 20.0f);
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
    const Vector2 baseOrigin{683, 210};

    InitWindow(screenWidth, screenHeight, "Submarine Noir - Cinematic 2.5D");
    SetTargetFPS(60);

    std::unordered_map<std::string, Scene> scenes;
    scenes["control_room"] = Scene{
        "control_room", Color{14, 20, 26, 255}, Rectangle{-7.2f, -6.0f, 14.4f, 12.0f},
        {
            Rectangle{-1.2f, -1.7f, 2.4f, 3.4f}, // central console cluster
            Rectangle{3.0f, -3.0f, 1.7f, 1.9f},
            Rectangle{-2.5f, 2.1f, 2.1f, 1.5f},
        },
        {
            {Rectangle{2.8f, -2.8f, 1.7f, 1.6f}, "Command Console", 1, "", {0, 0}},
            {Rectangle{-6.8f, -0.5f, 0.9f, 2.2f}, "Bulkhead Door", -1, "engine_corridor", {4.8f, 0.2f}},
            {Rectangle{-2.4f, 2.1f, 1.8f, 1.3f}, "Captain's Chair", 4, "", {0, 0}},
        },
        {
            {{-7.2f, -6, 0}, {14.4f, 12, 0.2f}, Color{60, 68, 74, 255}, Color{41, 47, 52, 255}, Color{36, 40, 45, 255}},
            {{-1.2f, -1.7f, 0.2f}, {2.4f, 3.4f, 0.8f}, Color{222, 44, 51, 255}, Color{147, 24, 33, 255}, Color{181, 31, 41, 255}},
            {{3.0f, -3.0f, 0.2f}, {1.7f, 1.9f, 1.8f}, Color{162, 140, 121, 255}, Color{106, 90, 77, 255}, Color{130, 110, 94, 255}},
            {{-2.5f, 2.1f, 0.2f}, {2.1f, 1.5f, 1.3f}, Color{158, 130, 102, 255}, Color{102, 82, 64, 255}, Color{126, 100, 78, 255}},
            // pipes/industrial hints
            {{-5.2f, -3.8f, 0.2f}, {0.45f, 7.2f, 0.45f}, Color{194, 120, 83, 255}, Color{125, 76, 53, 255}, Color{150, 91, 63, 255}},
            {{-4.2f, -4.1f, 0.2f}, {0.4f, 6.8f, 0.4f}, Color{174, 168, 160, 255}, Color{110, 106, 100, 255}, Color{136, 130, 122, 255}},
        },
        {
            // ONLY two visible walls (L shape): north and west
            {{-7.2f, -6.0f, 0.2f}, {14.4f, 0.85f, 3.2f}, Color{134, 138, 141, 255}, Color{95, 98, 102, 255}, Color{112, 115, 120, 255}},
            {{-7.2f, -5.15f, 0.2f}, {0.85f, 11.15f, 3.2f}, Color{128, 133, 138, 255}, Color{84, 90, 98, 255}, Color{103, 110, 117, 255}},
        },
        "CONTROL ROOM // submarine steel, pipes, pressure, silence"
    };

    scenes["engine_corridor"] = Scene{
        "engine_corridor", Color{24, 13, 15, 255}, Rectangle{-7.8f, -5.3f, 15.6f, 10.8f},
        {
            Rectangle{-1.4f, -0.6f, 2.0f, 3.2f},
            Rectangle{-4.6f, -2.7f, 2.4f, 1.6f},
            Rectangle{2.7f, -2.1f, 1.9f, 1.5f},
        },
        {
            {Rectangle{6.2f, -0.4f, 1.1f, 1.8f}, "Return to Control", -1, "control_room", {-5.4f, 0.2f}},
            {Rectangle{-2.8f, -1.7f, 2.0f, 1.6f}, "Maintenance Hatch", 7, "", {0, 0}},
            {Rectangle{1.4f, 2.1f, 1.9f, 1.4f}, "Crew Journal", 10, "", {0, 0}},
        },
        {
            {{-7.8f, -5.3f, 0}, {15.6f, 10.8f, 0.2f}, Color{58, 32, 34, 255}, Color{39, 22, 24, 255}, Color{46, 26, 28, 255}},
            {{-1.4f, -0.6f, 0.2f}, {2.0f, 3.2f, 0.8f}, Color{226, 40, 47, 255}, Color{151, 23, 29, 255}, Color{182, 29, 35, 255}},
            {{-4.6f, -2.7f, 0.2f}, {2.4f, 1.6f, 2.1f}, Color{171, 108, 79, 255}, Color{112, 73, 54, 255}, Color{136, 89, 66, 255}},
            {{2.7f, -2.1f, 0.2f}, {1.9f, 1.5f, 1.8f}, Color{165, 140, 120, 255}, Color{106, 88, 74, 255}, Color{130, 108, 90, 255}},
            // machinery pipes
            {{-6.0f, -4.1f, 0.2f}, {0.45f, 8.0f, 0.45f}, Color{199, 123, 81, 255}, Color{128, 80, 53, 255}, Color{153, 96, 64, 255}},
            {{-5.2f, -4.3f, 0.2f}, {0.35f, 7.5f, 0.35f}, Color{165, 160, 153, 255}, Color{106, 102, 96, 255}, Color{130, 124, 118, 255}},
        },
        {
            {{-7.8f, -5.3f, 0.2f}, {15.6f, 0.85f, 3.0f}, Color{141, 95, 74, 255}, Color{92, 63, 49, 255}, Color{113, 77, 60, 255}},
            {{-7.8f, -4.45f, 0.2f}, {0.85f, 9.95f, 3.0f}, Color{132, 86, 69, 255}, Color{84, 56, 45, 255}, Color{103, 68, 54, 255}},
        },
        "ENGINE CORRIDOR // submarine machinery, rust, steam, dread"
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

    AppMode appMode = AppMode::MainMenu;
    GameState state = GameState::FreeRoam;
    std::vector<std::string> flags;
    std::vector<std::string> dialogueLog;

    std::string currentSceneId = "control_room";
    Vector2 playerWorld{0.0f, 0.0f};
    Vector2 targetWorld{0.0f, 0.0f};
    float playerSpeed = 4.8f;
    float clickCooldown = 0.0f;

    int activeDialogueNode = -1;
    bool isFading = false;
    float fadeAlpha = 0.0f;
    float fadeDirection = 1.0f;
    std::string pendingScene;
    Vector2 pendingSpawn{0, 0};

    Vector2 cameraPan{0.0f, 0.0f};
    float cameraAngle = 0.0f;
    float cameraZoom = 34.0f;
    const float maxAngle = 0.55f; // never expose out-of-room view
    Vector2 prevMouse = GetMousePosition();
    int frameCounter = 0;

    bool running = true;
    while (running && !WindowShouldClose()) {
        frameCounter++;
        float dt = GetFrameTime();
        if (clickCooldown > 0.0f) clickCooldown = std::max(0.0f, clickCooldown - dt);

        Vector2 mouseNow = GetMousePosition();
        Vector2 mouseDelta = Vector2Subtract(mouseNow, prevMouse);
        prevMouse = mouseNow;

        if (appMode == AppMode::MainMenu) {
            Rectangle startBtn{screenWidth * 0.5f - 180, screenHeight * 0.5f - 20, 360, 54};
            Rectangle quitBtn{screenWidth * 0.5f - 180, screenHeight * 0.5f + 50, 360, 48};
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouseNow, startBtn)) {
                appMode = AppMode::InGame;
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouseNow, quitBtn)) {
                running = false;
            }

            BeginDrawing();
            ClearBackground(Color{8, 11, 15, 255});
            DrawRectangle(0, 0, screenWidth, screenHeight, Color{12, 16, 24, 255});
            for (int y = 0; y < screenHeight; y += 3) DrawRectangle(0, y, screenWidth, 1, Color{0, 0, 0, static_cast<unsigned char>(8 + y % 8)});
            DrawRectangle(0, 0, screenWidth, 140, Color{130, 20, 28, 18});
            DrawText("SUBMARINE NOIR", screenWidth / 2 - 180, 140, 48, Color{236, 224, 210, 255});
            DrawText("Cinematic 2.5D Narrative Prototype", screenWidth / 2 - 190, 195, 22, Color{177, 191, 204, 240});

            bool hoverStart = CheckCollisionPointRec(mouseNow, startBtn);
            bool hoverQuit = CheckCollisionPointRec(mouseNow, quitBtn);
            DrawRectangleRec(startBtn, hoverStart ? Color{72, 88, 102, 255} : Color{48, 62, 74, 255});
            DrawRectangleLinesEx(startBtn, 2.0f, Color{168, 188, 206, 230});
            DrawText("START DIVE", static_cast<int>(startBtn.x + 108), static_cast<int>(startBtn.y + 15), 24, RAYWHITE);
            DrawRectangleRec(quitBtn, hoverQuit ? Color{80, 50, 50, 255} : Color{58, 38, 38, 255});
            DrawRectangleLinesEx(quitBtn, 2.0f, Color{190, 143, 143, 230});
            DrawText("QUIT", static_cast<int>(quitBtn.x + 145), static_cast<int>(quitBtn.y + 13), 22, RAYWHITE);
            EndDrawing();
            continue;
        }

        Scene& scene = scenes[currentSceneId];

        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            cameraPan = Vector2Add(cameraPan, mouseDelta);
        }
        if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            cameraAngle += mouseDelta.x * 0.0065f;
            if (cameraAngle > maxAngle) cameraAngle = maxAngle;
            if (cameraAngle < -maxAngle) cameraAngle = -maxAngle;
        }
        cameraZoom = std::clamp(cameraZoom + GetMouseWheelMove() * 2.0f, 24.0f, 48.0f);

        if (state == GameState::FreeRoam) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && clickCooldown <= 0.0f) {
                Vector2 mouseWorld = ScreenToWorld(mouseNow, baseOrigin, cameraZoom, cameraAngle, cameraPan);
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
                clickCooldown = 0.10f;
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
                Rectangle btn{520, static_cast<float>(screenHeight - 146 + static_cast<int>(i) * 38), static_cast<float>(screenWidth - 560), 30};
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
                Scene& nextScene = scenes[currentSceneId];
                playerWorld = FindValidTarget(pendingSpawn, nextScene, Vector2{0.0f, 0.0f});
                targetWorld = playerWorld;
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
            Vector2 a = WorldToScreen(Vector2{-9.5f, static_cast<float>(i)}, 0.21f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
            Vector2 b = WorldToScreen(Vector2{9.5f, static_cast<float>(i)}, 0.21f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
            DrawLineEx(a, b, 1.0f, Color{140, 144, 148, 28});
            Vector2 c = WorldToScreen(Vector2{static_cast<float>(i), -9.5f}, 0.21f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
            Vector2 d = WorldToScreen(Vector2{static_cast<float>(i), 9.5f}, 0.21f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
            DrawLineEx(c, d, 1.0f, Color{140, 144, 148, 28});
        }

        float playerDepth = playerWorld.x + playerWorld.y;
        for (const auto& g : scene.solids) if (DepthOf(g) <= playerDepth) DrawIsoPrism(g, baseOrigin, cameraZoom, cameraAngle, cameraPan);

        Vector2 p = WorldToScreen(playerWorld, 0.25f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
        DrawCircleV(Vector2{p.x + 9, p.y + 8}, 12.0f, Color{8, 8, 12, 120});
        DrawCircleV(p, 12.5f, Color{235, 230, 220, 255});
        DrawCircleV(Vector2{p.x, p.y - 2}, 8.3f, Color{20, 20, 25, 255});

        for (const auto& g : scene.solids) if (DepthOf(g) > playerDepth) DrawIsoPrism(g, baseOrigin, cameraZoom, cameraAngle, cameraPan);
        for (const auto& w : scene.wallsAlways) DrawIsoPrism(w, baseOrigin, cameraZoom, cameraAngle, cameraPan);

        Vector2 mouseWorld = ScreenToWorld(mouseNow, baseOrigin, cameraZoom, cameraAngle, cameraPan);
        for (const auto& h : scene.hotspots) {
            Vector2 c{h.worldRect.x + h.worldRect.width * 0.5f, h.worldRect.y + h.worldRect.height * 0.5f};
            Vector2 cs = WorldToScreen(c, 0.25f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
            bool hover = InRect(mouseWorld, h.worldRect);
            DrawCircleV(cs, hover ? 16.0f : 10.0f, hover ? Color{248, 228, 120, 130} : Color{140, 220, 180, 70});
            if (hover) DrawText(h.label.c_str(), static_cast<int>(cs.x + 12), static_cast<int>(cs.y - 9), 16, RAYWHITE);
        }

        for (int y = 0; y < screenHeight; y += 3) DrawRectangle(0, y, screenWidth, 1, Color{0, 0, 0, static_cast<unsigned char>(9 + (y + frameCounter) % 10)});
        DrawRectangle(0, 0, screenWidth, 110, Color{86, 25, 28, static_cast<unsigned char>(10 + std::abs((frameCounter % 90) - 45))});
        DrawRectangle(0, 0, screenWidth, 44, Color{6, 10, 14, 230});

        DrawRectangle(10, 7, 760, 30, Color{18, 24, 30, 200});
        DrawRectangleLinesEx(Rectangle{10, 7, 760, 30}, 1.5f, Color{120, 140, 155, 170});
        DrawText(scene.flavorText.c_str(), 18, 14, 14, Color{210, 220, 224, 240});
        DrawText(TextFormat("Flags: %i", static_cast<int>(flags.size())), screenWidth - 150, 14, 14, Color{180, 230, 190, 255});

        DrawRectangle(0, screenHeight - 160, screenWidth, 160, Color{8, 10, 14, 195});
        DrawRectangle(14, screenHeight - 154, 470, 148, Color{12, 18, 24, 180});
        DrawRectangleLinesEx(Rectangle{14, static_cast<float>(screenHeight - 154), 470, 148}, 1.5f, Color{110, 135, 150, 170});
        DrawText("DIALOG LOG", 26, screenHeight - 146, 16, Color{233, 204, 149, 255});
        for (size_t i = 0; i < dialogueLog.size(); ++i) DrawText(dialogueLog[i].c_str(), 26, screenHeight - 124 + static_cast<int>(i) * 16, 14, Color{205, 215, 218, 245});

        if (state == GameState::Dialogue && activeDialogueNode >= 0) {
            const DialogueNode& node = dialogue[activeDialogueNode];
            DrawRectangle(500, screenHeight - 244, screenWidth - 520, 232, Color{12, 14, 18, 236});
            DrawRectangleLinesEx(Rectangle{500, static_cast<float>(screenHeight - 244), static_cast<float>(screenWidth - 520), 232}, 2.0f, Color{132, 156, 178, 255});
            DrawText(node.speaker.c_str(), 520, screenHeight - 224, 20, Color{247, 183, 124, 255});
            DrawText(node.line.c_str(), 520, screenHeight - 193, 19, RAYWHITE);
            for (size_t i = 0; i < node.choices.size(); ++i) {
                Rectangle btn{520, static_cast<float>(screenHeight - 146 + static_cast<int>(i) * 38), static_cast<float>(screenWidth - 560), 30};
                bool hover = CheckCollisionPointRec(mouseNow, btn);
                DrawRectangleRec(btn, hover ? Color{58, 78, 96, 255} : Color{31, 43, 54, 255});
                DrawRectangleLinesEx(btn, 1.0f, Color{150, 173, 191, 255});
                DrawText(node.choices[i].text.c_str(), static_cast<int>(btn.x + 10), static_cast<int>(btn.y + 7), 15, RAYWHITE);
            }
        }

        // cinematic letterbox
        DrawRectangle(0, 0, screenWidth, 24, Color{0, 0, 0, 235});
        DrawRectangle(0, screenHeight - 24, screenWidth, 24, Color{0, 0, 0, 235});

        DrawText("LMB move/interact | RMB pan | MMB rotate | Wheel zoom", screenWidth - 500, screenHeight - 18, 12, Color{180, 186, 194, 230});

        if (isFading) DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, fadeAlpha));
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
