#include "raylib_compat.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

struct Choice {
    std::string text;
    int nextNode;
    std::vector<std::string> requiresFlags;
    std::vector<std::string> blockedByFlags;
    int requiredKeycardLevel = 0;
    std::string setFlag;
};

struct DialogueNode {
    std::string speaker;
    std::string line;
    std::vector<Choice> choices;
    float timeLimitSec = 0.0f;
    int timeoutNextNode = -1;
    std::string timeoutSetFlag;
};

struct Hotspot {
    Rectangle worldRect;
    std::string label;
    int dialogueNode = -1;
    std::string transitionTo;
    Vector2 spawnWorld{0, 0};
    bool movePlayer = false;
    Vector2 moveTarget{0, 0};
    int requiredKeycardLevel = 0;
    std::string requiredFlag;
    int grantKeycardLevel = 0;
    std::string grantFlag;
    std::string deniedText;
};

struct IsoPrism { Vector3 origin; Vector3 size; Color top; Color left; Color right; };

struct Scene {
    std::string id;
    bool verticalCutaway = false;
    Color background;
    Rectangle walkArea;
    std::vector<Rectangle> blockers;
    std::vector<Hotspot> hotspots;
    std::vector<IsoPrism> solids;
    std::vector<IsoPrism> wallsAlways;
    std::string flavorText;
    std::string onEnterFlag;
};

enum class AppMode { MainMenu, InGame, Ending };
enum class GameState { FreeRoam, Dialogue, Transition };

static bool HasFlag(const std::vector<std::string>& flags, const std::string& f) {
    return !f.empty() && std::find(flags.begin(), flags.end(), f) != flags.end();
}

static void SetFlag(std::vector<std::string>& flags, const std::string& f) {
    if (f.empty() || HasFlag(flags, f)) return;
    flags.push_back(f);
}

static Vector2 Rotate2D(Vector2 p, float a) {
    float c = std::cos(a), s = std::sin(a);
    return Vector2{p.x * c - p.y * s, p.x * s + p.y * c};
}

static Vector2 WorldToScreenIso(Vector2 world, float z, Vector2 origin, float scale, float angle, Vector2 pan) {
    Vector2 r = Rotate2D(world, angle);
    return Vector2{origin.x + pan.x + (r.x - r.y) * scale, origin.y + pan.y + (r.x + r.y) * scale * 0.5f - z * scale};
}

static Vector2 ScreenToWorldIso(Vector2 screen, Vector2 origin, float scale, float angle, Vector2 pan) {
    float u = (screen.x - origin.x - pan.x) / scale;
    float v = (screen.y - origin.y - pan.y) / (scale * 0.5f);
    Vector2 rotated{(u + v) * 0.5f, (v - u) * 0.5f};
    return Rotate2D(rotated, -angle);
}

static Vector2 WorldToScreenCutaway(Vector2 world, float z, Vector2 origin, float scale, Vector2 pan) {
    return Vector2{origin.x + pan.x + world.x * scale, origin.y + pan.y - world.y * scale - z * scale * 0.35f};
}

static Vector2 ScreenToWorldCutaway(Vector2 screen, Vector2 origin, float scale, Vector2 pan) {
    return Vector2{(screen.x - origin.x - pan.x) / scale, -(screen.y - origin.y - pan.y) / scale};
}

static Vector2 ToScreen(const Scene& s, Vector2 world, float z, Vector2 origin, float scale, float angle, Vector2 pan) {
    return s.verticalCutaway ? WorldToScreenCutaway(world, z, origin, scale, pan)
                             : WorldToScreenIso(world, z, origin, scale, angle, pan);
}

static Vector2 ToWorld(const Scene& s, Vector2 screen, Vector2 origin, float scale, float angle, Vector2 pan) {
    return s.verticalCutaway ? ScreenToWorldCutaway(screen, origin, scale, pan)
                             : ScreenToWorldIso(screen, origin, scale, angle, pan);
}

static void DrawQuad(const Vector2& a, const Vector2& b, const Vector2& c, const Vector2& d, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, d, col);
}

static void DrawIsoPrism(const IsoPrism& g, const Scene& s, Vector2 origin, float scale, float angle, Vector2 pan) {
    if (s.verticalCutaway) {
        Vector2 p0 = ToScreen(s, Vector2{g.origin.x, g.origin.y}, g.origin.z + g.size.z, origin, scale, angle, pan);
        Vector2 p1 = ToScreen(s, Vector2{g.origin.x + g.size.x, g.origin.y + g.size.y}, g.origin.z, origin, scale, angle, pan);
        Rectangle r{std::min(p0.x, p1.x), std::min(p0.y, p1.y), std::fabs(p1.x - p0.x), std::fabs(p1.y - p0.y)};
        DrawRectangleRec(r, g.top);
        DrawRectangleLinesEx(r, 1.0f, g.right);
        return;
    }

    Vector3 o = g.origin, sz = g.size;
    Vector2 p000 = ToScreen(s, Vector2{o.x, o.y}, o.z, origin, scale, angle, pan);
    Vector2 p100 = ToScreen(s, Vector2{o.x + sz.x, o.y}, o.z, origin, scale, angle, pan);
    Vector2 p010 = ToScreen(s, Vector2{o.x, o.y + sz.y}, o.z, origin, scale, angle, pan);
    Vector2 p110 = ToScreen(s, Vector2{o.x + sz.x, o.y + sz.y}, o.z, origin, scale, angle, pan);
    Vector2 p001 = ToScreen(s, Vector2{o.x, o.y}, o.z + sz.z, origin, scale, angle, pan);
    Vector2 p101 = ToScreen(s, Vector2{o.x + sz.x, o.y}, o.z + sz.z, origin, scale, angle, pan);
    Vector2 p011 = ToScreen(s, Vector2{o.x, o.y + sz.y}, o.z + sz.z, origin, scale, angle, pan);
    Vector2 p111 = ToScreen(s, Vector2{o.x + sz.x, o.y + sz.y}, o.z + sz.z, origin, scale, angle, pan);

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
    for (float r = 0.2f; r <= 4.0f; r += 0.2f) {
        for (int i = 0; i < 24; ++i) {
            float a = 6.283185f * (static_cast<float>(i) / 24.0f);
            Vector2 c{desired.x + std::cos(a) * r, desired.y + std::sin(a) * r};
            if (!IsBlocked(c, scene)) return c;
        }
    }
    return fallback;
}

static float DepthOf(const IsoPrism& g) { return g.origin.x + g.origin.y + (g.size.x + g.size.y) * 0.5f; }

static bool ChoiceUnlocked(const Choice& c, const std::vector<std::string>& flags, int keycardLevel) {
    if (keycardLevel < c.requiredKeycardLevel) return false;
    for (const auto& f : c.requiresFlags) if (!HasFlag(flags, f)) return false;
    for (const auto& f : c.blockedByFlags) if (HasFlag(flags, f)) return false;
    return true;
}

int main() {
    const int screenWidth = 1366;
    const int screenHeight = 768;
    const Vector2 baseOrigin{683, 220};

    InitWindow(screenWidth, screenHeight, "Submarine Noir - Branching Prototype");
    SetTargetFPS(60);

    std::unordered_map<std::string, Scene> scenes;

    scenes["control_room"] = Scene{
        "control_room", false, Color{14, 20, 26, 255}, Rectangle{-7.2f, -6.0f, 14.4f, 12.0f},
        {Rectangle{-1.2f, -1.7f, 2.4f, 3.4f}, Rectangle{3.0f, -3.0f, 1.7f, 1.9f}, Rectangle{-2.5f, 2.1f, 2.1f, 1.5f}},
        {
            {Rectangle{2.8f, -2.8f, 1.7f, 1.6f}, "Command Console", 1},
            {Rectangle{-6.8f, -0.5f, 0.9f, 2.2f}, "Bulkhead Door", -1, "engine_corridor", {4.8f, 0.2f}},
            {Rectangle{-2.4f, 2.1f, 1.8f, 1.3f}, "Captain's Chair", 4},
            {Rectangle{4.8f, 2.8f, 1.0f, 1.0f}, "Security Locker", -1, "", {0, 0}, false, {0,0}, 0, "", 1, "got_keycard_lv1", "Locker is jammed."},
            {Rectangle{5.6f, -4.4f, 1.0f, 1.0f}, "Command Hatch", 11, "", {0,0}, false, {0,0}, 2, "", 0, "", "Requires Keycard L2"},
            {Rectangle{0.8f, 4.5f, 1.2f, 1.2f}, "Lift to Bunker Stack", -1, "vertical_bunker", {0.0f, 2.0f}},
        },
        {
            {{-7.2f, -6, 0}, {14.4f, 12, 0.2f}, Color{66, 73, 80, 255}, Color{45, 50, 56, 255}, Color{38, 42, 47, 255}},
            {{-1.2f, -1.7f, 0.2f}, {2.4f, 3.4f, 0.8f}, Color{222, 44, 51, 255}, Color{147, 24, 33, 255}, Color{181, 31, 41, 255}},
            {{3.0f, -3.0f, 0.2f}, {1.7f, 1.9f, 1.8f}, Color{162, 140, 121, 255}, Color{106, 90, 77, 255}, Color{130, 110, 94, 255}},
            {{-5.8f, -4.2f, 0.2f}, {0.45f, 8.0f, 0.45f}, Color{199, 123, 81, 255}, Color{128, 80, 53, 255}, Color{153, 96, 64, 255}},
        },
        {
            {{-7.2f, -6.0f, 0.2f}, {14.4f, 0.85f, 3.2f}, Color{139, 143, 146, 255}, Color{95, 98, 102, 255}, Color{112, 115, 120, 255}},
            {{-7.2f, -5.15f, 0.2f}, {0.85f, 11.15f, 3.2f}, Color{132, 137, 142, 255}, Color{84, 90, 98, 255}, Color{103, 110, 117, 255}},
        },
        "CONTROL ROOM // pressure, steel, failing trust", "visited_control"
    };

    scenes["engine_corridor"] = Scene{
        "engine_corridor", false, Color{24, 13, 15, 255}, Rectangle{-7.8f, -5.3f, 15.6f, 10.8f},
        {Rectangle{-1.4f, -0.6f, 2.0f, 3.2f}, Rectangle{-4.6f, -2.7f, 2.4f, 1.6f}, Rectangle{2.7f, -2.1f, 1.9f, 1.5f}},
        {
            {Rectangle{6.2f, -0.4f, 1.1f, 1.8f}, "Return to Control", -1, "control_room", {-5.4f, 0.2f}},
            {Rectangle{-2.8f, -1.7f, 2.0f, 1.6f}, "Maintenance Hatch", 7},
            {Rectangle{1.4f, 2.1f, 1.9f, 1.4f}, "Crew Journal", 10, "", {0, 0}, false, {0,0}, 0, "", 0, "got_old_blueprint", ""},
        },
        {
            {{-7.8f, -5.3f, 0}, {15.6f, 10.8f, 0.2f}, Color{58, 32, 34, 255}, Color{39, 22, 24, 255}, Color{46, 26, 28, 255}},
            {{-1.4f, -0.6f, 0.2f}, {2.0f, 3.2f, 0.8f}, Color{226, 40, 47, 255}, Color{151, 23, 29, 255}, Color{182, 29, 35, 255}},
            {{-6.2f, -4.3f, 0.2f}, {0.45f, 8.1f, 0.45f}, Color{199, 123, 81, 255}, Color{128, 80, 53, 255}, Color{153, 96, 64, 255}},
        },
        {
            {{-7.8f, -5.3f, 0.2f}, {15.6f, 0.85f, 3.0f}, Color{141, 95, 74, 255}, Color{92, 63, 49, 255}, Color{113, 77, 60, 255}},
            {{-7.8f, -4.45f, 0.2f}, {0.85f, 9.95f, 3.0f}, Color{132, 86, 69, 255}, Color{84, 56, 45, 255}, Color{103, 68, 54, 255}},
        },
        "ENGINE CORRIDOR // rust and red emergency strips", "visited_engine"
    };

    scenes["vertical_bunker"] = Scene{
        "vertical_bunker", true, Color{10, 14, 20, 255}, Rectangle{-7.0f, -14.0f, 14.0f, 16.0f},
        {
            Rectangle{-7.0f, -1.8f, 14.0f, 0.6f}, Rectangle{-7.0f, -5.8f, 14.0f, 0.6f}, Rectangle{-7.0f, -9.8f, 14.0f, 0.6f}, Rectangle{-7.0f, -13.8f, 14.0f, 0.6f},
            Rectangle{-2.2f, -10.4f, 2.2f, 0.6f}, Rectangle{2.0f, -6.4f, 2.0f, 0.6f}
        },
        {
            {Rectangle{-5.8f, -2.4f, 1.8f, 1.0f}, "Stairs Down", -1, "", {0, 0}, true, {-5.0f, -6.2f}},
            {Rectangle{-5.8f, -6.4f, 1.8f, 1.0f}, "Stairs Down", -1, "", {0, 0}, true, {-5.0f, -10.2f}},
            {Rectangle{-5.8f, -10.4f, 1.8f, 1.0f}, "Stairs Down", -1, "", {0, 0}, true, {-5.0f, -13.5f}},
            {Rectangle{-3.0f, -10.8f, 1.4f, 1.0f}, "Stairs Up", -1, "", {0,0}, true, {-2.2f, -6.0f}},
            {Rectangle{-3.0f, -6.8f, 1.4f, 1.0f}, "Stairs Up", -1, "", {0,0}, true, {-2.2f, -2.0f}},
            {Rectangle{3.8f, -2.4f, 1.6f, 1.0f}, "Key Office", 12, "", {0,0}, false, {0,0}, 0, "", 2, "got_keycard_lv2", ""},
            {Rectangle{3.6f, -10.4f, 1.6f, 1.0f}, "Locked Cache", 13, "", {0,0}, false, {0,0}, 2, "", 0, "got_mechanical_key", "Requires Keycard L2"},
            {Rectangle{5.8f, -13.5f, 1.1f, 1.1f}, "Return to Command", -1, "control_room", {-4.2f, 0.0f}},
        },
        {
            {{-7, -2.0f, 0.0f}, {14, 0.8f, 1.3f}, Color{84, 96, 106, 255}, Color{56, 64, 72, 255}, Color{66, 76, 86, 255}},
            {{-7, -6.0f, 0.0f}, {14, 0.8f, 1.3f}, Color{84, 96, 106, 255}, Color{56, 64, 72, 255}, Color{66, 76, 86, 255}},
            {{-7, -10.0f, 0.0f}, {14, 0.8f, 1.3f}, Color{84, 96, 106, 255}, Color{56, 64, 72, 255}, Color{66, 76, 86, 255}},
            {{-7, -14.0f, 0.0f}, {14, 0.8f, 1.3f}, Color{84, 96, 106, 255}, Color{56, 64, 72, 255}, Color{66, 76, 86, 255}},
            {{3.8f, -2.6f, 0.0f}, {1.7f, 1.2f, 2.2f}, Color{126, 142, 153, 255}, Color{81, 94, 104, 255}, Color{97, 113, 124, 255}},
            {{3.6f, -10.6f, 0.0f}, {1.7f, 1.2f, 2.2f}, Color{145, 128, 103, 255}, Color{95, 83, 67, 255}, Color{114, 99, 80, 255}},
        },
        {
            {{-7.0f, -14.0f, 0.0f}, {14.0f, 0.6f, 16.0f}, Color{100, 108, 116, 255}, Color{72, 78, 84, 255}, Color{83, 90, 96, 255}},
            {{-7.0f, -14.0f, 0.0f}, {0.6f, 12.8f, 16.0f}, Color{95, 102, 110, 255}, Color{66, 72, 79, 255}, Color{78, 84, 90, 255}},
        },
        "VERTICAL BUNKER // layered decks, stairs, locked sectors", "visited_bunker"
    };

    std::unordered_map<int, DialogueNode> dialogue{
        {1, {"Ops AI", "Captain, sonar picks up movement outside the hull. Want a full scan?", {
            {"Do a silent scan.", 2, {}, {}, 0, "silent_scan"},
            {"Ping active sonar.", 3, {}, {}, 0, "loud_scan"},
            {"Ignore it.", -1, {}, {}, 0, ""},
            {"I checked the bunker stack first.", 14, {"visited_bunker"}, {}, 0, ""}
        }}},
        {2, {"Ops AI", "Silent sweep complete. Heat signatures uncertain.", {{"Log it.", -1, {}, {}, 0, "prep_security"}, {"Open crew channel.", 5, {}, {}, 0, ""}}}},
        {3, {"Ops AI", "Active ping answered. Something answered back.", {{"Seal doors.", 6, {}, {}, 0, "lockdown"}, {"Keep pinging.", -1, {}, {}, 0, ""}}}},
        {4, {"Inner Voice", "The chair is warm, but nobody sits here anymore.", {{"Sit.", -1, {}, {}, 0, "memory_echo"}, {"Step away.", -1, {}, {}, 0, ""}}}},
        {5, {"Deck Chief", "Metal scratching through vents.", {{"Arm everyone.", -1, {}, {}, 0, "crew_armed"}, {"Stay calm.", -1, {}, {}, 0, "crew_calm"}}}},
        {6, {"System", "LOCKDOWN INITIATED // Some doors failed to close.", {{"Route power to seals.", -1, {}, {}, 0, "reroute_power"}}}},
        {7, {"Mechanic", "Hatch wheel is stuck.", {{"Force it.", 8, {}, {}, 0, "force_hatch"}, {"Leave it.", -1, {}, {}, 0, ""}}}},
        {8, {"Narrator", "Warm air exhales like a living thing.", {{"Flashlight inside.", 9, {}, {}, 0, "light_check"}, {"Close it.", -1, {}, {}, 0, ""}}}},
        {9, {"Narrator", "Wet footprints stop mid-corridor.", {{"Mark for investigation.", -1, {}, {}, 0, "trace_marked"}}}},
        {10, {"Journal", "Blueprint margins mention a hidden vertical bunker beneath command.", {{"Pocket blueprint.", -1, {}, {}, 0, "got_old_blueprint"}, {"Leave it.", -1, {}, {}, 0, ""}}}},
        {11, {"Command Hatch", "Final decision gate. If you enter now, the story ends.", {
            {"Launch evacuation protocol.", 15, {"got_keycard_lv2"}, {}, 2, ""},
            {"Stay and hold command.", 16, {}, {}, 0, ""},
            {"Abort.", -1, {}, {}, 0, ""}
        }, 10.0f, 17, "hesitated_final_gate"}},
        {12, {"Officer", "You found us. This gives you Keycard Level 2.", {{"Take card.", -1, {}, {}, 0, "got_keycard_lv2"}}}},
        {13, {"Cache", "Inside is an old mechanical key and emergency codes.", {{"Take key.", -1, {}, {}, 0, "got_mechanical_key"}}}},
        {14, {"Ops AI", "Then you already know this was planned long ago.", {{"Use that intel now.", -1, {}, {}, 0, "bunker_intel_used"}}}},
        {15, {"Ending", "You evacuate with survivors. The submarine sleeps beneath ice.", {{"END: Evacuation", -1, {}, {}, 0, "ending_evac"}}}},
        {16, {"Ending", "You stay behind, lock every hatch, and disappear with the hull.", {{"END: Last Captain", -1, {}, {}, 0, "ending_last_captain"}}}},
        {17, {"Ending", "You hesitated. Systems failed. Floodwater takes the lower decks.", {{"END: Drowned Decision", -1, {}, {}, 0, "ending_drowned"}}}},
    };

    AppMode appMode = AppMode::MainMenu;
    GameState state = GameState::FreeRoam;
    std::vector<std::string> flags;
    std::vector<std::string> dialogueLog;
    std::string endingText;

    std::string currentSceneId = "control_room";
    Vector2 playerWorld{0.0f, 0.0f};
    Vector2 targetWorld{0.0f, 0.0f};
    Vector2 playerVel{0.0f, 0.0f};
    const float maxSpeed = 5.1f;
    const float accel = 17.0f;
    const float friction = 12.0f;
    float clickCooldown = 0.0f;

    int activeDialogueNode = -1;
    float dialogueTimer = 0.0f;
    bool isFading = false;
    float fadeAlpha = 0.0f;
    float fadeDirection = 1.0f;
    std::string pendingScene;
    Vector2 pendingSpawn{0, 0};

    int keycardLevel = 0;
    Vector2 cameraPan{0.0f, 0.0f};
    float cameraAngle = 0.0f;
    float cameraZoom = 34.0f;
    const float maxAngle = 0.55f;
    Vector2 prevMouse = GetMousePosition();
    int frameCounter = 0;
    float storyClock = 0.0f;

    bool running = true;
    while (running && !WindowShouldClose()) {
        frameCounter++;
        float dt = GetFrameTime();
        storyClock += dt;
        if (clickCooldown > 0.0f) clickCooldown = std::max(0.0f, clickCooldown - dt);

        Vector2 mouseNow = GetMousePosition();
        Vector2 mouseDelta = Vector2Subtract(mouseNow, prevMouse);
        prevMouse = mouseNow;

        if (appMode == AppMode::MainMenu) {
            Rectangle startBtn{screenWidth * 0.5f - 180, screenHeight * 0.5f - 20, 360, 54};
            Rectangle quitBtn{screenWidth * 0.5f - 180, screenHeight * 0.5f + 50, 360, 48};
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouseNow, startBtn)) appMode = AppMode::InGame;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouseNow, quitBtn)) running = false;

            BeginDrawing();
            ClearBackground(Color{8, 11, 15, 255});
            DrawRectangle(0, 0, screenWidth, screenHeight, Color{12, 16, 24, 255});
            for (int y = 0; y < screenHeight; y += 3) DrawRectangle(0, y, screenWidth, 1, Color{0, 0, 0, static_cast<unsigned char>(8 + y % 8)});
            DrawRectangle(0, 0, screenWidth, 170, Color{130, 20, 28, 20});
            DrawText("SUBMARINE NOIR", screenWidth / 2 - 180, 132, 48, Color{236, 224, 210, 255});
            DrawText("Vertical stacks, keys, timed choices, branching endings", screenWidth / 2 - 280, 188, 22, Color{177, 191, 204, 240});

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

        if (appMode == AppMode::Ending) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawRectangle(0, 0, screenWidth, screenHeight, Color{12, 14, 20, 255});
            DrawText("ENDING", screenWidth / 2 - 80, 180, 48, Color{238, 198, 156, 255});
            DrawText(endingText.c_str(), screenWidth / 2 - 390, 280, 28, RAYWHITE);
            DrawText("Press left click for Main Menu", screenWidth / 2 - 180, 380, 20, Color{170, 190, 210, 255});
            EndDrawing();
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                appMode = AppMode::MainMenu;
                flags.clear();
                dialogueLog.clear();
                endingText.clear();
                currentSceneId = "control_room";
                playerWorld = Vector2{0.0f, 0.0f};
                targetWorld = playerWorld;
                keycardLevel = 0;
                storyClock = 0.0f;
                state = GameState::FreeRoam;
            }
            continue;
        }

        Scene& scene = scenes[currentSceneId];
        SetFlag(flags, scene.onEnterFlag);

        if (!scene.verticalCutaway && IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            cameraPan = Vector2Add(cameraPan, mouseDelta);
        }
        if (!scene.verticalCutaway && IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            cameraAngle += mouseDelta.x * 0.0065f;
            cameraAngle = std::clamp(cameraAngle, -maxAngle, maxAngle);
        }
        cameraZoom = std::clamp(cameraZoom + GetMouseWheelMove() * 2.0f, 24.0f, 48.0f);

        if (state == GameState::FreeRoam) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && clickCooldown <= 0.0f) {
                Vector2 mouseWorld = ToWorld(scene, mouseNow, baseOrigin, cameraZoom, cameraAngle, cameraPan);
                bool clickedHotspot = false;

                for (const auto& h : scene.hotspots) {
                    if (!InRect(mouseWorld, h.worldRect)) continue;
                    clickedHotspot = true;

                    if (keycardLevel < h.requiredKeycardLevel || (!h.requiredFlag.empty() && !HasFlag(flags, h.requiredFlag))) {
                        if (!h.deniedText.empty()) dialogueLog.push_back("LOCKED: " + h.deniedText);
                        break;
                    }

                    if (h.grantKeycardLevel > 0) {
                        keycardLevel = std::max(keycardLevel, h.grantKeycardLevel);
                        dialogueLog.push_back("Acquired Keycard Level " + std::to_string(h.grantKeycardLevel));
                    }
                    SetFlag(flags, h.grantFlag);

                    if (h.movePlayer) {
                        targetWorld = FindValidTarget(h.moveTarget, scene, playerWorld);
                    } else if (!h.transitionTo.empty()) {
                        pendingScene = h.transitionTo;
                        pendingSpawn = h.spawnWorld;
                        state = GameState::Transition;
                        isFading = true;
                        fadeDirection = 1.0f;
                    } else if (h.dialogueNode >= 0) {
                        activeDialogueNode = h.dialogueNode;
                        dialogueTimer = dialogue[activeDialogueNode].timeLimitSec;
                        state = GameState::Dialogue;
                    }
                    break;
                }

                if (!clickedHotspot) {
                    targetWorld = FindValidTarget(mouseWorld, scene, playerWorld);
                }
                clickCooldown = 0.10f;
            }

            Vector2 toTarget = Vector2Subtract(targetWorld, playerWorld);
            float d = Vector2Length(toTarget);
            if (d > 0.02f) {
                Vector2 desired = Vector2Scale(Vector2Normalize(toTarget), maxSpeed);
                playerVel = Vector2Add(playerVel, Vector2Scale(Vector2Subtract(desired, playerVel), std::min(1.0f, accel * dt)));
            } else {
                playerVel = Vector2Add(playerVel, Vector2Scale(playerVel, -std::min(1.0f, friction * dt)));
            }

            Vector2 step = Vector2Scale(playerVel, dt);
            Vector2 candidate = Vector2Add(playerWorld, step);
            if (!IsBlocked(candidate, scene)) playerWorld = candidate;
            else {
                Vector2 xOnly{playerWorld.x + step.x, playerWorld.y};
                Vector2 yOnly{playerWorld.x, playerWorld.y + step.y};
                if (!IsBlocked(xOnly, scene)) playerWorld = xOnly;
                else if (!IsBlocked(yOnly, scene)) playerWorld = yOnly;
                else playerVel = Vector2{0, 0};
            }
        } else if (state == GameState::Dialogue && activeDialogueNode >= 0) {
            DialogueNode& node = dialogue[activeDialogueNode];
            if (node.timeLimitSec > 0.0f) {
                dialogueTimer -= dt;
                if (dialogueTimer <= 0.0f) {
                    SetFlag(flags, node.timeoutSetFlag);
                    activeDialogueNode = node.timeoutNextNode;
                    if (activeDialogueNode < 0) state = GameState::FreeRoam;
                    else dialogueTimer = dialogue[activeDialogueNode].timeLimitSec;
                }
            }

            for (size_t i = 0; i < node.choices.size(); ++i) {
                Rectangle btn{520, static_cast<float>(screenHeight - 146 + static_cast<int>(i) * 38), static_cast<float>(screenWidth - 560), 30};
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouseNow, btn)) {
                    const Choice& pick = node.choices[i];
                    if (!ChoiceUnlocked(pick, flags, keycardLevel)) {
                        dialogueLog.push_back("Choice locked.");
                        break;
                    }

                    SetFlag(flags, pick.setFlag);
                    if (pick.setFlag == "ending_evac") endingText = "You evacuated with survivors.";
                    if (pick.setFlag == "ending_last_captain") endingText = "You stayed and sealed the hull.";
                    if (pick.setFlag == "ending_drowned") endingText = "You hesitated. Flood took the decks.";

                    dialogueLog.push_back(node.speaker + ": " + node.line);
                    dialogueLog.push_back("YOU: " + pick.text);
                    if (dialogueLog.size() > 14) dialogueLog.erase(dialogueLog.begin(), dialogueLog.begin() + 2);

                    activeDialogueNode = pick.nextNode;
                    if (HasFlag(flags, "ending_evac") || HasFlag(flags, "ending_last_captain") || HasFlag(flags, "ending_drowned")) {
                        appMode = AppMode::Ending;
                        state = GameState::FreeRoam;
                        break;
                    }
                    if (activeDialogueNode < 0) state = GameState::FreeRoam;
                    else dialogueTimer = dialogue[activeDialogueNode].timeLimitSec;
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
                playerVel = Vector2{0, 0};
                fadeDirection = -1.0f;
            } else if (fadeDirection < 0.0f && fadeAlpha <= 0.0f) {
                fadeAlpha = 0.0f;
                isFading = false;
                state = GameState::FreeRoam;
            }
        }

        float flicker = 0.85f + 0.15f * std::sin(frameCounter * 0.09f);
        if ((frameCounter % 160) < 6) flicker *= 0.55f;

        BeginDrawing();
        ClearBackground(BLACK);
        DrawRectangle(0, 0, screenWidth, screenHeight, scene.background);

        if (!scene.verticalCutaway) {
            for (int i = -10; i <= 10; ++i) {
                Vector2 a = ToScreen(scene, Vector2{-9.5f, static_cast<float>(i)}, 0.21f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
                Vector2 b = ToScreen(scene, Vector2{9.5f, static_cast<float>(i)}, 0.21f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
                DrawLineEx(a, b, 1.0f, Color{140, 144, 148, 28});
                Vector2 c = ToScreen(scene, Vector2{static_cast<float>(i), -9.5f}, 0.21f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
                Vector2 d = ToScreen(scene, Vector2{static_cast<float>(i), 9.5f}, 0.21f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
                DrawLineEx(c, d, 1.0f, Color{140, 144, 148, 28});
            }
        }

        float playerDepth = playerWorld.x + playerWorld.y;
        for (const auto& g : scene.solids) if (DepthOf(g) <= playerDepth || scene.verticalCutaway) DrawIsoPrism(g, scene, baseOrigin, cameraZoom, cameraAngle, cameraPan);

        Vector2 p = ToScreen(scene, playerWorld, 0.25f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
        float bob = std::sin(frameCounter * 0.15f) * 1.6f * std::min(1.0f, Vector2Length(playerVel));
        DrawCircleV(Vector2{p.x + 9, p.y + 8}, 12.0f, Color{8, 8, 12, 120});
        DrawCircleV(Vector2{p.x, p.y + bob}, 12.5f, Color{235, 230, 220, 255});
        DrawCircleV(Vector2{p.x, p.y - 2 + bob}, 8.3f, Color{20, 20, 25, 255});

        if (!scene.verticalCutaway) {
            for (const auto& g : scene.solids) if (DepthOf(g) > playerDepth) DrawIsoPrism(g, scene, baseOrigin, cameraZoom, cameraAngle, cameraPan);
        }
        for (const auto& w : scene.wallsAlways) DrawIsoPrism(w, scene, baseOrigin, cameraZoom, cameraAngle, cameraPan);

        if (scene.verticalCutaway) {
            for (int y = 0; y < screenHeight; y += 4) DrawRectangle(0, y, screenWidth, 1, Color{0, 0, 0, 10});
        }

        Vector2 mouseWorld = ToWorld(scene, mouseNow, baseOrigin, cameraZoom, cameraAngle, cameraPan);
        for (const auto& h : scene.hotspots) {
            Vector2 c{h.worldRect.x + h.worldRect.width * 0.5f, h.worldRect.y + h.worldRect.height * 0.5f};
            Vector2 cs = ToScreen(scene, c, 0.25f, baseOrigin, cameraZoom, cameraAngle, cameraPan);
            bool hover = InRect(mouseWorld, h.worldRect);
            DrawCircleV(cs, hover ? 16.0f : 10.0f, hover ? Color{248, 228, 120, 130} : Color{140, 220, 180, 70});
            if (hover) DrawText(h.label.c_str(), static_cast<int>(cs.x + 12), static_cast<int>(cs.y - 9), 16, RAYWHITE);
        }

        for (int y = 0; y < screenHeight; y += 3) DrawRectangle(0, y, screenWidth, 1, Color{0, 0, 0, static_cast<unsigned char>(9 + (y + frameCounter) % 10)});
        DrawRectangle(0, 0, screenWidth, 110, Color{86, 25, 28, static_cast<unsigned char>((10 + std::abs((frameCounter % 90) - 45)) * flicker)});

        DrawRectangle(0, 0, screenWidth, 44, Color{6, 10, 14, 230});
        DrawRectangle(10, 7, 820, 30, Color{18, 24, 30, 200});
        DrawRectangleLinesEx(Rectangle{10, 7, 820, 30}, 1.5f, Color{120, 140, 155, 170});
        DrawText(scene.flavorText.c_str(), 18, 14, 14, Color{210, 220, 224, 240});
        DrawText(TextFormat("KC:%i  Time:%02i", keycardLevel, static_cast<int>(storyClock)), screenWidth - 190, 14, 14, Color{180, 230, 190, 255});

        DrawRectangle(0, screenHeight - 160, screenWidth, 160, Color{8, 10, 14, 195});
        DrawRectangle(14, screenHeight - 154, 470, 148, Color{12, 18, 24, 180});
        DrawRectangleLinesEx(Rectangle{14, static_cast<float>(screenHeight - 154), 470, 148}, 1.5f, Color{110, 135, 150, 170});
        DrawText("DIALOG LOG", 26, screenHeight - 146, 16, Color{233, 204, 149, 255});
        for (size_t i = 0; i < dialogueLog.size(); ++i) DrawText(dialogueLog[i].c_str(), 26, screenHeight - 124 + static_cast<int>(i) * 16, 14, Color{205, 215, 218, 245});

        if (state == GameState::Dialogue && activeDialogueNode >= 0) {
            DialogueNode& node = dialogue[activeDialogueNode];
            DrawRectangle(500, screenHeight - 244, screenWidth - 520, 232, Color{12, 14, 18, 236});
            DrawRectangleLinesEx(Rectangle{500, static_cast<float>(screenHeight - 244), static_cast<float>(screenWidth - 520), 232}, 2.0f, Color{132, 156, 178, 255});
            DrawText(node.speaker.c_str(), 520, screenHeight - 224, 20, Color{247, 183, 124, 255});
            DrawText(node.line.c_str(), 520, screenHeight - 193, 19, RAYWHITE);
            if (node.timeLimitSec > 0.0f) {
                DrawText(TextFormat("TIME LEFT: %.1f", std::max(0.0f, dialogueTimer)), screenWidth - 300, screenHeight - 224, 18, Color{255, 130, 130, 255});
            }
            for (size_t i = 0; i < node.choices.size(); ++i) {
                Rectangle btn{520, static_cast<float>(screenHeight - 146 + static_cast<int>(i) * 38), static_cast<float>(screenWidth - 560), 30};
                bool unlocked = ChoiceUnlocked(node.choices[i], flags, keycardLevel);
                bool hover = unlocked && CheckCollisionPointRec(mouseNow, btn);
                DrawRectangleRec(btn, !unlocked ? Color{52, 48, 48, 255} : (hover ? Color{58, 78, 96, 255} : Color{31, 43, 54, 255}));
                DrawRectangleLinesEx(btn, 1.0f, unlocked ? Color{150, 173, 191, 255} : Color{110, 90, 90, 255});
                DrawText(node.choices[i].text.c_str(), static_cast<int>(btn.x + 10), static_cast<int>(btn.y + 7), 15, unlocked ? RAYWHITE : Color{170, 150, 150, 255});
            }
        }

        DrawRectangle(0, 0, screenWidth, 24, Color{0, 0, 0, 235});
        DrawRectangle(0, screenHeight - 24, screenWidth, 24, Color{0, 0, 0, 235});
        DrawText("LMB interact | RMB pan (iso) | MMB rotate (iso) | Wheel zoom", screenWidth - 560, screenHeight - 18, 12, Color{180, 186, 194, 230});

        if (isFading) DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, fadeAlpha));
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
