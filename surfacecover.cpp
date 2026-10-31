#include "surfacecover.h"

#include <algorithm>

SurfaceCover::SurfaceCover()
    : m_snow(kSize * kSize, 0.0f)
    , m_wet(kSize * kSize, 0.0f)
    , m_pixels(kSize * kSize * 4, 0)
{
    clear();
}

void SurfaceCover::clear()
{
    std::fill(m_snow.begin(), m_snow.end(), 0.0f);
    std::fill(m_wet.begin(), m_wet.end(), 0.0f);

    rebuildPixels();
}

int SurfaceCover::cellIndex(float x, float z) const
{
    const float u = (x + kHalfExtent) / (2.0f * kHalfExtent);
    const float v = (z + kHalfExtent) / (2.0f * kHalfExtent);

    if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f)
        return -1;

    const int ix = static_cast<int>(u * kSize);
    const int iz = static_cast<int>(v * kSize);

    return iz * kSize + ix;
}

void SurfaceCover::addSnow(float x, float z, float amount)
{
    const int i = cellIndex(x, z);

    if (i < 0)
        return;

    // На мокрой поверхности снег накапливается хуже (тает).
    const float efficiency = 1.0f - 0.8f * m_wet[i];

    m_snow[i] = std::min(1.0f, m_snow[i] + amount * efficiency);
}

void SurfaceCover::addRain(float x, float z)
{
    const int i = cellIndex(x, z);

    if (i < 0)
        return;

    m_wet[i] = std::min(1.0f, m_wet[i] + 0.08f);
    m_snow[i] = std::max(0.0f, m_snow[i] - 0.02f);
}

void SurfaceCover::update(float deltaTime, bool snowing)
{
    // Поверхность высыхает; в мороз/снегопад быстрее.
    const float dryRate = snowing ? 0.04f : 0.01f;

    for (float& w : m_wet)
        w = std::max(0.0f, w - dryRate * deltaTime);

    rebuildPixels();
}

void SurfaceCover::rebuildPixels()
{
    const std::size_t count = m_snow.size();

    for (std::size_t i = 0; i < count; ++i)
    {
        m_pixels[i * 4 + 0] = static_cast<unsigned char>(
            std::clamp(m_snow[i], 0.0f, 1.0f) * 255.0f + 0.5f);

        m_pixels[i * 4 + 1] = static_cast<unsigned char>(
            std::clamp(m_wet[i], 0.0f, 1.0f) * 255.0f + 0.5f);

        m_pixels[i * 4 + 2] = 0;
        m_pixels[i * 4 + 3] = 255;
    }
}
