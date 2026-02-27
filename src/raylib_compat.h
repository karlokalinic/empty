#pragma once

#if __has_include(<raylib.h>)
#include <raylib.h>
#define SUBNOIR_HAS_RAYLIB 1
#else
#define SUBNOIR_HAS_RAYLIB 0

#include <cstdarg>

struct Vector2 {
    float x;
    float y;
};

struct Rectangle {
    float x;
    float y;
    float width;
    float height;
};

struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

inline constexpr Color BLACK{0, 0, 0, 255};
inline constexpr Color RAYWHITE{245, 245, 245, 255};

enum MouseButton {
    MOUSE_LEFT_BUTTON = 0,
};

void InitWindow(int width, int height, const char* title);
void SetTargetFPS(int fps);
bool WindowShouldClose();
float GetFrameTime();
void CloseWindow();

Vector2 GetMousePosition();
bool IsMouseButtonPressed(int button);
bool CheckCollisionPointRec(Vector2 point, Rectangle rec);

Vector2 Vector2Subtract(Vector2 v1, Vector2 v2);
float Vector2Length(Vector2 v);
Vector2 Vector2Scale(Vector2 v, float scale);
Vector2 Vector2Normalize(Vector2 v);
Vector2 Vector2Add(Vector2 v1, Vector2 v2);
float Vector2Distance(Vector2 v1, Vector2 v2);

void BeginDrawing();
void EndDrawing();
void ClearBackground(Color color);

void DrawRectangle(int posX, int posY, int width, int height, Color color);
void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color);
void DrawCircleV(Vector2 center, float radius, Color color);
void DrawText(const char* text, int posX, int posY, int fontSize, Color color);
void DrawRectangleRec(Rectangle rec, Color color);
void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color);

Color Fade(Color color, float alpha);
const char* TextFormat(const char* text, ...);

#endif
