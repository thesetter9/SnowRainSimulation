#ifndef SCENEDATA_H
#define SCENEDATA_H

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#include <glm/glm.hpp>

#include "materials.h"

// Статический объект сцены: прямоугольная площадка/коробка.
// Один и тот же список используется и для отрисовки (GLWidget),
// и для коллизий частиц (через ICollision), поэтому визуальная
// сцена всегда совпадает с физической.
struct SceneBox
{
    float x0, x1;
    float z0, z1;
    float height;     // высота верхней грани
    glm::vec3 color;  // базовый оттенок; поверх него шейдер накладывает текстуру материала
    Material material;
};

using BoxList = std::vector<SceneBox>;

// Высота базового рельефа без объектов.
inline float terrainBaseHeight(float x, float z)
{
    return 0.15f * std::sin(x * 0.55f) * std::cos(z * 0.45f);
}

// Базовая сцена из пяти объектов.
inline const BoxList& defaultSceneBoxes()
{
    static const BoxList boxes =
        {
            { -7.0f, -2.0f, -2.5f,  2.5f, 0.8f, { 0.35f, 0.38f, 0.42f }, Material::Stone },   // каменная площадка
            {  3.0f,  7.0f, -2.0f,  2.0f, 1.4f, { 0.42f, 0.25f, 0.12f }, Material::Wood },    // деревянная платформа
            { -3.0f, -1.0f, -9.0f, -6.0f, 2.6f, { 0.55f, 0.30f, 0.25f }, Material::Brick },   // «дом»
            {  8.0f, 10.0f, -8.0f, -5.5f, 1.0f, { 0.30f, 0.30f, 0.33f }, Material::Stone },   // низкий камень
            { -1.0f,  1.5f,  5.0f,  7.0f, 0.5f, { 0.20f, 0.22f, 0.26f }, Material::Asphalt }  // «асфальт»
        };
    return boxes;
}

// Совместимость со старым кодом.
inline const BoxList& sceneBoxes()
{
    return defaultSceneBoxes();
}

// Случайная сцена из count объектов. Фиксированный seed нужен, чтобы
// эксперименты были воспроизводимы: одинаковый count -> одинаковая сцена.
inline BoxList makeRandomBoxes(int count,
                               unsigned seed = 12345u,
                               float halfExtent = 18.0f)
{
    std::mt19937 rng(seed);
    auto uni = [&rng](float a, float b)
    {
        return std::uniform_real_distribution<float>(a, b)(rng);
    };

    // Палитра и материалы: камень, дерево, кирпич, асфальт.
    const glm::vec3 palette[4] =
        {
            { 0.35f, 0.38f, 0.42f },
            { 0.42f, 0.25f, 0.12f },
            { 0.55f, 0.30f, 0.25f },
            { 0.20f, 0.22f, 0.26f }
        };
    const Material materials[4] =
        {
            Material::Stone,
            Material::Wood,
            Material::Brick,
            Material::Asphalt
        };

    BoxList boxes;
    boxes.reserve(static_cast<std::size_t>(std::max(count, 0)));

    for (int i = 0; i < count; ++i)
    {
        const float w = uni(0.8f, 3.0f);
        const float d = uni(0.8f, 3.0f);
        const float cx = uni(-halfExtent + w * 0.5f, halfExtent - w * 0.5f);
        const float cz = uni(-halfExtent + d * 0.5f, halfExtent - d * 0.5f);

        SceneBox b;
        b.x0 = cx - w * 0.5f;
        b.x1 = cx + w * 0.5f;
        b.z0 = cz - d * 0.5f;
        b.z1 = cz + d * 0.5f;
        b.height = uni(0.4f, 3.5f);

        const glm::vec3 base = palette[i % 4];
        const float k = uni(0.85f, 1.15f);
        b.color = glm::vec3(base.x * k, base.y * k, base.z * k);
        b.material = materials[i % 4];

        boxes.push_back(b);
    }
    return boxes;
}

// Эталонное (полный перебор) вычисление высоты поверхности под (x, z).
inline float boxesHeight(const BoxList& boxes, float x, float z)
{
    float h = terrainBaseHeight(x, z);
    for (const SceneBox& b : boxes)
    {
        if (x > b.x0 && x < b.x1 && z > b.z0 && z < b.z1)
            h = std::max(h, b.height);
    }
    return h;
}

// Карта материалов верхних поверхностей в сетке size x size, накрывающей
// квадрат [-halfExtent, halfExtent]^2 (та же сетка, что у SurfaceCover).
// Клетка получает материал самого высокого объекта над её центром,
// а если над ней ничего нет, то траву (рельеф).
inline std::vector<unsigned char> buildMaterialMap(const BoxList& boxes,
                                                   int size,
                                                   float halfExtent)
{
    std::vector<unsigned char> map(
        static_cast<std::size_t>(size) * size,
        static_cast<unsigned char>(Material::Grass));

    // Рисуем объекты от низких к высоким: более высокий перекрывает.
    std::vector<int> order(boxes.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&boxes](int a, int b)
                     {
                         return boxes[a].height < boxes[b].height;
                     });

    const float cell = 2.0f * halfExtent / static_cast<float>(size);

    auto toIndex = [&](float v)
    {
        return std::clamp(static_cast<int>(std::floor((v + halfExtent) / cell)),
                          0, size - 1);
    };

    for (int idx : order)
    {
        const SceneBox& b = boxes[idx];
        const int ix0 = toIndex(b.x0);
        const int ix1 = toIndex(b.x1);
        const int iz0 = toIndex(b.z0);
        const int iz1 = toIndex(b.z1);

        for (int iz = iz0; iz <= iz1; ++iz)
        {
            const float cz = -halfExtent + (static_cast<float>(iz) + 0.5f) * cell;
            if (!(cz > b.z0 && cz < b.z1))
                continue;

            for (int ix = ix0; ix <= ix1; ++ix)
            {
                const float cx = -halfExtent + (static_cast<float>(ix) + 0.5f) * cell;
                if (cx > b.x0 && cx < b.x1)
                    map[static_cast<std::size_t>(iz) * size + ix] =
                        static_cast<unsigned char>(b.material);
            }
        }
    }

    return map;
}

// Совместимость: высота для базовой сцены.
inline float sceneHeight(float x, float z)
{
    return boxesHeight(defaultSceneBoxes(), x, z);
}

#endif // SCENEDATA_H
