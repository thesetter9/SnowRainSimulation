#include "mainwindow.h"
#include "glwidget.h"
#include "collision.h"

#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_glWidget(nullptr)
    , m_typeCombo(nullptr)
    , m_intensitySlider(nullptr)
    , m_windSlider(nullptr)
    , m_intensityLabel(nullptr)
    , m_windLabel(nullptr)
    , m_fpsLabel(nullptr)
{
    setWindowTitle(
        "Моделирование атмосферных осадков"
        );

    resize(1200, 800);

    createInterface();
    createMenus();
}

MainWindow::~MainWindow()
{
}

void MainWindow::createInterface()
{
    QWidget* central =
        new QWidget(this);

    QVBoxLayout* mainLayout =
        new QVBoxLayout(central);

    m_glWidget =
        new GLWidget(central);

    mainLayout->addWidget(
        m_glWidget,
        1
        );

    QWidget* controls =
        new QWidget(central);

    QHBoxLayout* controlsLayout =
        new QHBoxLayout(controls);

    // Тип осадков.
    controlsLayout->addWidget(
        new QLabel("Осадки:")
        );

    m_typeCombo =
        new QComboBox();

    m_typeCombo->addItem(
        "Снег"
        );

    m_typeCombo->addItem(
        "Дождь"
        );

    controlsLayout->addWidget(
        m_typeCombo
        );

    // Интенсивность.
    controlsLayout->addWidget(
        new QLabel("Интенсивность:")
        );

    m_intensitySlider =
        new QSlider(Qt::Horizontal);

    m_intensitySlider->setRange(
        500,
        15000
        );

    m_intensitySlider->setValue(
        5000
        );

    m_intensitySlider->setToolTip(
        "Плотность генерируемых частиц"
        );

    controlsLayout->addWidget(
        m_intensitySlider,
        1
        );

    m_intensityLabel =
        new QLabel("5000");

    controlsLayout->addWidget(
        m_intensityLabel
        );

    // Направление ветра.
    controlsLayout->addWidget(
        new QLabel("Ветер:")
        );

    m_windSlider =
        new QSlider(Qt::Horizontal);

    m_windSlider->setRange(
        0,
        359
        );

    m_windSlider->setValue(
        0
        );

    m_windSlider->setToolTip(
        "Направление ветра в градусах"
        );

    controlsLayout->addWidget(
        m_windSlider,
        1
        );

    m_windLabel =
        new QLabel("0°");

    controlsLayout->addWidget(
        m_windLabel
        );

    // Алгоритм обнаружения столкновений.
    controlsLayout->addWidget(
        new QLabel("Коллизии:")
        );

    QComboBox* collisionCombo =
        new QComboBox();

    const CollisionAlgorithm algorithms[] =
        {
            CollisionAlgorithm::BruteForce,
            CollisionAlgorithm::HeightMap,
            CollisionAlgorithm::UniformGrid
        };

    for (CollisionAlgorithm algorithm : algorithms)
    {
        collisionCombo->addItem(
            QString::fromUtf8(collisionName(algorithm)),
            static_cast<int>(algorithm)
            );
    }

    collisionCombo->setToolTip(
        "Алгоритм поиска поверхности под частицей"
        );

    controlsLayout->addWidget(
        collisionCombo
        );

    // Число объектов на сцене.
    controlsLayout->addWidget(
        new QLabel("Объектов:")
        );

    QComboBox* objectsCombo =
        new QComboBox();

    objectsCombo->addItem("5 (базовая)", 0);

    for (int n : {50, 200, 1000, 3000})
        objectsCombo->addItem(QString::number(n), n);

    objectsCombo->setToolTip(
        "Число статических объектов (для исследования коллизий)"
        );

    controlsLayout->addWidget(
        objectsCombo
        );

    m_fpsLabel =
        new QLabel("FPS: --");

    controlsLayout->addWidget(
        m_fpsLabel
        );

    mainLayout->addWidget(
        controls
        );

    setCentralWidget(
        central
        );

    // Смена типа.
    connect(
        m_typeCombo,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged
            ),
        this,
        [this](int index)
        {
            if (index == 0)
            {
                m_glWidget->setParticleType(
                    ParticleType::SNOW
                    );
            }
            else
            {
                m_glWidget->setParticleType(
                    ParticleType::RAIN
                    );
            }
        }
        );

    // Интенсивность.
    connect(
        m_intensitySlider,
        &QSlider::valueChanged,
        this,
        [this](int value)
        {
            m_intensityLabel->setText(
                QString::number(value)
                );

            m_glWidget->setIntensity(
                value
                );
        }
        );

    // Ветер.
    connect(
        m_windSlider,
        &QSlider::valueChanged,
        this,
        [this](int value)
        {
            m_windLabel->setText(
                QString("%1°").arg(value)
                );

            m_glWidget->setWindAngle(
                value
                );
        }
        );

    // Алгоритм коллизий.
    connect(
        collisionCombo,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged
            ),
        this,
        [this, collisionCombo](int)
        {
            m_glWidget->setCollisionAlgorithm(
                static_cast<CollisionAlgorithm>(
                    collisionCombo->currentData().toInt()
                    )
                );
        }
        );

    // Число объектов на сцене.
    connect(
        objectsCombo,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged
            ),
        this,
        [this, objectsCombo](int)
        {
            m_glWidget->setObjectCount(
                objectsCombo->currentData().toInt()
                );
        }
        );

    // Обновляем FPS примерно раз в 500 мс.
    QTimer* fpsTimer =
        new QTimer(this);

    connect(
        fpsTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            if (m_glWidget->benchmarkRunning())
            {
                m_fpsLabel->setText(
                    QString("Бенчмарк: %1/%2")
                        .arg(m_glWidget->benchmarkIndex() + 1)
                        .arg(m_glWidget->benchmarkTotal())
                    );
                return;
            }

            m_fpsLabel->setText(
                QString("FPS: %1")
                    .arg(
                        m_glWidget->fps(),
                        0,
                        'f',
                        1
                        )
                );
        }
        );

    fpsTimer->start(500);
}

void MainWindow::createMenus()
{
    QMenu* fileMenu =
        menuBar()->addMenu("Файл");

    QAction* exitAction =
        fileMenu->addAction(
            "Выход"
            );

    connect(
        exitAction,
        &QAction::triggered,
        this,
        &QMainWindow::close
        );

    QMenu* researchMenu =
        menuBar()->addMenu("Исследование");

    QAction* startBenchAction =
        researchMenu->addAction(
            "Запустить бенчмарк"
            );

    QAction* stopBenchAction =
        researchMenu->addAction(
            "Остановить бенчмарк"
            );

    stopBenchAction->setEnabled(false);

    connect(
        startBenchAction,
        &QAction::triggered,
        this,
        [this, startBenchAction, stopBenchAction]()
        {
            const auto answer = QMessageBox::question(
                this,
                "Бенчмарк",
                "Будут измерены время физики, отрисовки и FPS для "
                "разных алгоритмов коллизий, числа частиц и объектов "
                "(60 конфигураций, несколько минут).\n\n"
                "Замеры идут для выбранного сейчас типа осадков. "
                "Управление на это время блокируется. Начать?"
                );

            if (answer != QMessageBox::Yes)
                return;

            centralWidget()->setEnabled(false);
            startBenchAction->setEnabled(false);
            stopBenchAction->setEnabled(true);

            m_glWidget->startBenchmark();
        }
        );

    connect(
        stopBenchAction,
        &QAction::triggered,
        this,
        [this]()
        {
            m_glWidget->stopBenchmark();
        }
        );

    // Вызывается и при нормальном завершении, и при остановке.
    m_glWidget->onBenchmarkFinished =
        [this, startBenchAction, stopBenchAction](
            bool completed,
            const std::string& csv)
    {
        centralWidget()->setEnabled(true);
        startBenchAction->setEnabled(true);
        stopBenchAction->setEnabled(false);

        if (!completed)
            return;

        const QString path = QFileDialog::getSaveFileName(
            this,
            "Сохранить результаты бенчмарка",
            "benchmark.csv",
            "CSV (*.csv)"
            );

        if (path.isEmpty())
            return;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            QMessageBox::warning(
                this,
                "Бенчмарк",
                "Не удалось записать файл:\n" + path
                );
            return;
        }

        file.write(csv.data(), static_cast<qint64>(csv.size()));
        file.close();

        QMessageBox::information(
            this,
            "Бенчмарк",
            "Результаты сохранены:\n" + path
            );
    };

    QMenu* helpMenu =
        menuBar()->addMenu("Справка");

    QAction* aboutAction =
        helpMenu->addAction(
            "О программе"
            );

    QAction* helpAction =
        helpMenu->addAction(
            "Как пользоваться"
            );

    connect(
        aboutAction,
        &QAction::triggered,
        this,
        [this]()
        {
            QMessageBox::about(
                this,
                "О программе",
                "Программа моделирования атмосферных "
                "осадков в трехмерном пространстве.\n\n"
                "Реализованы режимы снега и дождя, "
                "физическое движение частиц, гравитация, "
                "сопротивление воздуха, ветер и "
                "столкновение с поверхностями."
                );
        }
        );

    connect(
        helpAction,
        &QAction::triggered,
        this,
        [this]()
        {
            QMessageBox::information(
                this,
                "Управление",
                "Осадки — выбор типа частиц.\n\n"
                "Интенсивность — количество "
                "активных частиц.\n\n"
                "Ветер — направление действия "
                "ветрового потока.\n\n"
                "Коллизии — алгоритм поиска "
                "поверхности под частицей.\n\n"
                "Объектов — число статических "
                "объектов на сцене.\n\n"
                "FPS — текущая частота обновления "
                "сцены.\n\n"
                "Для выполнения требования ТЗ "
                "частота обновления поддерживается "
                "около 60 кадров/с."
                );
        }
        );
}
