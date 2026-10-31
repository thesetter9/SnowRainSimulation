#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class GLWidget;
class QComboBox;
class QSlider;
class QLabel;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    void createInterface();
    void createMenus();

    GLWidget* m_glWidget;

    QComboBox* m_typeCombo;
    QSlider* m_intensitySlider;
    QSlider* m_windSlider;

    QLabel* m_intensityLabel;
    QLabel* m_windLabel;
    QLabel* m_fpsLabel;
};

#endif // MAINWINDOW_H
