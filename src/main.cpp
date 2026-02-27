#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Choice
{
    std::string text;
    int nextNode = -1;
    std::string setFlag;
    std::string requiresFlag;
    std::string blocksIfFlag;
    std::string startQuest;
    int composureDelta = 0;
    int crewTrustDelta = 0;
    int threatDelta = 0;
    std::string consequenceLine;
};

struct DialogueNode
{
    std::string speaker;
    std::string line;
    std::vector<Choice> choices;
};

struct Hotspot
{
    Rectangle area{};
    std::string label;
    int dialogueNode = -1;
    std::string transitionTo;
    Vector2 spawnPosition{};
};

struct Scene
{
    std::string id;
    Color topColor{};
    Color bottomColor{};
    Vector2 cameraTarget{};
    Vector2 cameraOffsetNorm{0.5f, 0.5f};
    float cameraZoom = 0.62f;
    std::vector<Vector2> walkPolygon;
    std::vector<Hotspot> hotspots;
    std::string flavorText;
    std::string artDirection;
};

struct WorldRule
{
    std::string code;
    std::string text;
};

enum class QuestState
{
    Locked,
    Active,
    Completed
};

struct QuestObjective
{
    std::string text;
    std::vector<std::string> doneByFlags;
};

struct Quest
{
    std::string id;
    std::string title;
    std::string purpose;
    QuestState state = QuestState::Locked;
    size_t objectiveIndex = 0;
    std::vector<QuestObjective> objectives;
};

struct CommandState
{
    int composure = 60;
    int crewTrust = 55;
    int threat = 30;
};

struct AmbientEvent
{
    std::string id;
    std::string line;
    std::string requiresFlag;
    std::string grantsFlag;
    int minThreat = 0;
    bool fireOnce = true;
};

enum class GameState
{
    FreeRoam,
    Dialogue,
    Transition
};

static bool PointInPolygon(const Vector2 &p, const std::vector<Vector2> &poly)
{
    if (poly.size() < 3)
    {
        return false;
    }

    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
    {
        const bool crosses = ((poly[i].y > p.y) != (poly[j].y > p.y)) &&
                             (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) /
                                            ((poly[j].y - poly[i].y) + 0.0001f) +
                                        poly[i].x);
        if (crosses)
        {
            inside = !inside;
        }
    }

    return inside;
}

static Vector2 ClampToWalkable(Vector2 desired, const std::vector<Vector2> &polygon)
{
    if (PointInPolygon(desired, polygon))
    {
        return desired;
    }

    Vector2 best = polygon.empty() ? Vector2{0.0f, 0.0f} : polygon.front();
    float bestDist = INFINITY;
    for (const auto &p : polygon)
    {
        const float dist = Vector2Distance(p, desired);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = p;
        }
    }
    return best;
}

static bool AddFlag(std::unordered_set<std::string> &flags, const std::string &flag)
{
    if (flag.empty())
    {
        return false;
    }
    return flags.insert(flag).second;
}

static bool ChoiceUnlocked(const Choice &c, const std::unordered_set<std::string> &flags)
{
    if (!c.requiresFlag.empty() && flags.find(c.requiresFlag) == flags.end())
    {
        return false;
    }
    if (!c.blocksIfFlag.empty() && flags.find(c.blocksIfFlag) != flags.end())
    {
        return false;
    }
    return true;
}

static void PushLog(std::vector<std::string> &log, const std::string &line, size_t maxLines = 16)
{
    if (line.empty())
    {
        return;
    }
    log.push_back(line);
    if (log.size() > maxLines)
    {
        const size_t overflow = log.size() - maxLines;
        log.erase(log.begin(), log.begin() + static_cast<std::ptrdiff_t>(overflow));
    }
}

static bool ObjectiveDone(const QuestObjective &objective, const std::unordered_set<std::string> &flags)
{
    for (const auto &f : objective.doneByFlags)
    {
        if (flags.find(f) != flags.end())
        {
            return true;
        }
    }
    return false;
}

static void StartQuest(Quest &q, std::vector<std::string> &log)
{
    if (q.state != QuestState::Locked)
    {
        return;
    }
    q.state = QuestState::Active;
    q.objectiveIndex = 0;
    PushLog(log, "QUEST STARTED // " + q.title);
}

static void ProgressQuest(Quest &q, const std::unordered_set<std::string> &flags, std::vector<std::string> &log)
{
    if (q.state != QuestState::Active)
    {
        return;
    }

    while (q.objectiveIndex < q.objectives.size() &&
           ObjectiveDone(q.objectives[q.objectiveIndex], flags))
    {
        PushLog(log, "OBJECTIVE CLEARED // " + q.objectives[q.objectiveIndex].text);
        ++q.objectiveIndex;
    }

    if (q.objectiveIndex >= q.objectives.size())
    {
        q.state = QuestState::Completed;
        PushLog(log, "QUEST COMPLETE // " + q.title);
    }
}

static const char *QuestStateLabel(QuestState s)
{
    switch (s)
    {
    case QuestState::Locked:
        return "Locked";
    case QuestState::Active:
        return "Active";
    case QuestState::Completed:
        return "Completed";
    default:
        return "Unknown";
    }
}

static int ClampStat(int value)
{
    return std::clamp(value, 0, 100);
}

static void ApplyChoiceImpact(const Choice &choice, CommandState &commandState, std::vector<std::string> &chronicle)
{
    const int prevComposure = commandState.composure;
    const int prevTrust = commandState.crewTrust;
    const int prevThreat = commandState.threat;

    commandState.composure = ClampStat(commandState.composure + choice.composureDelta);
    commandState.crewTrust = ClampStat(commandState.crewTrust + choice.crewTrustDelta);
    commandState.threat = ClampStat(commandState.threat + choice.threatDelta);

    if (commandState.composure != prevComposure || commandState.crewTrust != prevTrust || commandState.threat != prevThreat)
    {
        PushLog(chronicle,
                TextFormat(
                    "SYSTEM SHIFT // C:%+d T:%+d TH:%+d",
                    commandState.composure - prevComposure,
                    commandState.crewTrust - prevTrust,
                    commandState.threat - prevThreat));
    }

    if (!choice.consequenceLine.empty())
    {
        PushLog(chronicle, choice.consequenceLine);
    }
}

static bool ParseQuestState(const std::string &token, QuestState &outState)
{
    if (token == "locked")
    {
        outState = QuestState::Locked;
        return true;
    }
    if (token == "active")
    {
        outState = QuestState::Active;
        return true;
    }
    if (token == "completed")
    {
        outState = QuestState::Completed;
        return true;
    }
    return false;
}

static const char *QuestStateToken(QuestState state)
{
    switch (state)
    {
    case QuestState::Locked:
        return "locked";
    case QuestState::Active:
        return "active";
    case QuestState::Completed:
        return "completed";
    default:
        return "locked";
    }
}

static bool SaveSnapshot(
    const std::string &path,
    const std::string &sceneId,
    Vector2 playerPos,
    Vector2 targetPos,
    const std::unordered_set<std::string> &flags,
    const std::unordered_map<std::string, Quest> &quests,
    const CommandState &commandState)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out)
    {
        return false;
    }

    out << "scene " << sceneId << '\n';
    out << "player " << playerPos.x << ' ' << playerPos.y << '\n';
    out << "target " << targetPos.x << ' ' << targetPos.y << '\n';
    out << "stats " << commandState.composure << ' ' << commandState.crewTrust << ' ' << commandState.threat << '\n';
    for (const auto &flag : flags)
    {
        out << "flag " << flag << '\n';
    }
    for (const auto &questPair : quests)
    {
        out << "quest " << questPair.first << ' '
            << QuestStateToken(questPair.second.state) << ' '
            << questPair.second.objectiveIndex << '\n';
    }
    return true;
}

static bool LoadSnapshot(
    const std::string &path,
    std::unordered_map<std::string, Scene> &scenes,
    std::string &sceneId,
    Vector2 &playerPos,
    Vector2 &targetPos,
    std::unordered_set<std::string> &flags,
    std::unordered_map<std::string, Quest> &quests,
    CommandState &commandState,
    std::vector<std::string> &chronicle)
{
    std::ifstream in(path);
    if (!in)
    {
        PushLog(chronicle, "LOAD FAILED // save file missing");
        return false;
    }

    std::string loadedSceneId = sceneId;
    Vector2 loadedPlayer = playerPos;
    Vector2 loadedTarget = targetPos;
    CommandState loadedState = commandState;
    std::unordered_set<std::string> loadedFlags;
    std::unordered_map<std::string, QuestState> loadedQuestStates;
    std::unordered_map<std::string, size_t> loadedQuestIndices;

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(in, line))
    {
        ++lineNumber;
        if (line.empty())
        {
            continue;
        }

        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "scene")
        {
            iss >> loadedSceneId;
        }
        else if (key == "player")
        {
            iss >> loadedPlayer.x >> loadedPlayer.y;
        }
        else if (key == "target")
        {
            iss >> loadedTarget.x >> loadedTarget.y;
        }
        else if (key == "stats")
        {
            iss >> loadedState.composure >> loadedState.crewTrust >> loadedState.threat;
        }
        else if (key == "flag")
        {
            std::string flag;
            iss >> flag;
            if (!flag.empty())
            {
                loadedFlags.insert(flag);
            }
        }
        else if (key == "quest")
        {
            std::string questId;
            std::string stateToken;
            size_t objectiveIndex = 0;
            iss >> questId >> stateToken >> objectiveIndex;
            QuestState questState{};
            if (!questId.empty() && ParseQuestState(stateToken, questState))
            {
                loadedQuestStates[questId] = questState;
                loadedQuestIndices[questId] = objectiveIndex;
            }
        }
        else
        {
            PushLog(chronicle, "LOAD WARNING // unknown token at line " + std::to_string(lineNumber));
        }
    }

    auto sceneIt = scenes.find(loadedSceneId);
    if (sceneIt == scenes.end())
    {
        PushLog(chronicle, "LOAD FAILED // scene not found in current build");
        return false;
    }

    sceneId = loadedSceneId;
    playerPos = loadedPlayer;
    targetPos = loadedTarget;
    flags = std::move(loadedFlags);
    commandState.composure = ClampStat(loadedState.composure);
    commandState.crewTrust = ClampStat(loadedState.crewTrust);
    commandState.threat = ClampStat(loadedState.threat);

    for (auto &questPair : quests)
    {
        const auto stateIt = loadedQuestStates.find(questPair.first);
        const auto indexIt = loadedQuestIndices.find(questPair.first);
        if (stateIt != loadedQuestStates.end())
        {
            questPair.second.state = stateIt->second;
        }
        if (indexIt != loadedQuestIndices.end())
        {
            questPair.second.objectiveIndex = std::min(indexIt->second, questPair.second.objectives.size());
        }
    }

    PushLog(chronicle, "LOAD COMPLETE // command snapshot restored");
    return true;
}

static unsigned char U8(int value)
{
    return static_cast<unsigned char>(std::clamp(value, 0, 255));
}

static uint32_t HashNoise(int x, int y, int frame)
{
    uint32_t h = static_cast<uint32_t>(x) * 374761393u;
    h += static_cast<uint32_t>(y) * 668265263u;
    h += static_cast<uint32_t>(frame) * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

static Camera2D BuildFixedCamera(const Scene &scene, int screenWidth, int screenHeight)
{
    Camera2D camera{};
    camera.target = scene.cameraTarget;
    camera.offset = Vector2{
        static_cast<float>(screenWidth) * scene.cameraOffsetNorm.x,
        static_cast<float>(screenHeight) * scene.cameraOffsetNorm.y};
    camera.rotation = 0.0f;
    camera.zoom = scene.cameraZoom;
    return camera;
}

static void DrawBackdrop(const Scene &scene, int w, int h, float t)
{
    DrawRectangleGradientV(0, 0, w, h, scene.topColor, scene.bottomColor);

    if (scene.id == "control_room")
    {
        const int drift = static_cast<int>(std::sin(t * 0.9f) * 32.0f);
        DrawCircleGradient(240 + drift, 130, 230.0f, Color{64, 120, 150, 100}, BLANK);
        DrawCircleGradient(w - 160, 170, 180.0f, Color{180, 210, 240, 44}, BLANK);

        for (int i = 0; i < 11; ++i)
        {
            const float x = 90.0f + static_cast<float>(i) * 118.0f;
            const float sway = std::sin(t * 0.5f + static_cast<float>(i) * 0.7f) * 10.0f;
            DrawLineEx(Vector2{x, 126.0f + sway}, Vector2{x + 52.0f, 730.0f}, 2.0f, Color{120, 170, 185, 60});
        }

        DrawRectangle(0, h - 190, w, 190, Color{8, 14, 22, 138});
        DrawRectangle(100, 188, 320, 74, Color{12, 30, 46, 158});
        DrawRectangle(w - 430, 214, 320, 72, Color{12, 30, 46, 148});
    }
    else if (scene.id == "engine_corridor")
    {
        const int pulse = 76 + static_cast<int>((std::sin(t * 3.0f) + 1.0f) * 30.0f);
        DrawRectangle(0, 0, w, 84, Color{86, 18, 20, U8(pulse)});
        DrawRectangle(0, h - 96, w, 96, Color{66, 12, 18, U8(pulse + 16)});

        const int offset = static_cast<int>(std::fmod(t * 32.0f, 88.0f));
        for (int i = -1; i < 18; ++i)
        {
            const int x = i * 88 + offset;
            DrawRectangle(x, 120, 36, h - 240, Color{92, 26, 28, 46});
            DrawLineEx(
                Vector2{static_cast<float>(x + 18), 120.0f},
                Vector2{static_cast<float>(x + 64), static_cast<float>(h - 120)},
                2.0f,
                Color{160, 42, 38, 72});
        }

        DrawCircleGradient(w / 2, 142, 210.0f, Color{220, 55, 48, 40}, BLANK);
    }
    else if (scene.id == "abyss_archive")
    {
        const float sway = std::sin(t * 0.6f) * 26.0f;
        DrawCircleGradient(w / 2, 136, 300.0f, Color{74, 138, 124, 72}, BLANK);
        DrawCircleGradient(w / 2 + static_cast<int>(sway), h / 2 + 24, 230.0f, Color{34, 118, 106, 58}, BLANK);

        for (int i = 0; i < 8; ++i)
        {
            const int y = 128 + i * 64;
            DrawLineEx(
                Vector2{130.0f, static_cast<float>(y)},
                Vector2{static_cast<float>(w - 130), static_cast<float>(y + 8)},
                2.0f,
                Color{90, 168, 154, 38});
        }

        DrawRectangle(224, 170, w - 448, h - 300, Color{8, 28, 30, 116});
        DrawRectangleLines(224, 170, w - 448, h - 300, Color{150, 190, 170, 90});
    }
}

static void DrawFocusLight(const Scene &scene, Vector2 playerPos, float t)
{
    if (scene.id == "engine_corridor")
    {
        DrawCircleGradient(
            static_cast<int>(playerPos.x + 20.0f),
            static_cast<int>(playerPos.y - 24.0f),
            160.0f + std::sin(t * 2.0f) * 8.0f,
            Color{240, 98, 72, 52},
            BLANK);
    }
    else if (scene.id == "abyss_archive")
    {
        DrawCircleGradient(
            static_cast<int>(playerPos.x - 10.0f),
            static_cast<int>(playerPos.y - 26.0f),
            180.0f + std::sin(t * 1.6f) * 10.0f,
            Color{120, 230, 198, 44},
            BLANK);
    }
    else
    {
        DrawCircleGradient(
            static_cast<int>(playerPos.x),
            static_cast<int>(playerPos.y - 24.0f),
            170.0f + std::sin(t * 1.9f) * 9.0f,
            Color{255, 214, 166, 42},
            BLANK);
    }
}

static void DrawSceneParticles(const Scene &scene, int worldWidth, int worldHeight, int frame)
{
    if (scene.id == "control_room")
    {
        for (int i = 0; i < 190; ++i)
        {
            const uint32_t n = HashNoise(i * 17, frame / 2 + i * 31, frame);
            const int x = static_cast<int>(n % static_cast<uint32_t>(worldWidth));
            const int y = static_cast<int>((n / 13u) % static_cast<uint32_t>(worldHeight));
            if ((n & 15u) == 0u)
            {
                DrawCircle(x, y, 1.4f, Color{170, 214, 235, 24});
            }
        }
    }
    else if (scene.id == "engine_corridor")
    {
        for (int i = 0; i < 150; ++i)
        {
            const uint32_t n = HashNoise(i * 19, frame + i * 7, frame);
            const int x = static_cast<int>(n % static_cast<uint32_t>(worldWidth));
            const int y = static_cast<int>((n / 23u) % static_cast<uint32_t>(worldHeight));
            if ((n & 31u) == 0u)
            {
                DrawCircle(x, y, 1.2f, Color{255, 124, 96, 30});
            }
        }
    }
    else
    {
        for (int i = 0; i < 170; ++i)
        {
            const uint32_t n = HashNoise(i * 29, frame + i * 17, frame);
            const int x = static_cast<int>(n % static_cast<uint32_t>(worldWidth));
            const int y = static_cast<int>((n / 29u) % static_cast<uint32_t>(worldHeight));
            if ((n & 23u) == 0u)
            {
                DrawCircle(x, y, 1.3f, Color{162, 228, 210, 30});
            }
        }
    }
}

static void DrawForegroundOcclusion(const Scene &scene, int worldWidth, int worldHeight, float t)
{
    if (scene.id == "control_room")
    {
        DrawRectangle(-20, worldHeight - 420, worldWidth + 40, 480, Color{4, 10, 16, 44});
        DrawRectangle(0, 0, 360, worldHeight, Color{8, 16, 24, 30});
        DrawRectangle(worldWidth - 340, 0, 340, worldHeight, Color{8, 16, 24, 28});
    }
    else if (scene.id == "engine_corridor")
    {
        const int pulse = 40 + static_cast<int>((std::sin(t * 3.4f) + 1.0f) * 16.0f);
        DrawRectangle(0, worldHeight - 380, worldWidth, 420, Color{24, 6, 8, U8(pulse)});
        DrawRectangle(0, 0, 300, worldHeight, Color{16, 6, 8, 35});
        DrawRectangle(worldWidth - 300, 0, 300, worldHeight, Color{16, 6, 8, 35});
    }
    else
    {
        DrawRectangle(0, worldHeight - 430, worldWidth, 460, Color{4, 16, 16, 52});
        DrawRectangle(0, 0, 320, worldHeight, Color{8, 20, 20, 34});
        DrawRectangle(worldWidth - 320, 0, 320, worldHeight, Color{8, 20, 20, 34});
    }
}

static void DrawCinematicFrame(int screenWidth, int screenHeight, float t)
{
    const int topBand = 36;
    const int bottomBand = 52;
    DrawRectangle(0, 0, screenWidth, topBand, Color{2, 2, 4, 230});
    DrawRectangle(0, screenHeight - bottomBand, screenWidth, bottomBand, Color{2, 2, 4, 236});
    DrawRectangleGradientV(
        0,
        topBand - 2,
        screenWidth,
        24,
        Color{0, 0, 0, 120 + static_cast<unsigned char>(std::sin(t * 1.5f) * 12.0f)},
        BLANK);
    DrawRectangleGradientV(
        0,
        screenHeight - bottomBand - 22,
        screenWidth,
        24,
        BLANK,
        Color{0, 0, 0, 140});
}

static void DrawAtmosphere(int w, int h, int frame, float t)
{
    for (int y = 0; y < h; y += 4)
    {
        DrawLine(0, y, w, y, Color{0, 0, 0, 20});
    }

    for (int y = 0; y < h; y += 3)
    {
        for (int x = (y + frame) % 6; x < w; x += 6)
        {
            if ((HashNoise(x, y, frame) & 31u) == 0u)
            {
                DrawPixel(x, y, Color{242, 248, 255, 16});
            }
        }
    }

    const int edgeAlpha = 120 + static_cast<int>(std::sin(t * 1.2f) * 14.0f);
    DrawRectangleGradientH(0, 0, 220, h, Color{0, 0, 0, U8(edgeAlpha)}, BLANK);
    DrawRectangleGradientH(w - 220, 0, 220, h, BLANK, Color{0, 0, 0, U8(edgeAlpha)});
    DrawRectangleGradientV(0, 0, w, 140, Color{0, 0, 0, 102}, BLANK);
    DrawRectangleGradientV(0, h - 140, w, 140, BLANK, Color{0, 0, 0, 112});
}

static void DrawPlayer(Vector2 pos)
{
    DrawEllipse(static_cast<int>(pos.x), static_cast<int>(pos.y + 16.0f), 16.0f, 8.0f, Color{0, 0, 0, 96});

    const Vector2 head{pos.x, pos.y - 14.0f};
    const Vector2 left{pos.x - 14.0f, pos.y + 2.0f};
    const Vector2 right{pos.x + 14.0f, pos.y + 2.0f};
    const Vector2 foot{pos.x, pos.y + 24.0f};

    DrawTriangle(head, right, foot, Color{210, 220, 226, 255});
    DrawTriangle(head, left, foot, Color{120, 140, 156, 255});
    DrawCircleV(Vector2{pos.x, pos.y - 8.0f}, 4.0f, Color{26, 34, 44, 255});
}

static void DrawQuestPanel(const Quest &quest, int w)
{
    const Rectangle panel{static_cast<float>(w - 430), 44.0f, 416.0f, 170.0f};
    DrawRectangleRec(panel, Color{8, 10, 14, 214});
    DrawRectangleLinesEx(panel, 1.6f, Color{120, 154, 170, 208});

    DrawText("PRIMARY QUEST", static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 10.0f), 16, Color{238, 202, 130, 255});
    DrawText(quest.title.c_str(), static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 30.0f), 18, Color{216, 230, 236, 255});
    DrawText(TextFormat("Status: %s", QuestStateLabel(quest.state)),
             static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 56.0f), 16, Color{168, 220, 184, 255});

    if (quest.state == QuestState::Locked)
    {
        DrawText("Lead: inspect Cartography Lens in control room.",
                 static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 86.0f), 15, Color{184, 198, 205, 255});
    }
    else if (quest.state == QuestState::Active && quest.objectiveIndex < quest.objectives.size())
    {
        DrawText("Current objective:",
                 static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 84.0f), 15, Color{193, 206, 208, 255});
        DrawText(quest.objectives[quest.objectiveIndex].text.c_str(),
                 static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 104.0f), 15, Color{212, 222, 226, 255});
    }
    else
    {
        DrawText("Protocol cycle finalized. Route opens for Act II.",
                 static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 86.0f), 15, Color{184, 224, 200, 255});
    }

    DrawText(quest.purpose.c_str(),
             static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 132.0f), 14, Color{145, 174, 188, 240});
}

static void DrawStatBar(
    const char *label,
    int value,
    int x,
    int y,
    int width,
    Color fillColor,
    Color backColor)
{
    const int clamped = ClampStat(value);
    DrawText(label, x, y - 16, 14, Color{200, 214, 222, 240});
    DrawRectangle(x, y, width, 12, backColor);
    DrawRectangle(x, y, (width * clamped) / 100, 12, fillColor);
    DrawRectangleLines(x, y, width, 12, Color{130, 152, 166, 220});
    DrawText(TextFormat("%d", clamped), x + width + 8, y - 2, 14, Color{190, 214, 222, 240});
}

static void DrawCommandPanel(const CommandState &commandState, int x, int y)
{
    DrawRectangle(x, y, 260, 104, Color{8, 10, 14, 210});
    DrawRectangleLines(x, y, 260, 104, Color{116, 144, 162, 220});
    DrawText("COMMAND PROFILE", x + 10, y + 8, 16, Color{236, 198, 134, 255});

    DrawStatBar("Composure", commandState.composure, x + 10, y + 34, 196, Color{112, 204, 198, 245}, Color{24, 42, 44, 220});
    DrawStatBar("Crew Trust", commandState.crewTrust, x + 10, y + 58, 196, Color{138, 196, 255, 245}, Color{24, 34, 48, 220});
    DrawStatBar("Threat", commandState.threat, x + 10, y + 82, 196, Color{238, 92, 92, 245}, Color{56, 22, 22, 220});
}

static void DrawQuestStack(const std::unordered_map<std::string, Quest> &quests, int x, int y)
{
    DrawRectangle(x, y, 290, 110, Color{8, 10, 14, 200});
    DrawRectangleLines(x, y, 290, 110, Color{116, 144, 162, 220});
    DrawText("ACTIVE THREADS", x + 10, y + 8, 16, Color{236, 198, 134, 255});

    int row = 0;
    for (const auto &q : quests)
    {
        if (q.second.state != QuestState::Active)
        {
            continue;
        }
        DrawText(
            TextFormat("- %s", q.second.title.c_str()),
            x + 10,
            y + 34 + row * 20,
            14,
            Color{204, 218, 224, 240});
        ++row;
        if (row >= 3)
        {
            break;
        }
    }

    if (row == 0)
    {
        DrawText("- No active side threads", x + 10, y + 34, 14, Color{164, 182, 194, 240});
    }
}

static void DrawCodex(
    int w,
    int h,
    const std::vector<std::string> &reasons,
    const std::vector<WorldRule> &rules,
    const std::vector<std::string> &pillars)
{
    const Rectangle panel{46.0f, 52.0f, static_cast<float>(w - 92), static_cast<float>(h - 104)};
    DrawRectangleRec(panel, Color{4, 6, 8, 238});
    DrawRectangleLinesEx(panel, 2.0f, Color{138, 174, 190, 210});

    int y = 74;
    DrawText("WORLDFORGE FIELD CODEX", 66, y, 30, Color{237, 218, 158, 255});
    y += 40;
    DrawText("TAB closes codex", 68, y, 16, Color{172, 192, 201, 255});
    y += 34;

    DrawText("Reasons of Existence", 68, y, 22, Color{203, 222, 230, 255});
    y += 30;
    for (const auto &r : reasons)
    {
        DrawText(r.c_str(), 74, y, 18, Color{194, 207, 213, 255});
        y += 24;
    }

    y += 12;
    DrawText("World Rules", 68, y, 22, Color{203, 222, 230, 255});
    y += 30;
    for (const auto &rule : rules)
    {
        DrawText(TextFormat("[%s] %s", rule.code.c_str(), rule.text.c_str()),
                 74, y, 18, Color{197, 212, 216, 255});
        y += 24;
    }

    y += 12;
    DrawText("Design Pillars", 68, y, 22, Color{203, 222, 230, 255});
    y += 30;
    for (const auto &p : pillars)
    {
        DrawText(p.c_str(), 74, y, 18, Color{195, 208, 215, 255});
        y += 24;
    }
}

int main()
{
    const int screenWidth = 1366;
    const int screenHeight = 768;
    const int worldWidth = 3200;
    const int worldHeight = 2000;
    InitWindow(screenWidth, screenHeight, "Worldforge Noir Slice - raylib");
    SetTargetFPS(60);

    std::unordered_map<std::string, Scene> scenes;
    scenes["control_room"] = Scene{
        "control_room",
        Color{11, 26, 39, 255},
        Color{4, 10, 16, 255},
        Vector2{1500.0f, 980.0f},
        Vector2{0.60f, 0.66f},
        0.58f,
        {{128, 138}, {1230, 140}, {1290, 652}, {158, 700}},
        {
            {Rectangle{955, 210, 190, 150}, "Command Console", 1, "", {0.0f, 0.0f}},
            {Rectangle{64, 250, 106, 240}, "Bulkhead Door", -1, "engine_corridor", {1104.0f, 418.0f}},
            {Rectangle{514, 500, 220, 120}, "Captain's Chair", 4, "", {0.0f, 0.0f}},
            {Rectangle{768, 395, 168, 112}, "Cartography Lens", 11, "", {0.0f, 0.0f}},
            {Rectangle{1220, 452, 118, 170}, "Archive Lift", -1, "abyss_archive", {214.0f, 514.0f}},
        },
        "CONTROL ROOM // pressure stable // sonar veil oscillating",
        "ART: rust-cathedral bridge, cobalt bloom, static grain"};

    scenes["engine_corridor"] = Scene{
        "engine_corridor",
        Color{32, 10, 16, 255},
        Color{12, 6, 8, 255},
        Vector2{1600.0f, 1020.0f},
        Vector2{0.53f, 0.69f},
        0.54f,
        {{90, 120}, {1240, 140}, {1230, 670}, {110, 660}},
        {
            {Rectangle{1180, 260, 122, 220}, "Return to Control", -1, "control_room", {210.0f, 420.0f}},
            {Rectangle{346, 264, 260, 168}, "Maintenance Hatch", 7, "", {0.0f, 0.0f}},
            {Rectangle{640, 476, 192, 134}, "Crew Journal", 10, "", {0.0f, 0.0f}},
            {Rectangle{94, 458, 138, 180}, "Archive Valve", -1, "abyss_archive", {1020.0f, 520.0f}},
        },
        "ENGINE CORRIDOR // emergency strips active // heat anomalies +2",
        "ART: crimson hazard rhythm, steel ribs, claustrophobic parallax"};

    scenes["abyss_archive"] = Scene{
        "abyss_archive",
        Color{8, 34, 34, 255},
        Color{4, 14, 14, 255},
        Vector2{1460.0f, 940.0f},
        Vector2{0.64f, 0.63f},
        0.52f,
        {{88, 132}, {1242, 132}, {1248, 670}, {102, 664}},
        {
            {Rectangle{102, 252, 118, 236}, "Return Corridor", -1, "engine_corridor", {1084.0f, 436.0f}},
            {Rectangle{560, 250, 250, 214}, "Reliquary Bell", 13, "", {0.0f, 0.0f}},
            {Rectangle{960, 420, 220, 160}, "Rule Tablet", 14, "", {0.0f, 0.0f}},
        },
        "ABYSS ARCHIVE // lumen algae breathing // bell core synchronized",
        "ART: monastic machinery, teal patina, sacred industrial silhouette"};

    std::unordered_map<int, DialogueNode> dialogue{
        {1, {"Ops AI", "Captain, sonar catches movement around the hull. Your order?", {
                                                                                           {"Run a silent scan.", 2, "silent_scan", "", "", "", 4, 3, -6, "Silent protocol stabilizes the crew feed."},
                                                                                           {"Ping active sonar for certainty.", 3, "loud_scan", "", "", "", -5, -2, 12, "The ping echoes louder than expected across the hull."},
                                                                                           {"Ignore it. Keep us dark.", -1, "stay_dark", "", "", "", -2, -4, 5, "Crew channels fill with unresolved tension."},
                                                                                       }}},
        {2, {"Ops AI", "Silent sweep complete. Heat signatures are fragmented, like memory pieces.", {
                                                                                                         {"Log threat and alert security.", -1, "prep_security", "", "", ""},
                                                                                                         {"Open channel to crew deck.", 5, "", "", "", ""},
                                                                                                     }}},
        {3, {"Ops AI", "Active ping echoed back. Response pattern was not mechanical.", {
                                                                                            {"Seal all doors and run lockdown.", 6, "lockdown", "", "", "", -1, 6, -4, "Bulkhead integrity increases, crew compliance rises."},
                                                                                            {"Keep pinging. I want a map.", -1, "echo_mapping", "", "", "", -4, -3, 8, "Echo turbulence escalates outside the corridor grid."},
                                                                                        }}},
        {4, {"Inner Voice", "The chair is warm. Whoever left knew they would not return.", {
                                                                                               {"Sit for thirty seconds.", -1, "memory_echo", "", "", ""},
                                                                                               {"Step away before it speaks.", -1, "refused_echo", "", "", ""},
                                                                                           }}},
        {5, {"Deck Chief", "Crew hears metal scratching in the vents. They want orders.", {
                                                                                              {"Arm all teams and pair up.", -1, "crew_armed", "", "", ""},
                                                                                              {"No panic. Hold position.", -1, "crew_calm", "", "", ""},
                                                                                          }}},
        {6, {"System", "LOCKDOWN INITIATED // Two forward seals reported partial closure.", {
                                                                                                {"Route power into magnetic rails.", -1, "reroute_power", "", "", ""},
                                                                                            }}},
        {7, {"Mechanic", "Hatch wheel is stuck. Rust explains one thing, breathing explains another.", {
                                                                                                           {"Force it open.", 8, "force_hatch", "", "", "", -4, -2, 10, "Mechanical stress spikes near the hatch seam."},
                                                                                                           {"Leave it sealed for now.", -1, "hatch_delayed", "", "", "", 2, 1, -2, "Delay buys stability but curiosity keeps rising."},
                                                                                                       }}},
        {8, {"Narrator", "The hatch opens two centimeters. Warm air exhales like a sleeping throat.", {
                                                                                                          {"Shine a light inside.", 9, "light_check", "", "", ""},
                                                                                                          {"Close it now.", -1, "hatch_resealed", "", "", ""},
                                                                                                      }}},
        {9, {"Narrator", "Wet footprints continue inward, then stop mid-corridor with no turn.", {
                                                                                                     {"Mark anomaly and map path vectors.", -1, "trace_marked", "", "", "", 2, 3, -1, "Forensic trail logged into tactical routing."},
                                                                                                 }}},
        {10, {"Journal", "'Day 41. Hidden chamber appears when pressure bells align. Ringing can call rescue or predators.'", {
                                                                                                                                  {"Take torn blueprint page.", -1, "journal_page", "", "", ""},
                                                                                                                                  {"Memorize entry and leave.", -1, "journal_memorized", "", "", ""},
                                                                                                                              }}},
        {11, {"Cartographer", "Worldforge Charter awaiting command: review doctrine or authorize protocol.", {
                                                                                                                 {"Read founding reasons.", 12, "", "", "", ""},
                                                                                                                 {"Authorize Null Bell Protocol.", -1, "protocol_authorized", "", "protocol_authorized", "null_bell_protocol", -2, 5, 6, "Protocol armed. Command burden increases."},
                                                                                                                 {"Show world rules.", 14, "", "", "", ""},
                                                                                                             }}},
        {12, {"Cartographer", "Founding reasons: preserve drowned memory, map hostile currents, forge command identity under pressure.", {
                                                                                                                                             {"Commit doctrine to command log.", -1, "reasons_logged", "", "", ""},
                                                                                                                                             {"Then list world rules.", 14, "", "", "", ""},
                                                                                                                                             {"Return to duty.", -1, "", "", "", ""},
                                                                                                                                         }}},
        {13, {"Reliquary Bell", "The brass core hums with distant lungs. One strike broadcasts your position across the trench.", {
                                                                                                                                      {"Strike once and transmit beacon.", 16, "beacon_broadcast", "protocol_authorized", "", "signal_triangulation", -3, -1, 16, "Beacon flare confirms your location to unknown listeners."},
                                                                                                                                      {"Stay silent and profile resonance.", -1, "bell_profiled", "", "", "", 3, 2, -3, "Spectral profile captured with minimal exposure."},
                                                                                                                                      {"Leave it untouched.", -1, "bell_ignored", "", "", "", 1, -1, -1, "Silence preserved, but actionable data remains low."},
                                                                                                                                  }}},
        {14, {"Archivist Tablet", "Rules: never ping twice, never open two hatches, never name the unknown, never waste heat, never flood with light.", {
                                                                                                                                                            {"Seal rules into doctrine.", -1, "world_rules_logged", "", "", ""},
                                                                                                                                                            {"Understood. Move.", -1, "", "", "", ""},
                                                                                                                                                            {"Run triangulation protocol on received signal.", 18, "", "beacon_broadcast", "", "", 0, 2, 4, "Archive math routes the foreign signal through old trench maps."},
                                                                                                                                                        }}},
        {16, {"System", "Beacon pulse sent. External reply arrived in 4.2 seconds from an unmapped source.", {
                                                                                                                 {"Prepare to receive unknown contact.", 17, "prepare_contact", "", "", "", -1, 1, 6, "Open channel. An unknown cadence enters command audio."},
                                                                                                                 {"Cut exterior lights and wait.", -1, "exterior_dark", "", "", "", 2, 0, -2, "Exterior profile minimized; signal remains faint."},
                                                                                                             }}},
        {17, {"Unknown Contact", "Designation requested. Provide protocol identity.", {
                                                                                          {"Respond with numeric protocol only.", -1, "contact_tagged", "", "", "", 2, 3, -1, "Contact accepts numbered format and pauses."},
                                                                                          {"Use crew names to establish trust.", -1, "rule_break_name", "", "", "", -4, 1, 10, "Rule break logged. Contact audio sharpens."},
                                                                                          {"Terminate channel immediately.", -1, "channel_terminated", "", "", "", 1, -3, -3, "Channel killed before identity exchange."},
                                                                                      }}},
        {18, {"Triangulation Console", "Signal overlays reveal three impossible source points in one chamber.", {
                                                                                                                    {"Tag all three sources as mirrored echo.", -1, "triangulation_done", "", "", "", 1, 2, 1, "Map layer updated: mirrored echo geometry confirmed."},
                                                                                                                    {"Discard data as sensor corruption.", -1, "triangulation_discarded", "", "", "", -2, -2, 3, "Archive marks data unreliable. Crew disputes decision."},
                                                                                                                }}},
    };

    std::unordered_map<std::string, Quest> quests;
    quests["null_bell_protocol"] = Quest{
        "null_bell_protocol",
        "Null Bell Protocol",
        "Purpose: Decide whether humanity survives by silence or by signal.",
        QuestState::Locked,
        0,
        {
            {"Authorize protocol at Cartography Lens.", {"protocol_authorized"}},
            {"Investigate and mark hatch anomaly.", {"trace_marked"}},
            {"Recover hidden blueprint fragment.", {"journal_page"}},
            {"Commit strategy: lockdown or beacon.", {"lockdown", "beacon_broadcast"}},
        },
    };
    quests["signal_triangulation"] = Quest{
        "signal_triangulation",
        "Signal Triangulation",
        "Purpose: Verify whether the reply is a rescue channel, mirrored echo, or hostile lure.",
        QuestState::Locked,
        0,
        {
            {"Broadcast one sanctioned beacon pulse.", {"beacon_broadcast"}},
            {"Stabilize unknown-contact exchange.", {"contact_tagged", "channel_terminated"}},
            {"Resolve triangulation inference in archive.", {"triangulation_done", "triangulation_discarded"}},
        },
    };

    const std::vector<std::string> reasons{
        "1. Preserve collective memory after surface data collapse.",
        "2. Translate abyss signals into navigable command knowledge.",
        "3. Forge leaders who stay human under pressure horror."};

    const std::vector<WorldRule> rules{
        {"R1", "Never ping active sonar twice in one cycle."},
        {"R2", "Never open two sealed hatches simultaneously."},
        {"R3", "Unknown voices receive numbers, never names."},
        {"R4", "Heat is evidence; cold zones require confirmation."},
        {"R5", "Light is bait. Illuminate only what you must."},
        {"R6", "Every breach report is true until disproven."},
    };

    const std::vector<std::string> pillars{
        "A. Rust Cathedral Geometry: sacred framing in industrial steel.",
        "B. Cyan vs Amber Lighting: bioluminescent cold against human warmth.",
        "C. Compression Horror: narrow corridors then abyssal volume reveal.",
        "D. Analog Imperfection: grain, scanlines, slight signal instability.",
        "E. Story-through-machines: every console acts as a character.",
    };

    std::unordered_set<std::string> flags;
    std::vector<std::string> chronicle;
    CommandState commandState{};
    const std::string savePath = "worldforge_save.txt";
    std::vector<AmbientEvent> ambientEvents{
        {"hull_groan", "AMBIENT // Hull groan translated as low-frequency speech.", "silent_scan", "event_hull_groan", 10, true},
        {"crew_prayer", "CREW FEED // Prayer loops detected in lower deck comms.", "protocol_authorized", "event_crew_prayer", 20, true},
        {"cold_spike", "SENSOR // Sudden cold pocket intersects mapped corridor.", "trace_marked", "event_cold_spike", 25, true},
        {"echo_shift", "SONAR // Returning echo now matches partial crew cadence.", "beacon_broadcast", "event_echo_shift", 35, true},
    };
    float ambientTimer = 0.0f;
    PushLog(chronicle, "WORLD READY // Doctrine loaded");

    GameState state = GameState::FreeRoam;
    std::string currentSceneId = "control_room";

    Vector2 playerPos{820.0f, 500.0f};
    Vector2 targetPos = playerPos;
    const float playerSpeed = 180.0f;

    int activeDialogueNode = -1;
    bool showCodex = false;
    bool debugVisuals = false;

    bool isFading = false;
    float fadeAlpha = 0.0f;
    float fadeDirection = 1.0f;
    std::string pendingScene;
    Vector2 pendingSpawn{0.0f, 0.0f};

    int frameCounter = 0;

    while (!WindowShouldClose())
    {
        ++frameCounter;
        const float dt = GetFrameTime();
        const float t = static_cast<float>(GetTime());
        auto sceneIt = scenes.find(currentSceneId);
        if (sceneIt == scenes.end())
        {
            PushLog(chronicle, "SCENE ERROR // fallback to control_room");
            currentSceneId = "control_room";
            sceneIt = scenes.find(currentSceneId);
            if (sceneIt == scenes.end())
            {
                PushLog(chronicle, "SCENE ERROR // control_room missing, aborting");
                break;
            }
        }
        Scene &scene = sceneIt->second;
        const Camera2D camera = BuildFixedCamera(scene, screenWidth, screenHeight);
        const Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);

        if (IsKeyPressed(KEY_TAB))
        {
            showCodex = !showCodex;
        }
        if (IsKeyPressed(KEY_F3))
        {
            debugVisuals = !debugVisuals;
        }
        if (IsKeyPressed(KEY_F5))
        {
            if (SaveSnapshot(savePath, currentSceneId, playerPos, targetPos, flags, quests, commandState))
            {
                PushLog(chronicle, "SAVE COMPLETE // worldforge_save.txt");
            }
            else
            {
                PushLog(chronicle, "SAVE FAILED // cannot write snapshot");
            }
        }
        if (IsKeyPressed(KEY_F9))
        {
            LoadSnapshot(savePath, scenes, currentSceneId, playerPos, targetPos, flags, quests, commandState, chronicle);
        }

        ambientTimer += dt;
        if (ambientTimer >= 8.0f)
        {
            ambientTimer = 0.0f;
            for (auto &event : ambientEvents)
            {
                if (event.fireOnce && flags.find(event.grantsFlag) != flags.end())
                {
                    continue;
                }
                if (!event.requiresFlag.empty() && flags.find(event.requiresFlag) == flags.end())
                {
                    continue;
                }
                if (commandState.threat < event.minThreat)
                {
                    continue;
                }
                PushLog(chronicle, event.line);
                AddFlag(flags, event.grantsFlag);
                commandState.threat = ClampStat(commandState.threat + 2);
                break;
            }
        }
        for (auto &q : quests)
        {
            ProgressQuest(q.second, flags, chronicle);
        }

        if (state == GameState::FreeRoam)
        {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                bool clickedHotspot = false;

                for (const auto &hotspot : scene.hotspots)
                {
                    if (!CheckCollisionPointRec(mouseWorld, hotspot.area))
                    {
                        continue;
                    }

                    clickedHotspot = true;
                    targetPos = ClampToWalkable(
                        Vector2{hotspot.area.x + hotspot.area.width * 0.5f, hotspot.area.y + hotspot.area.height * 0.5f},
                        scene.walkPolygon);

                    if (!hotspot.transitionTo.empty())
                    {
                        pendingScene = hotspot.transitionTo;
                        pendingSpawn = hotspot.spawnPosition;
                        state = GameState::Transition;
                        isFading = true;
                        fadeDirection = 1.0f;
                    }
                    else if (hotspot.dialogueNode >= 0)
                    {
                        activeDialogueNode = hotspot.dialogueNode;
                        state = GameState::Dialogue;
                    }
                    break;
                }

                if (!clickedHotspot)
                {
                    targetPos = ClampToWalkable(mouseWorld, scene.walkPolygon);
                }
            }

            const Vector2 delta = Vector2Subtract(targetPos, playerPos);
            const float dist = Vector2Length(delta);
            if (dist > 1.0f)
            {
                const Vector2 step = Vector2Scale(Vector2Normalize(delta), playerSpeed * dt);
                playerPos = (Vector2Length(step) > dist) ? targetPos : Vector2Add(playerPos, step);
            }
        }
        else if (state == GameState::Dialogue && activeDialogueNode >= 0)
        {
            const auto nodeIt = dialogue.find(activeDialogueNode);
            if (nodeIt == dialogue.end())
            {
                state = GameState::FreeRoam;
                activeDialogueNode = -1;
            }
            else
            {
                const DialogueNode &node = nodeIt->second;
                for (size_t i = 0; i < node.choices.size(); ++i)
                {
                    const Rectangle btn{
                        46.0f,
                        static_cast<float>(screenHeight - 156 + static_cast<int>(i) * 36),
                        static_cast<float>(screenWidth - 92),
                        30.0f};

                    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
                        !CheckCollisionPointRec(GetMousePosition(), btn))
                    {
                        continue;
                    }

                    const Choice &pick = node.choices[i];
                    if (!ChoiceUnlocked(pick, flags))
                    {
                        PushLog(chronicle, "LOCKED CHOICE // requirement or rule block active");
                        continue;
                    }

                    PushLog(chronicle, node.speaker + ": " + node.line);
                    PushLog(chronicle, "YOU: " + pick.text);

                    if (AddFlag(flags, pick.setFlag))
                    {
                        PushLog(chronicle, "FLAG GAINED // " + pick.setFlag);
                    }

                    ApplyChoiceImpact(pick, commandState, chronicle);

                    if (!pick.startQuest.empty())
                    {
                        auto qIt = quests.find(pick.startQuest);
                        if (qIt != quests.end())
                        {
                            StartQuest(qIt->second, chronicle);
                        }
                    }

                    for (auto &q : quests)
                    {
                        ProgressQuest(q.second, flags, chronicle);
                    }

                    activeDialogueNode = pick.nextNode;
                    if (activeDialogueNode < 0)
                    {
                        state = GameState::FreeRoam;
                    }
                    break;
                }
            }
        }

        if (state == GameState::Transition && isFading)
        {
            fadeAlpha += fadeDirection * dt;
            if (fadeDirection > 0.0f && fadeAlpha >= 1.0f)
            {
                fadeAlpha = 1.0f;
                if (scenes.find(pendingScene) == scenes.end())
                {
                    PushLog(chronicle, "TRANSITION FAILED // target scene missing");
                    pendingScene = currentSceneId;
                    pendingSpawn = playerPos;
                }
                currentSceneId = pendingScene;
                playerPos = pendingSpawn;
                targetPos = pendingSpawn;
                fadeDirection = -1.0f;
            }
            else if (fadeDirection < 0.0f && fadeAlpha <= 0.0f)
            {
                fadeAlpha = 0.0f;
                isFading = false;
                state = GameState::FreeRoam;
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode2D(camera);

        DrawBackdrop(scene, worldWidth, worldHeight, t);
        DrawSceneParticles(scene, worldWidth, worldHeight, frameCounter);

        if (debugVisuals)
        {
            for (size_t i = 0; i < scene.walkPolygon.size(); ++i)
            {
                const Vector2 a = scene.walkPolygon[i];
                const Vector2 b = scene.walkPolygon[(i + 1) % scene.walkPolygon.size()];
                DrawLineEx(a, b, 2.0f, Color{88, 170, 175, 72});
            }
        }

        DrawFocusLight(scene, playerPos, t);
        DrawPlayer(playerPos);

        for (const auto &hotspot : scene.hotspots)
        {
            const bool hover = CheckCollisionPointRec(mouseWorld, hotspot.area);
            const Vector2 center{
                hotspot.area.x + hotspot.area.width * 0.5f,
                hotspot.area.y + hotspot.area.height * 0.5f};

            if (state == GameState::FreeRoam && !hover)
            {
                const float pulseRadius = 10.0f + std::sin(t * 2.4f + center.x * 0.01f) * 2.0f;
                DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), pulseRadius, Color{200, 216, 196, 36});
            }

            if (hover)
            {
                DrawCircleGradient(
                    static_cast<int>(center.x),
                    static_cast<int>(center.y),
                    62.0f,
                    Color{255, 236, 188, 34},
                    BLANK);
                DrawText(
                    hotspot.label.c_str(),
                    static_cast<int>(hotspot.area.x),
                    static_cast<int>(hotspot.area.y - 18.0f),
                    16,
                    Color{245, 242, 226, 255});
            }

            if (debugVisuals)
            {
                const Color fill = hover ? Color{230, 215, 120, 64} : Color{120, 180, 162, 22};
                const Color line = hover ? Color{232, 228, 166, 190} : Color{180, 220, 204, 90};
                DrawRectangleRec(hotspot.area, fill);
                DrawRectangleLinesEx(hotspot.area, 1.2f, line);
            }
        }

        DrawForegroundOcclusion(scene, worldWidth, worldHeight, t);

        EndMode2D();

        DrawAtmosphere(screenWidth, screenHeight, frameCounter, t);
        DrawCinematicFrame(screenWidth, screenHeight, t);

        DrawRectangle(0, 0, screenWidth, 38, Color{3, 5, 8, 220});
        DrawText(scene.flavorText.c_str(), 14, 8, 17, Color{198, 216, 225, 240});
        DrawText(scene.artDirection.c_str(), 14, 30, 13, Color{146, 174, 188, 210});
        DrawText("TAB: codex | F3: debug", screenWidth - 260, 10, 16, Color{185, 205, 214, 220});

        const Quest &primaryQuest = quests.at("null_bell_protocol");
        DrawQuestPanel(primaryQuest, screenWidth);
        DrawText(TextFormat("Flags: %i", static_cast<int>(flags.size())),
                 screenWidth - 100, 190, 15, Color{160, 225, 188, 255});

        DrawRectangle(0, screenHeight - 148, screenWidth, 148, Color{8, 10, 14, 190});
        DrawText("CHRONICLE", 14, screenHeight - 140, 16, Color{238, 198, 132, 255});
        const size_t visibleLines = 7;
        const size_t start = (chronicle.size() > visibleLines) ? chronicle.size() - visibleLines : 0;
        for (size_t i = start; i < chronicle.size(); ++i)
        {
            const int row = static_cast<int>(i - start);
            DrawText(chronicle[i].c_str(), 14, screenHeight - 118 + row * 18, 15, Color{198, 208, 214, 246});
        }

        if (state == GameState::Dialogue && activeDialogueNode >= 0)
        {
            const auto it = dialogue.find(activeDialogueNode);
            if (it != dialogue.end())
            {
                const DialogueNode &node = it->second;
                const Rectangle panel{30.0f, static_cast<float>(screenHeight - 270), static_cast<float>(screenWidth - 60), 244.0f};
                DrawRectangleRec(panel, Color{7, 8, 10, 236});
                DrawRectangleLinesEx(panel, 1.8f, Color{125, 157, 180, 255});

                DrawText(node.speaker.c_str(),
                         static_cast<int>(panel.x + 16.0f), static_cast<int>(panel.y + 14.0f), 22,
                         Color{246, 188, 128, 255});
                DrawText(node.line.c_str(),
                         static_cast<int>(panel.x + 16.0f), static_cast<int>(panel.y + 46.0f), 19, RAYWHITE);

                for (size_t i = 0; i < node.choices.size(); ++i)
                {
                    const Choice &c = node.choices[i];
                    const bool unlocked = ChoiceUnlocked(c, flags);
                    const Rectangle btn{
                        46.0f,
                        static_cast<float>(screenHeight - 156 + static_cast<int>(i) * 36),
                        static_cast<float>(screenWidth - 92),
                        30.0f};
                    const bool hover = CheckCollisionPointRec(GetMousePosition(), btn);

                    const Color base = !unlocked ? Color{20, 20, 24, 200}
                                                 : (hover ? Color{58, 76, 88, 255} : Color{32, 42, 52, 255});
                    const Color border = !unlocked ? Color{72, 72, 82, 200} : Color{132, 154, 172, 255};
                    DrawRectangleRec(btn, base);
                    DrawRectangleLinesEx(btn, 1.0f, border);

                    const std::string label = unlocked ? c.text : (c.text + " [LOCKED]");
                    const Color textColor = unlocked ? RAYWHITE : Color{130, 130, 142, 255};
                    DrawText(label.c_str(), static_cast<int>(btn.x + 8.0f), static_cast<int>(btn.y + 6.0f), 16, textColor);
                }
            }
        }

        if (showCodex)
        {
            DrawCodex(screenWidth, screenHeight, reasons, rules, pillars);
        }

        if (isFading)
        {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, fadeAlpha));
        }

        DrawText("LMB: move/interact/choose | fixed camera | ESC: quit", screenWidth - 430, screenHeight - 20, 12, Color{182, 182, 182, 210});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
