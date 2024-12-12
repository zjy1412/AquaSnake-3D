#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <GL/glew.h>  // 确保GLEW最先包含
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTimer>
#include <glm/glm.hpp>  // 确保能找到这个路径
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "snake.h"
#include "obstacle.h"

class GameWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);
    ~GameWidget();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void initShaders();
    void createAquarium();
    void drawAquarium();
    void updateGame();
    void spawnFood();
    void updateCamera();     // 更新摄像机位置
    bool isInAquarium(const glm::vec3& pos) const;  // 改为使用 glm::vec3

    QTimer* gameTimer;
    float rotationAngle;
    float cameraDistance;    // 摄像机距离
    float cameraHeight;      // 摄像机高度
    Snake* snake;
    glm::vec3 foodPosition;  // 改为使用 glm::vec3
    glm::vec3 cameraPos;
    glm::vec3 cameraTarget;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    float aquariumSize;
    bool gameOver;
    std::vector<Obstacle> obstacles;
    void initObstacles();
    void checkCollisions();
    float waterLevel;
    GLuint waterShader;
    void initWaterEffect();
    void drawWater();
    float cameraAngle;    // 添加相机角度
    const float CAMERA_DEFAULT_ANGLE = -30.0f;  // 减小俯视角
    const float DEFAULT_CAMERA_DISTANCE = -15.0f; // 增加相机距离
    const float DEFAULT_CAMERA_HEIGHT = 10.0f;    // 增加相机高度
    const float AQUARIUM_DEFAULT_SIZE = 2000.0f;   // 增大水族箱尺寸到2000
    const float SEGMENT_SIZE = 50.0f;  // 添加这行，用于检测碰撞
    const float MIN_FOOD_DISTANCE = 200.0f;         // 食物最小间距
    const int MAX_OBSTACLES = 50;                  // 最大障碍物数量
    const int MIN_FOOD_COUNT = 50;                 // 场景中最少食物数量
    std::vector<glm::vec3> foodPositions;         // 改为存储多个食物位置

    enum class GameState {
        READY = 0,
        PLAYING = 1,
        PAUSED = 2,
        GAME_OVER = 3
    };
    
    GameState gameState;
    int score;
    bool isValidFoodPosition(const glm::vec3& pos) const;
    void resetGame();
    
    // 相机插值参数
    glm::vec3 targetCameraPos;
    glm::vec3 targetCameraTarget;
    static constexpr float CAMERA_SMOOTH_FACTOR = 0.1f;
    
    // 添加无敌帧相关变量
    int invincibleFrames = 0;  // 当前无敌帧计数

    // 相机设置优化
    struct CameraSettings {
        float distance;      // 基础跟随距离
        float minHeight;     // 最小高度
        float maxHeight;     // 最大高度
        float baseFOV;       // 基础视野角度
        float maxFOV;        // 最大视野角度
        float smoothFactor;  // 平滑因子
    };
    
    // 统一的相机设置，不再分不同模式
    const CameraSettings CAMERA_SETTINGS {
        600.0f,  // 基础距离 - 增加以看到更多内容
        400.0f,  // 最小高度 - 提高基础高度
        800.0f,  // 最大高度 - 增加最大高度
        60.0f,   // 基础FOV - 增大基础视野
        75.0f,   // 最大FOV - 增大最大视野
        0.05f    // 平滑因子
    };
    
    // 相机行为参数
    float currentHeight;              // 当前高度
    float targetHeight;               // 目标高度
    float currentFOV;                 // 当前FOV
    float targetFOV;                  // 目标FOV
    static constexpr float HEIGHT_SMOOTH_FACTOR = 0.02f;  // 高度平滑因子
    static constexpr float FOV_SMOOTH_FACTOR = 0.03f;     // FOV平滑因子
    
    // 视角切换相关变量
    bool isTransitioning = false;          // 是否正在过渡
    glm::vec3 transitionStart;            // 过渡起始位置
    float transitionProgress = 0.0f;       // 过渡进度
    float transitionSpeed = 0.05f;         // 过渡速度

    // 相机位置偏移参数
    static constexpr float SIDE_OFFSET_FACTOR = 0.3f;   // 相机侧向偏移因子
    static constexpr float FORWARD_OFFSET = 300.0f;     // 前方观察点偏移
};

#endif // GAMEWIDGET_H