#include "raylib_compat.h"

#if !SUBNOIR_HAS_RAYLIB

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
int g_frameCount = 0;
}

void InitWindow(int, int, const char*) {}
void SetTargetFPS(int) {}

bool WindowShouldClose() {
    return g_frameCount++ > 1;
}

float GetFrameTime() {
    return 1.0f / 60.0f;
}

void CloseWindow() {}

Vector2 GetMousePosition() {
    return Vector2{0.0f, 0.0f};
}

bool IsMouseButtonPressed(int) {
    return false;
}

bool CheckCollisionPointRec(Vector2 point, Rectangle rec) {
    return point.x >= rec.x && point.x <= rec.x + rec.width && point.y >= rec.y && point.y <= rec.y + rec.height;
}

Vector2 Vector2Subtract(Vector2 v1, Vector2 v2) {
    return Vector2{v1.x - v2.x, v1.y - v2.y};
}

float Vector2Length(Vector2 v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

Vector2 Vector2Scale(Vector2 v, float scale) {
    return Vector2{v.x * scale, v.y * scale};
}

Vector2 Vector2Normalize(Vector2 v) {
    const float length = Vector2Length(v);
    if (length <= 0.00001f) {
        return Vector2{0.0f, 0.0f};
    }
    return Vector2{v.x / length, v.y / length};
}

Vector2 Vector2Add(Vector2 v1, Vector2 v2) {
    return Vector2{v1.x + v2.x, v1.y + v2.y};
}

float Vector2Distance(Vector2 v1, Vector2 v2) {
    return Vector2Length(Vector2Subtract(v1, v2));
}

void BeginDrawing() {}
void EndDrawing() {}
void ClearBackground(Color) {}
void DrawRectangle(int, int, int, int, Color) {}
void DrawLineEx(Vector2, Vector2, float, Color) {}
void DrawCircleV(Vector2, float, Color) {}
void DrawText(const char*, int, int, int, Color) {}
void DrawRectangleRec(Rectangle, Color) {}
void DrawRectangleLinesEx(Rectangle, float, Color) {}

Color Fade(Color color, float alpha) {
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    color.a = static_cast<unsigned char>(alpha * static_cast<float>(color.a));
    return color;
}

const char* TextFormat(const char* text, ...) {
    static char buffer[1024];
    va_list args;
    va_start(args, text);
    std::vsnprintf(buffer, sizeof(buffer), text, args);
    va_end(args);
    return buffer;
}

#endif
