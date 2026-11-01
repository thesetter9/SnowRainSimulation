#ifndef COLLISION_H
#define COLLISION_H

#include <memory>

#include "scenedata.h"

// Алгоритмы определения высоты поверхности под точкой (x, z).
// Частица столкнулась с поверхностью, если её y <= heightAt(x, z).
enum class CollisionAlgorithm
{
    BruteForce  = 0, // перебор всех объектов, O(B) на запрос
    HeightMap   = 1, // предрассчитанная карта высот, O(1) на запрос
    UniformGrid = 2  // равномерная сетка объектов, ~O(B / ячеек) на запрос
};

// Паттерн «Стратегия»: ParticleSystem работает только с этим интерфейсом.
class ICollision
{
public:
    virtual ~ICollision() = default;

    // Высота поверхности (рельеф + объекты) под точкой (x, z).
    virtual float heightAt(float x, float z) const = 0;

    virtual CollisionAlgorithm type() const = 0;
};

// halfExtent должен совпадать с SurfaceCover::kHalfExtent (сейчас 20).
std::unique_ptr<ICollision> makeCollision(CollisionAlgorithm algorithm,
                                          const BoxList& boxes,
                                          float halfExtent = 20.0f);

// Название алгоритма для интерфейса и отчётов.
const char* collisionName(CollisionAlgorithm algorithm);

#endif // COLLISION_H
