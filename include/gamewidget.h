#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#define GLM_ENABLE_EXPERIMENTAL

#include <GL/glew.h> 
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTimer>
#include <glm/glm.hpp>  
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>  
#include "snake.h"
#include "obstacle.h"
#include "food.h"
#include "water.h"  

// 前向声明
class MenuWidget;

class GameWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

    // 添加友元类声明
    friend class MenuWidget;

public:
    explicit GameWidget(QWidget *parent = nullptr);
    ~GameWidget();
    void resetGame();
    void pauseGame();
    void resumeGame();
    int getScore() const { return score; }
    int getSnakeLength() const { return snake ? snake->getBody().size() : 3; }
    bool isGamePaused() const { return gameState == GameState::PAUSED; }
    
    // 添加访问器方法
    float getAquariumSize() const { return aquariumSize; }
    const glm::mat4& getViewMatrix() const { return viewMatrix; }
    const glm::mat4& getProjectionMatrix() const { return projectionMatrix; }
    void setViewMatrix(const glm::mat4& matrix) { viewMatrix = matrix; }
    void setProjectionMatrix(const glm::mat4& matrix) { projectionMatrix = matrix; }

signals:
    void scoreChanged(int newScore);
    void lengthChanged(int newLength);
    void gameOver();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    void initShaders();
    void createAquarium();
    void drawAquarium();
    void updateGame();
    void spawnFood();
    void updateCamera();     // 更新摄像机位置
    bool isInAquarium(const glm::vec3& pos) const;  

    QTimer* gameTimer;
    float rotationAngle;
    float cameraDistance;    // 摄像机离
    float cameraHeight;      // 摄像机高度
    Snake* snake;
    glm::vec3 cameraPos;
    glm::vec3 cameraTarget;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    float aquariumSize;
    bool isGameOver;  // 改名以避免与信号冲突
    std::vector<Obstacle> obstacles;
    void initObstacles();
    void checkCollisions();
    float waterLevel;
    GLuint waterShader;
    void initWaterEffect();
    void drawWater();
    float cameraAngle;    // 添加相机角度
    const float CAMERA_DEFAULT_ANGLE = -30.0f;  
    const float DEFAULT_CAMERA_DISTANCE = -20.0f;
    const float DEFAULT_CAMERA_HEIGHT = 15.0f;
    const float AQUARIUM_DEFAULT_SIZE = 5000.0f;
    const float SEGMENT_SIZE = 100.0f;
    const float MIN_FOOD_DISTANCE = 400.0f;
    const int MAX_OBSTACLES = 100;
    const int MIN_FOOD_COUNT = 100;
    std::vector<Food> foods;

    enum class GameState {
        READY = 0,
        PLAYING = 1,
        PAUSED = 2,
        GAME_OVER = 3
    };
    
    GameState gameState;
    int score;
    bool isValidFoodPosition(const glm::vec3& pos) const;
    
    // 相机插值参数
    glm::vec3 targetCameraPos;
    glm::vec3 targetCameraTarget;
    static constexpr float CAMERA_SMOOTH_FACTOR = 0.1f;
    
    // 添加无敌帧相关变量
    int invincibleFrames = 0;  // 当前的无敌帧计数

    // 相机设置优化
    struct CameraSettings {
        float distance;      // 基础跟随距离
        float minHeight;     // 最小高度
        float maxHeight;     // 最大高度
        float baseFOV;       // 基础视野角度
        float maxFOV;        // 最大视野角度
        float smoothFactor;  // 平滑因子
        float rotationSpeed;      // 旋转速度
        float rotationSmoothing;  // 旋转平滑度
    };
    
    // 统一的相机设置，不再分不同模式
    const CameraSettings CAMERA_SETTINGS {
        800.0f,    // 减小基础距离从2000改为800
        400.0f,    // 减小最小高度从1000改为400
        800.0f,    // 减小最大高度从2000改为800
        90.0f,    // 墛大基础FOV角度
        120.0f,   // 墛大最大FOV角度
        0.05f,    // 保持平滑因子
        0.15f,    // 保持旋转速度
        0.08f     // 保持旋转平滑度
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
    glm::vec3 transitionStart;            // 过渡开始位置
    float transitionProgress = 0.0f;       // 过渡进度
    float transitionSpeed = 0.05f;         // 过渡速度

    // 相机位置偏移参数
    static constexpr float SIDE_OFFSET_FACTOR = 0.3f;   // 相机侧向偏移因子
    static constexpr float FORWARD_OFFSET = 600.0f;     // 前方观察点偏移

    // 相机旋转插值参数
    glm::quat currentCameraRotation;    // 当前相机旋转
    glm::quat targetCameraRotation;     // 目标相机旋转 
    float rotationSmoothFactor;         // 旋转平滑因子

    // 保留必要的水体相关变量
    Water* water;  
    float deltaTime;

    // 着色器源码
    static const char* volumetricLightVertexShader;   // 体积顶点着色器
    static const char* volumetricLightFragmentShader; // 体积光片段着色器

    // 资源句柄
    GLuint volumetricLightFBO;        // 体积光FBO
    GLuint volumetricLightTexture;    // 体积光纹理
    GLuint waterNormalTexture;        // 水面法线纹理
    GLuint bubbleTexture;             // 气泡纹理
    GLuint causticTexture;            // 焦散纹理

    // 动画和更新相关
    float causticTime;                // 焦散动画时间
    std::vector<glm::vec3> bubblePositions;  // 气泡位置数组

    // 函数声明 - 删除重复的声明
    void initVolumetricLight();        // 初始化体积光
    void updateVolumetricLight();      // 更新体积光
    void renderVolumetricLight();      // 渲染体积光
    void createWaterNormalTexture();   // 创建水面法线纹理
    void createBubbleTexture();        // 创建气泡纹理
    void initCausticTexture();         // 初始化焦散纹理
    void updateCaustics();             // 更新焦散效果
    void updateBubbles();              // 更新气泡
    
    // 添加新的函数声明
    void updateUnderwaterEffects();
    void renderUnderwaterEffects();
    void applyLightSettings();

    // 添加水下效果相关参数
    struct UnderwaterEffects {
        float dispersionStrength = 0.02f;    
        float visibilityRange = 800.0f;      
        float fogDensity = 0.001f;           
        glm::vec3 fogColor = glm::vec3(0.1f, 0.2f, 0.3f);
        float minAmbientLight = 0.4f;        // 增加最小环境光强度
        float maxAmbientLight = 0.8f;        // 墛大最大环境光强度
        float lightStability = 0.98f;        // 墛大光照稳定性
        float depthDarkening = 0.0002f;      // 降低深度衰减系数
        float currentLight = 0.5f;           // 初始值设为中等亮度
    } underwaterEffects;

    // 添加光源相关结构体和变量
    struct LightSource {
        glm::vec3 position;      // 光源位置
        glm::vec3 direction;     // 光源方向（仅用于方向光）
        glm::vec3 color;        // 光源颜色
        float intensity;        // 光源强度
        float radius;          // 点光源的影响范围（0表示方向光）
        float attenuation;     // 衰减系数
        
        // 添加新的参数
        bool castShadows;      // 是否投射阴影
        float spotCutoff;      // 聚光灯角度（若适用）
        float spotExponent;    // 聚光灯衰减指数（若适用）
    };

    // 光源数组
    std::vector<LightSource> lightSources;
    
    // 光照参数
    struct LightingParams {
        float sunlightIntensity = 2.0f;           // 增强太阳光强度
        float ambientIntensity = 0.4f;            // 提高环境光
        float volumetricIntensity = 0.8f;         // 增强体积光
        float causticLightIntensity = 0.6f;       // 增强焦散
        float waterScattering = 0.3f;             // 增加水体散射
        glm::vec3 waterAbsorption = glm::vec3(0.15f, 0.08f, 0.25f); // 调整水体吸收
    } lightingParams;

    void initLights();
    void updateLights();
    
    // 添加新的私有函数声明
    void applyUnderwaterState();
    void drawSceneObjects();

    // 添加相机模式枚举
    enum class CameraMode {
        FOLLOW,     // 跟随视角(原有视角)
        TOP_DOWN    // 俯视视角
    };
    
    CameraMode currentCameraMode = CameraMode::FOLLOW;  // 当前相机模式
    
    // 俯视相机参数
    static constexpr float TOP_DOWN_HEIGHT = 1500.0f;   // 俯视高度
    static constexpr float TOP_DOWN_SMOOTH_FACTOR = 0.1f;  // 平滑因子
};

#endif // GAMEWIDGET_H