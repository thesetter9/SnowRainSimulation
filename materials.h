#ifndef MATERIALS_H
#define MATERIALS_H

// Материалы поверхностей сцены. От материала зависит и вид (процедурная
// текстура и оптика в шейдере), и поведение осадков (физика покрытия).
//
// Значения качественные: они задают разницу между материалами, а не
// измеренные физические константы. 1.0 = «базовое» поведение.
enum class Material : unsigned char
{
    Grass = 0,
    Stone,
    Wood,
    Brick,
    Asphalt,
    Count
};

constexpr int kMaterialCount = static_cast<int>(Material::Count);

struct MaterialProps
{
    const char* name;

    // --- Физика покрытия (SurfaceCover, ParticleSystem) ---
    float wetGain;   // как быстро поверхность намокает от капель
    float dryRate;   // как быстро высыхает (пористые сохнут дольше)
    float snowGain;  // сколько снега удерживает (тёмный асфальт греется и держит мало)
    float meltRate;  // как быстро дождь подтапливает снег
    float splash;    // вероятность брызг при ударе капли (0..1)

    // --- Оптика (передаётся в шейдер) ---
    float wetDarken; // насколько темнеет во влажном состоянии
    float gloss;     // сила блеска и отражения неба на мокрой поверхности
    float specPower; // острота блика
};

inline const MaterialProps& materialProps(Material m)
{
    //                         wetGain dryRate snowGain meltRate splash | darken gloss  specPow
    static const MaterialProps kProps[kMaterialCount] =
        {
            { "Трава",   0.6f,  0.8f,  1.00f, 0.8f,  0.25f,   0.30f, 0.10f, 16.0f },
            { "Камень",  1.0f,  1.0f,  0.90f, 1.0f,  0.85f,   0.40f, 0.55f, 48.0f },
            { "Дерево",  0.7f,  0.6f,  1.00f, 0.9f,  0.55f,   0.50f, 0.25f, 24.0f },
            { "Кирпич",  0.8f,  0.7f,  0.95f, 1.0f,  0.70f,   0.35f, 0.15f, 20.0f },
            { "Асфальт", 1.3f,  1.3f,  0.60f, 1.5f,  1.00f,   0.35f, 1.00f, 80.0f }
        };

    return kProps[static_cast<int>(m)];
}

#endif // MATERIALS_H
