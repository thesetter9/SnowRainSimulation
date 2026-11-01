#include "collision.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

// ---------------------------------------------------------------------
// 1. Полный перебор: эталон точности и самый медленный вариант.
// ---------------------------------------------------------------------
class BruteForceCollision : public ICollision
{
public:
    explicit BruteForceCollision(const BoxList& boxes)
        : m_boxes(boxes)
    {
    }

    float heightAt(float x, float z) const override
    {
        return boxesHeight(m_boxes, x, z);
    }

    CollisionAlgorithm type() const override
    {
        return CollisionAlgorithm::BruteForce;
    }

private:
    BoxList m_boxes;
};

// ---------------------------------------------------------------------
// 2. Карта высот: сцена заранее «запекается» в двумерный массив.
//    Запрос — одно обращение к памяти. Цена: память R*R*4 байт и
//    погрешность порядка размера ячейки на краях объектов.
// ---------------------------------------------------------------------
class HeightMapCollision : public ICollision
{
public:
    HeightMapCollision(const BoxList& boxes, float halfExtent)
        : m_half(halfExtent)
        , m_cell(2.0f * halfExtent / kResolution)
        , m_map(static_cast<std::size_t>(kResolution) * kResolution)
    {
        // Рельеф в центре каждой ячейки.
        for (int iz = 0; iz < kResolution; ++iz)
        {
            const float z = -m_half + (iz + 0.5f) * m_cell;
            for (int ix = 0; ix < kResolution; ++ix)
            {
                const float x = -m_half + (ix + 0.5f) * m_cell;
                m_map[index(ix, iz)] = terrainBaseHeight(x, z);
            }
        }

        // Объекты: проходим только по ячейкам, которые они накрывают.
        for (const SceneBox& b : boxes)
        {
            const int ix0 = clampIndex((b.x0 + m_half) / m_cell);
            const int ix1 = clampIndex((b.x1 + m_half) / m_cell);
            const int iz0 = clampIndex((b.z0 + m_half) / m_cell);
            const int iz1 = clampIndex((b.z1 + m_half) / m_cell);

            for (int iz = iz0; iz <= iz1; ++iz)
            {
                const float z = -m_half + (iz + 0.5f) * m_cell;
                if (!(z > b.z0 && z < b.z1))
                    continue;

                for (int ix = ix0; ix <= ix1; ++ix)
                {
                    const float x = -m_half + (ix + 0.5f) * m_cell;
                    if (!(x > b.x0 && x < b.x1))
                        continue;

                    float& h = m_map[index(ix, iz)];
                    h = std::max(h, b.height);
                }
            }
        }
    }

    float heightAt(float x, float z) const override
    {
        if (x <= -m_half || x >= m_half || z <= -m_half || z >= m_half)
            return terrainBaseHeight(x, z);

        const int ix = clampIndex((x + m_half) / m_cell);
        const int iz = clampIndex((z + m_half) / m_cell);
        return m_map[index(ix, iz)];
    }

    CollisionAlgorithm type() const override
    {
        return CollisionAlgorithm::HeightMap;
    }

private:
    static constexpr int kResolution = 512; // ячейка ~7.8 см при размере сцены 40 м

    std::size_t index(int ix, int iz) const
    {
        return static_cast<std::size_t>(iz) * kResolution + ix;
    }

    static int clampIndex(float v)
    {
        return std::clamp(static_cast<int>(std::floor(v)), 0, kResolution - 1);
    }

    float m_half;
    float m_cell;
    std::vector<float> m_map;
};

// ---------------------------------------------------------------------
// 3. Равномерная сетка: в каждой ячейке хранится список объектов,
//    которые её пересекают. Запрос проверяет только объекты своей
//    ячейки. Результат точный, как у перебора.
// ---------------------------------------------------------------------
class UniformGridCollision : public ICollision
{
public:
    UniformGridCollision(const BoxList& boxes, float halfExtent)
        : m_half(halfExtent)
        , m_cell(2.0f * halfExtent / kCells)
        , m_boxes(boxes)
        , m_grid(static_cast<std::size_t>(kCells) * kCells)
    {
        for (int i = 0; i < static_cast<int>(m_boxes.size()); ++i)
        {
            const SceneBox& b = m_boxes[i];
            const int cx0 = clampCell((b.x0 + m_half) / m_cell);
            const int cx1 = clampCell((b.x1 + m_half) / m_cell);
            const int cz0 = clampCell((b.z0 + m_half) / m_cell);
            const int cz1 = clampCell((b.z1 + m_half) / m_cell);

            for (int cz = cz0; cz <= cz1; ++cz)
                for (int cx = cx0; cx <= cx1; ++cx)
                    m_grid[cellIndex(cx, cz)].push_back(i);
        }
    }

    float heightAt(float x, float z) const override
    {
        float h = terrainBaseHeight(x, z);

        if (x <= -m_half || x >= m_half || z <= -m_half || z >= m_half)
            return h;

        const int cx = clampCell((x + m_half) / m_cell);
        const int cz = clampCell((z + m_half) / m_cell);

        for (int i : m_grid[cellIndex(cx, cz)])
        {
            const SceneBox& b = m_boxes[i];
            if (x > b.x0 && x < b.x1 && z > b.z0 && z < b.z1)
                h = std::max(h, b.height);
        }
        return h;
    }

    CollisionAlgorithm type() const override
    {
        return CollisionAlgorithm::UniformGrid;
    }

private:
    static constexpr int kCells = 32; // ячейка 1.25 м при размере сцены 40 м

    std::size_t cellIndex(int cx, int cz) const
    {
        return static_cast<std::size_t>(cz) * kCells + cx;
    }

    static int clampCell(float v)
    {
        return std::clamp(static_cast<int>(std::floor(v)), 0, kCells - 1);
    }

    float m_half;
    float m_cell;
    BoxList m_boxes;
    std::vector<std::vector<int>> m_grid;
};

} // namespace

std::unique_ptr<ICollision> makeCollision(CollisionAlgorithm algorithm,
                                          const BoxList& boxes,
                                          float halfExtent)
{
    switch (algorithm)
    {
    case CollisionAlgorithm::HeightMap:
        return std::make_unique<HeightMapCollision>(boxes, halfExtent);
    case CollisionAlgorithm::UniformGrid:
        return std::make_unique<UniformGridCollision>(boxes, halfExtent);
    case CollisionAlgorithm::BruteForce:
    default:
        return std::make_unique<BruteForceCollision>(boxes);
    }
}

const char* collisionName(CollisionAlgorithm algorithm)
{
    switch (algorithm)
    {
    case CollisionAlgorithm::HeightMap:   return "Карта высот";
    case CollisionAlgorithm::UniformGrid: return "Равномерная сетка";
    case CollisionAlgorithm::BruteForce:
    default:                              return "Полный перебор";
    }
}
