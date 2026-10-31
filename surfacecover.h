#ifndef SURFACECOVER_H
#define SURFACECOVER_H

#include <vector>

// Покрытие поверхностей сцены: слой снега и влажность.
// Сетка kSize x kSize накрывает квадрат [-kHalfExtent, kHalfExtent]^2
// в плоскости XZ. Результат экспортируется как RGBA8-текстура:
//   R — толщина снега (0..1), G — влажность (0..1).
// Шейдер выбирает ячейку по мировым координатам XZ.
class SurfaceCover
{
public:
    static constexpr int kSize = 128;
    static constexpr float kHalfExtent = 20.0f;

    SurfaceCover();

    void clear();

    // Снежинка достигла поверхности в точке (x, z).
    void addSnow(float x, float z, float amount);

    // Капля дождя достигла поверхности в точке (x, z):
    // поверхность намокает, снег подтаивает.
    void addRain(float x, float z);

    // Высыхание и обновление текстуры.
    void update(float deltaTime, bool snowing);

    const unsigned char* pixels() const { return m_pixels.data(); }

private:
    int cellIndex(float x, float z) const;
    void rebuildPixels();

    std::vector<float> m_snow;
    std::vector<float> m_wet;
    std::vector<unsigned char> m_pixels;
};

#endif // SURFACECOVER_H
