#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QElapsedTimer>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPoint>
#include <QTimer>
#include <QWheelEvent>
#include <chrono>
#include <functional>
#include <string>

#include <vector>

#include "particlesystem.h"
#include "benchmark.h"


class GLWidget : public QOpenGLWidget,
                 protected QOpenGLFunctions
{
public:
    explicit GLWidget(QWidget* parent = nullptr);
    ~GLWidget();

    void setParticleType(ParticleType type);
    void setIntensity(int value);
    void setWindAngle(int angle);

    // Сила ветра в десятых долях м/с (30 = 3.0 м/с).
    void setWindStrength(int tenths);

    // Убрать снег и влагу со всех поверхностей.
    void resetCover();

    float fps() const;
    void setCollisionAlgorithm(CollisionAlgorithm algorithm);
    void setObjectCount(int count);
    void startBenchmark();
    void stopBenchmark();
    bool benchmarkRunning() const;
    int benchmarkIndex() const;
    int benchmarkTotal() const;
    // completed = false, если остановили вручную
    std::function<void(bool completed, const std::string& csv)> onBenchmarkFinished;
    void initializeRain();
    QOpenGLShaderProgram* m_rainShader = nullptr;
    QOpenGLVertexArrayObject m_rainVao;
    QOpenGLBuffer m_rainQuadVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_rainInstanceVbo{QOpenGLBuffer::VertexBuffer};
    std::vector<float> m_rainInstances;
    int m_rainCount = 0;


protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    // Орбитальная камера: ЛКМ — вращение, колесо — приближение.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void initializeShaders();
    void initializeScene();

    void updateParticles();

    void uploadVertices(
        QOpenGLVertexArrayObject& vao,
        QOpenGLBuffer& vbo,
        const std::vector<float>& vertices
        );

    void uploadParticles();
    void uploadSplashes();
    void uploadCover();

    void drawTerrain(
        const glm::mat4& view,
        const glm::mat4& projection
        );

    void drawParticles(
        const glm::mat4& view,
        const glm::mat4& projection
        );

    QTimer m_timer;
    QElapsedTimer m_elapsedTimer;

    ParticleSystem m_particleSystem;

    QOpenGLShaderProgram* m_shader;

    QOpenGLBuffer m_sceneVbo;
    QOpenGLVertexArrayObject m_sceneVao;

    QOpenGLBuffer m_particleVbo;
    QOpenGLVertexArrayObject m_particleVao;

    QOpenGLBuffer m_splashVbo;
    QOpenGLVertexArrayObject m_splashVao;

    std::vector<float> m_particleVertices;
    std::vector<float> m_splashVertices;

    int m_width;
    int m_height;

    float m_fps;
    int m_frameCounter;
    qint64 m_fpsTime;

    int m_sceneVertexCount;

    // Текстура покрытия поверхностей (снег / влажность).
    GLuint m_coverTexture;

    glm::vec3 m_cameraPosition;

    // Параметры орбитальной камеры (градусы / единицы сцены).
    float m_yaw;
    float m_pitch;
    float m_distance;
    QPoint m_lastMousePos;

    void benchmarkStep();
    void applyBenchmarkConfig();
    void finishBenchmark();
    void restoreAfterBenchmark(bool completed, const std::string& csv = std::string());

    Benchmark m_benchmark;
    bool m_benchApply = false;
    bool m_benchPainted = true;
    double m_benchUpdateMs = 0.0;
    std::chrono::steady_clock::time_point m_benchLastFrame;

    int m_objectCount = 0;
    std::size_t m_savedIntensity = 5000;
    CollisionAlgorithm m_savedAlgorithm = CollisionAlgorithm::BruteForce;
    int m_savedObjects = 0;
    float m_savedYaw = 25.0f, m_savedPitch = 20.0f, m_savedDistance = 26.0f;
};

#endif // GLWIDGET_H
