#ifndef SCENEDATA_H
#define SCENEDATA_H

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

// Статический объект сцены: прямоугольная площадка/коробка.
// Один и тот же список используется и для отрисовки (GLWidget),
// и для коллизий частиц (ParticleSystem::surfaceHeight),
// поэтому визуальная сцена всегда совпадает с физической.
struct SceneBox
{
    float x0, x1;
    float z0, z1;
    float height;      // высота верхней грани
    glm::vec3 color;   // базовый цвет (позже заменится текстурой)
};

inline const std::vector<SceneBox>& sceneBoxes()
{
    static const std::vector<SceneBox> boxes =
        {
            { -7.0f, -2.0f, -2.5f,  2.5f, 0.8f, { 0.35f, 0.38f, 0.42f } }, // каменная площадка
            {  3.0f,  7.0f, -2.0f,  2.0f, 1.4f, { 0.42f, 0.25f, 0.12f } }, // деревянная платформа
            { -3.0f, -1.0f, -9.0f, -6.0f, 2.6f, { 0.55f, 0.30f, 0.25f } }, // «дом»
            {  8.0f, 10.0f, -8.0f, -5.5f, 1.0f, { 0.30f, 0.30f, 0.33f } }, // низкий камень
            { -1.0f,  1.5f,  5.0f,  7.0f, 0.5f, { 0.20f, 0.22f, 0.26f } }  // «асфальт»
        };

    return boxes;
}

// Высота базового рельефа без объектов.
inline float terrainBaseHeight(float x, float z)
{
    return 0.15f * std::sin(x * 0.55f) * std::cos(z * 0.45f);
}

// Полная высота поверхности под точкой (x, z).
inline float sceneHeight(float x, float z)
{
    float h = terrainBaseHeight(x, z);

    for (const SceneBox& b : sceneBoxes())
    {
        if (x > b.x0 && x < b.x1 && z > b.z0 && z < b.z1)
            h = std::max(h, b.height);
    }

    return h;
}

#endif // SCENEDATA_H
