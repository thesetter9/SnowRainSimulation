#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <string>
#include <vector>

#include "collision.h"

// Одна конфигурация эксперимента.
struct BenchmarkConfig
{
    int particles;                // число активных частиц
    CollisionAlgorithm algorithm; // алгоритм коллизий
    int objects;                  // число объектов сцены (0 = базовая, 5 шт.)
};

// Результат замеров для одной конфигурации.
struct BenchmarkRow
{
    BenchmarkConfig config;
    int frames;          // сколько кадров измерено
    double updateMs;     // среднее время ParticleSystem::update (физика + коллизии)
    double paintMs;      // среднее время отрисовки (с glFinish)
    double frameMs;      // среднее время кадра целиком
    double p95FrameMs;   // 95-й процентиль времени кадра
    double fps;          // 1000 / frameMs
    double nsPerQuery;   // чистое время одного запроса heightAt, нс
};

// Автомат бенчмарка. От Qt не зависит: кадры ему «скармливает» GLWidget.
//
// Порядок работы:
//   start() -> цикл: [config() применяется снаружи] -> addFrame() на каждый
//   отрисованный кадр -> когда addFrame вернул true, нужно применить новый config().
//   Когда finished() == true, результаты лежат в rows() / toCsv().
class Benchmark
{
public:
    // weatherId — "snow" или "rain" (попадает в CSV).
    // Внутри на ~2 с блокируется: измеряется чистая стоимость запроса.
    void start(const std::string& weatherId);

    // Прервать без сохранения результатов.
    void stop();

    bool active() const { return m_active; }
    bool finished() const { return m_finished; }

    const BenchmarkConfig& config() const;
    int index() const { return m_index; }
    int total() const { return static_cast<int>(m_configs.size()); }

    // Передать результат одного кадра. Возвращает true, если автомат
    // перешёл к следующей конфигурации (её нужно применить).
    bool addFrame(double updateMs, double paintMs, double frameMs);

    const std::vector<BenchmarkRow>& rows() const { return m_rows; }

    // CSV с разделителем ';' и десятичной точкой.
    std::string toCsv() const;

    // Среднее время одного запроса heightAt (нс) для алгоритма и числа объектов.
    static double measureQueryNs(CollisionAlgorithm algorithm, int objects);

private:
    void resetPhase();

    // Параметры замера: минимум кадров И минимум времени.
    static constexpr int    kWarmupFrames  = 3;
    static constexpr double kWarmupMs      = 300.0;
    static constexpr int    kMeasureFrames = 10;
    static constexpr double kMeasureMs     = 1500.0;

    std::string m_weather;
    std::vector<BenchmarkConfig> m_configs;
    std::vector<double> m_queryNs;   // по одному значению на конфигурацию
    std::vector<BenchmarkRow> m_rows;

    bool m_active = false;
    bool m_finished = false;
    int m_index = 0;

    bool m_warmup = true;
    int m_frames = 0;
    double m_sumFrame = 0.0;
    double m_sumUpdate = 0.0;
    double m_sumPaint = 0.0;
    std::vector<double> m_samples;
};

#endif // BENCHMARK_H
