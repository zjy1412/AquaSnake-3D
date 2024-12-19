#ifndef UI_H
#define UI_H

#define GLM_ENABLE_EXPERIMENTAL

#include <GL/glew.h>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QOpenGLWidget>
#include <QStackedWidget>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "gamewidget.h"

// 游戏内HUD界面
class GameHUD : public QWidget {
    Q_OBJECT
public:
    explicit GameHUD(QWidget *parent = nullptr);
    void updateLength(int length);
    bool getIsPaused() const { return m_isPaused; }

signals:
    void pauseResumeClicked();
    void restartClicked();

private:
    QLabel* lengthLabel;
    QPushButton* pauseResumeButton;
    QPushButton* restartButton;
    bool m_isPaused;
};

// 主菜单界面
class MenuWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit MenuWidget(QWidget *parent = nullptr);
    ~MenuWidget();

signals:
    void startGameClicked();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    void initUI();
    void updateCamera();

    QPushButton* startButton;
    QLabel* titleLabel;
    QTimer* cameraTimer;
    GameWidget* gameWidget;
    
    // 相机参数
    float cameraAngle;
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
};

// 主UI管理器
class UIManager : public QStackedWidget {
    Q_OBJECT
public:
    explicit UIManager(QWidget *parent = nullptr);
    GameHUD* getGameHUD() { return gameHUD; }
    GameWidget* getGameWidget() { return gameWidget; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void startGame();
    void pauseResumeGame();
    void restartGame();

private:
    MenuWidget* menuWidget;
    GameWidget* gameWidget;
    GameHUD* gameHUD;
};

#endif // UI_H 