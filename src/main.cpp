#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

struct Choice {
    std::string text;
    int nextNode;
    std::string setFlag;
};

struct DialogueNode {
    std::string speaker;
    std::string line;
    std::vector<Choice> choices;
};

struct Hotspot {
    Rectangle area;
    std::string label;
    int dialogueNode;
    std::string transitionTo;
    Vector2 spawnPosition;
};

struct Scene {
    std::string id;
    Color background;
    std::vector<Vector2> walkPolygon;
    std::vector<Hotspot> hotspots;
    std::string flavorText;
};

static bool PointInPolygon(const Vector2& p, const std::vector<Vector2>& poly) {
    bool inside = false;
    if (poly.size() < 3) {
        return false;
    }

    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const bool intersect = ((poly[i].y > p.y) != (poly[j].y > p.y)) &&
                               (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) /
                                           ((poly[j].y - poly[i].y) + 0.0001f) +
                                       poly[i].x);
        if (intersect) {
            inside = !inside;
        }
    }

    return inside;
}

static Vector2 ClampToWalkable(Vector2 desired, const std::vector<Vector2>& polygon) {
    if (PointInPolygon(desired, polygon)) {
        return desired;
    }

    Vector2 fallback = polygon.empty() ? Vector2{0, 0} : polygon.front();
    float bestDist = INFINITY;
    for (const auto& point : polygon) {
        const float dist = Vector2Distance(point, desired);
        if (dist < bestDist) {
            bestDist = dist;
            fallback = point;
        }
    }
    return fallback;
}

enum class GameState {
    FreeRoam,
    Dialogue,
    Transition
};

int main() {
    const int screenWidth = 1366;
    const int screenHeight = 768;
    InitWindow(screenWidth, screenHeight, "Submarine Noir Prototype - raylib");
    SetTargetFPS(60);

    std::unordered_map<std::string, Scene> scenes;

    scenes["control_room"] = Scene{
        "control_room",
        Color{16, 24, 38, 255},
        {{130, 140}, {1230, 140}, {1290, 650}, {160, 700}},
        {
            {Rectangle{980, 220, 180, 140}, "Command Console", 1, "", {0, 0}},
            {Rectangle{70, 260, 100, 220}, "Bulkhead Door", -1, "engine_corridor", {1110, 420}},
            {Rectangle{520, 500, 220, 120}, "Captain's Chair", 4, "", {0, 0}},
        },
        "CONTROL ROOM // humidity 83% // hull strain: stable"
    };

    scenes["engine_corridor"] = Scene{
        "engine_corridor",
        Color{36, 14, 17, 255},
        {{90, 120}, {1240, 140}, {1230, 670}, {110, 660}},
        {
            {Rectangle{1180, 260, 120, 220}, "Return to Control", -1, "control_room", {210, 420}},
            {Rectangle{350, 270, 250, 160}, "Maintenance Hatch", 7, "", {0, 0}},
            {Rectangle{640, 480, 180, 130}, "Crew Journal", 10, "", {0, 0}},
        },
        "ENGINE CORRIDOR // red emergency strips active"
    };

    std::unordered_map<int, DialogueNode> dialogue{
        {1, {"Ops AI", "Captain, sonar picks up movement outside the hull. Want a full scan?", {
            {"Do a silent scan.", 2, "silent_scan"},
            {"Ping active sonar. I want certainty.", 3, "loud_scan"},
            {"Ignore it. We stay dark.", -1, ""}
        }}},
        {2, {"Ops AI", "Silent sweep complete. Heat signatures: uncertain. The ocean is lying tonight.", {
            {"Log it. Prepare security team.", -1, "prep_security"},
            {"Open channel to crew deck.", 5, ""}
        }}},
        {3, {"Ops AI", "Active ping answered. Something answered back.", {
            {"Seal all doors.", 6, "lockdown"},
            {"Keep pinging.", -1, ""}
        }}},
        {4, {"Inner Voice", "The chair is still warm, but nobody sits here anymore.", {
            {"Sit for a moment.", -1, "memory_echo"},
            {"Step away.", -1, ""}
        }}},
        {5, {"Deck Chief", "We're hearing metal scratching through the vents.", {
            {"Arm everyone.", -1, "crew_armed"},
            {"No panic. Stay put.", -1, "crew_calm"}
        }}},
        {6, {"System", "LOCKDOWN INITIATED // Some doors failed to close.", {
            {"Route power to magnetic seals.", -1, "reroute_power"}
        }}},
        {7, {"Mechanic", "Hatch wheel is stuck. Either rust... or pressure from the other side.", {
            {"Force it open.", 8, "force_hatch"},
            {"Leave it.", -1, ""}
        }}},
        {8, {"Narrator", "The hatch opens two centimeters. Warm air exhales like a living thing.", {
            {"Shine flashlight inside.", 9, "light_check"},
            {"Close it now.", -1, ""}
        }}},
        {9, {"Narrator", "Wet footprints lead inward, then stop mid-corridor.", {
            {"Mark area for investigation.", -1, "trace_marked"}
        }}},
        {10, {"Journal", "'Day 41. We have mapped every chamber except one that appears only on old blueprints.'", {
            {"Take journal page.", -1, "journal_page"},
            {"Leave it.", -1, ""}
        }}},
    };

    std::vector<std::string> flags;
    std::vector<std::string> dialogueLog;

    GameState state = GameState::FreeRoam;
    std::string currentSceneId = "control_room";

    Vector2 playerPos{320, 450};
    Vector2 targetPos = playerPos;
    float playerSpeed = 180.0f;

    int activeDialogueNode = -1;

    bool isFading = false;
    float fadeAlpha = 0.0f;
    float fadeDirection = 1.0f;
    std::string pendingScene;
    Vector2 pendingSpawn{0, 0};

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        Scene& scene = scenes[currentSceneId];

        if (state == GameState::FreeRoam) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                const Vector2 mouse = GetMousePosition();
                bool clickedHotspot = false;

                for (const auto& hotspot : scene.hotspots) {
                    if (CheckCollisionPointRec(mouse, hotspot.area)) {
                        clickedHotspot = true;
                        targetPos = ClampToWalkable(
                            Vector2{hotspot.area.x + hotspot.area.width / 2.0f, hotspot.area.y + hotspot.area.height / 2.0f},
                            scene.walkPolygon
                        );

                        if (!hotspot.transitionTo.empty()) {
                            pendingScene = hotspot.transitionTo;
                            pendingSpawn = hotspot.spawnPosition;
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

                if (!clickedHotspot) {
                    targetPos = ClampToWalkable(mouse, scene.walkPolygon);
                }
            }

            const Vector2 delta = Vector2Subtract(targetPos, playerPos);
            const float dist = Vector2Length(delta);
            if (dist > 1.0f) {
                const Vector2 step = Vector2Scale(Vector2Normalize(delta), playerSpeed * dt);
                if (Vector2Length(step) > dist) {
                    playerPos = targetPos;
                } else {
                    playerPos = Vector2Add(playerPos, step);
                }
            }
        } else if (state == GameState::Dialogue && activeDialogueNode >= 0) {
            const DialogueNode& node = dialogue[activeDialogueNode];
            const int baseY = screenHeight - 210;
            for (size_t i = 0; i < node.choices.size(); ++i) {
                Rectangle btn{40, static_cast<float>(baseY + 70 + static_cast<int>(i) * 35), 860, 28};
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), btn)) {
                    const Choice& pick = node.choices[i];
                    if (!pick.setFlag.empty()) {
                        flags.push_back(pick.setFlag);
                    }

                    dialogueLog.push_back(node.speaker + ": " + node.line);
                    dialogueLog.push_back("YOU: " + pick.text);
                    if (dialogueLog.size() > 14) {
                        dialogueLog.erase(dialogueLog.begin(), dialogueLog.begin() + 2);
                    }

                    activeDialogueNode = pick.nextNode;
                    if (activeDialogueNode < 0) {
                        state = GameState::FreeRoam;
                    }
                    break;
                }
            }
        }

        if (state == GameState::Transition && isFading) {
            fadeAlpha += fadeDirection * dt;
            if (fadeDirection > 0.0f && fadeAlpha >= 1.0f) {
                fadeAlpha = 1.0f;
                currentSceneId = pendingScene;
                playerPos = pendingSpawn;
                targetPos = pendingSpawn;
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

        for (size_t i = 0; i < scene.walkPolygon.size(); ++i) {
            const Vector2 p0 = scene.walkPolygon[i];
            const Vector2 p1 = scene.walkPolygon[(i + 1) % scene.walkPolygon.size()];
            DrawLineEx(p0, p1, 2.0f, Color{80, 150, 180, 90});
        }

        DrawCircleV(playerPos, 14.0f, Color{220, 220, 215, 255});
        DrawCircleV(playerPos, 10.0f, Color{20, 20, 25, 255});

        for (const auto& hotspot : scene.hotspots) {
            Color c{100, 200, 140, 45};
            if (CheckCollisionPointRec(GetMousePosition(), hotspot.area)) {
                c = Color{210, 230, 80, 80};
                DrawText(hotspot.label.c_str(), static_cast<int>(hotspot.area.x),
                         static_cast<int>(hotspot.area.y - 18), 16, RAYWHITE);
            }
            DrawRectangleRec(hotspot.area, c);
            DrawRectangleLinesEx(hotspot.area, 1.5f, Color{220, 255, 210, 110});
        }

        DrawRectangle(0, 0, screenWidth, 34, Color{5, 8, 12, 220});
        DrawText(scene.flavorText.c_str(), 14, 9, 16, Color{190, 210, 220, 220});

        DrawRectangle(0, screenHeight - 145, screenWidth, 145, Color{10, 10, 14, 170});
        DrawText("LOG", 14, screenHeight - 138, 16, Color{225, 200, 140, 255});
        for (size_t i = 0; i < dialogueLog.size(); ++i) {
            DrawText(dialogueLog[i].c_str(), 14, screenHeight - 115 + static_cast<int>(i) * 16, 14, Color{200, 210, 210, 240});
        }

        DrawText(TextFormat("Flags: %i", static_cast<int>(flags.size())), screenWidth - 160, 8, 16, Color{150, 220, 180, 255});

        if (state == GameState::Dialogue && activeDialogueNode >= 0) {
            const DialogueNode& node = dialogue[activeDialogueNode];
            DrawRectangle(24, screenHeight - 250, screenWidth - 48, 220, Color{8, 8, 11, 235});
            DrawRectangleLinesEx(Rectangle{24, static_cast<float>(screenHeight - 250), static_cast<float>(screenWidth - 48), 220}, 2.0f, Color{130, 160, 190, 255});
            DrawText(node.speaker.c_str(), 40, screenHeight - 234, 20, Color{240, 180, 120, 255});
            DrawText(node.line.c_str(), 40, screenHeight - 204, 20, RAYWHITE);

            for (size_t i = 0; i < node.choices.size(); ++i) {
                Rectangle btn{40, static_cast<float>(screenHeight - 140 + static_cast<int>(i) * 35), 860, 28};
                const bool hover = CheckCollisionPointRec(GetMousePosition(), btn);
                DrawRectangleRec(btn, hover ? Color{50, 65, 78, 255} : Color{28, 35, 44, 255});
                DrawRectangleLinesEx(btn, 1.0f, Color{130, 150, 168, 255});
                DrawText(node.choices[i].text.c_str(), static_cast<int>(btn.x + 8), static_cast<int>(btn.y + 6), 16, RAYWHITE);
            }
        }

        if (isFading) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, fadeAlpha));
        }

        DrawText("LMB: move/interact | Esc: quit", screenWidth - 320, screenHeight - 22, 12, Color{180, 180, 180, 190});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
