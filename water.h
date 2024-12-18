#define GLM_ENABLE_EXPERIMENTAL  // 添加这行到最前面

#ifndef WATER_H
#define WATER_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <QOpenGLFunctions>

class Water : protected QOpenGLFunctions {
public:
    Water(float size);
    ~Water();
    
    void init();
    void render(const glm::mat4& projection, const glm::mat4& view);
    void update(float deltaTime);
    void updateBubbles(float deltaTime);
    void renderVolumetricLight();

    struct WaterParams {
        float causticIntensity = 0.3f;    
        float waveHeight = 30.0f;          // 墛大波浪高度
        float waveSpeed = 0.8f;           
        float distortionStrength = 0.02f; 
        float surfaceRoughness = 0.7f;    
        glm::vec3 deepColor = glm::vec3(0.0f, 0.1f, 0.2f);
        glm::vec3 shallowColor = glm::vec3(0.0f, 0.4f, 0.6f);
        float volumetricLightIntensity = 0.3f;
        float volumetricLightDecay = 0.95f;
        float bubbleSpeed = 0.5f;
        float bubbleDensity = 0.3f;
        float lightBeamWidth = 15.0f;
        float chromaDispersion = 0.015f;
        float waterDensity = 0.0005f;      // 降低水密度（因为尺寸变大）
        float visibilityFalloff = 0.1f;    // 调整能见度衰减
        float causticScale = 0.8f;        // 焦散缩放
        float causticSpeed = 0.5f;        // 焦散动画速度
        float causticBlend = 0.6f;        // 焦散混合强度
        int causticLayers = 3;            // 焦散叠加层数

        // 添加水下效果参数
        float underwaterScatteringDensity = 0.05f;   // 增加散射密度
        float underwaterVisibility = 5000.0f;  // 墛大水下能见度
        float underwaterCausticIntensity = 1.2f;     // 墛强水下焦散
        float underwaterGodrayIntensity = 0.6f;      // 墛强光束效果
        float underwaterParticleDensity = 200.0f;    // 墛加粒子数量
        glm::vec3 underwaterColor = glm::vec3(0.1f, 0.3f, 0.5f); // 加深水下颜色

        float lightPenetration = 0.8f;    // 墛加光线穿透率
        float scatteringDensity = 0.3f;   // 降低散射密度
        float causticStrength = 1.5f;     // 墛强焦散效
    };

    WaterParams& getParams() { return waterParams; }

    struct Bubble {
        glm::vec3 position;
        float size;         // 气泡大小
        float speed;        // 上升速度
        float wobble;       // 左右摆动幅度
        float phase;        // 摆动相位
        float alpha;        // 透明度
        float rotationSpeed;// 添加旋转速度
        float rotation;     // 当前旋转角度
    };

    // 添加水下颗粒结构体
    struct WaterParticle {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 color;
        float size;
        float alpha;
        float targetAlpha;  // 新增：目标透明度
        float phase;
        float life;
        float fadeState;    // 新增：淡入淡出状态
    };

    // 添加设置相机位置的方法
    void setCameraPosition(const glm::vec3& pos) { cameraPos = pos; }

    // 添加getter用于调试
    GLuint getCausticTexture() const { return causticTexture; }
    GLuint getVolumetricLightTexture() const { return volumetricLightTexture; }
    GLuint getWaterNormalTexture() const { return waterNormalTexture; }
    GLuint getBubbleTexture() const { return bubbleTexture; }

    void initializeGL();  // 添加这个方法

    // 添加新的水下效果管理方法
    void beginUnderwaterEffect(const glm::mat4& proj, const glm::mat4& view);
    void endUnderwaterEffect();
    
    // 添加水下效果参数结构体
    struct UnderwaterState {
        bool isUnderwater;
        float fogDensity;
        glm::vec3 fogColor;
        float ambientIntensity;
        
        UnderwaterState() : 
            isUnderwater(false),
            fogDensity(0.001f),
            fogColor(0.1f, 0.2f, 0.3f),
            ambientIntensity(0.4f)
        {}
    };
    
    UnderwaterState& getUnderwaterState() { return underwaterState; }

    // 将这些方法移到 public 区域
    void updateWaterParticles(float deltaTime, const glm::vec3& snakePosition);
    void renderWaterParticles();

protected:
    int width() const;
    int height() const;

private:
    void initShaders();
    void createWaterSurface();
    void initCausticTexture();
    void initVolumetricLight();

    float size;
    WaterParams waterParams;
    float waterTime;
    GLuint waterProgram;
    GLuint waterVAO;
    GLuint waterVBO;
    GLuint causticTexture;           // 焦散纹理
    GLuint volumetricLightFBO;       // 体积光FBO
    GLuint volumetricLightTexture;   // 体积光纹理
    GLuint waterNormalTexture;       // 水面法线纹理
    GLuint bubbleTexture;            // 气泡纹理
    float causticTime;               // 焦散动画时间
    std::vector<glm::vec3> bubblePositions;  // 气泡位置数组
    std::vector<Bubble> bubbles;
    
    // 调整气泡参数
    static constexpr int MAX_BUBBLES = 500;         // 增加气泡数量
    static constexpr float MIN_BUBBLE_SIZE = 8.0f;  // 增大最小气泡尺寸
    static constexpr float MAX_BUBBLE_SIZE = 16.0f; // 增大最大气泡尺寸
    static constexpr float BUBBLE_BASE_ALPHA = 0.9f;// 增加基础不透明度
    static constexpr float BUBBLE_SPAWN_RATE = 0.05f;  // 增加生成率
    static constexpr float BUBBLE_SPREAD_FACTOR = 1.2f; // 增加扩散因子
    float bubbleSpawnTimer;

    // 添加着色器源码声明
    static const char* waterVertexShader;
    static const char* waterFragmentShader;
    static const char* volumetricLightVertexShader;
    static const char* volumetricLightFragmentShader;

    // 添加私有函数
    void initWaterNormalTexture();
    void createBubbleTexture();
    void updateCaustics();
    void renderBubbles();

    // 添加水体渲染参数
    const float WATER_ALPHA = 0.15f;
    const glm::vec3 WATER_COLOR = glm::vec3(0.1f, 0.4f, 0.6f);

    glm::vec3 cameraPos; // 添加机位置成员变量

    struct CausticLayer {
        float scale;
        float speed;
        float offset;
        glm::vec2 direction;
    };
    std::vector<CausticLayer> causticLayers;
    void generateCausticTexture();
    void updateCausticAnimation(float deltaTime);

    // 添加水下效果相关变量和方法
    struct UnderwaterParticle {
        glm::vec3 position;
        glm::vec3 velocity;
        float size;
        float life;
    };
    
    std::vector<UnderwaterParticle> underwaterParticles;
    GLuint underwaterParticleTexture;
    
    void initUnderwaterEffects();
    void updateUnderwaterParticles(float deltaTime);
    void renderUnderwaterEffects(const glm::mat4& projection, const glm::mat4& view);
    void generateUnderwaterParticle(UnderwaterParticle& particle);

    // 添加调试辅助函数
    bool validateShaderProgram();
    bool checkTextureState();
    void dumpOpenGLState();
    
    // 添加错误跟踪
    void checkGLError(const char* operation);

    GLsizei vertexCount;  // 添加顶点数量成员变量

    // 简化过渡态结构
    struct TransitionState {
        float currentFogDensity = 0.05f;
        glm::vec3 currentColor = glm::vec3(0.1f, 0.3f, 0.5f);
        float smoothFactor = 0.01f;  // 降低过渡速度
    } transitionState;
    
    // 水下视觉效果的过渡参数
    static constexpr float SMOOTH_FACTOR = 0.01f;        // 降低过渡速度
    static constexpr float DEPTH_INFLUENCE = 0.00002f;   // 降低深度影响

    // 添加视角转换相关常量
    static constexpr float MIN_VIEW_DOT = 0.7f;             // cos(45°)约为0.7
    static constexpr float COLOR_TRANSITION_SPEED = 0.1f;   // 颜色转换速度

    UnderwaterState underwaterState;
    
    // 保存原始OpenGL状态的变量
    struct GLState {
        GLboolean fog;
        GLboolean lighting;
        GLboolean depthTest;
        GLfloat fogParams[4];
        GLfloat lightModelAmbient[4];
    } originalState;
    
    void saveGLState();
    void restoreGLState();

    // 添加体积光关参数
    struct VolumetricLightParams {
        float density = 0.8f;         // 增加密度
        float scattering = 0.7f;      // 散射系数
        float exposure = 1.5f;        // 增强曝光
        float decay = 0.98f;          // 提高衰减系数，使光线更久
        int numSamples = 100;         // 采样数量
        glm::vec3 lightColor = glm::vec3(1.0f, 0.98f, 0.95f); // 温暖的光线颜色
    };
    VolumetricLightParams volumetricParams;
    
    // 体积光渲染所需的着色器
    GLuint volumetricProgram;
    GLuint volumetricVAO;
    GLuint volumetricVBO;
    
    void initVolumetricLightShader();
    void createVolumetricScreenQuad();

    // 添加矩阵成员变量
    glm::mat4 projectionMatrix;  // 投影矩阵
    glm::mat4 viewMatrix;        // 视图矩阵

    // 添加私有函数
    void initParticleSystem();
    void spawnBubble();
    void updateBubble(Bubble& bubble, float deltaTime);

    // 水下颗粒系统参数
    static constexpr int MAX_WATER_PARTICLES = 1000;      // 保持较多的粒子数量
    static constexpr float PARTICLE_MIN_SIZE = 1.5f;      
    static constexpr float PARTICLE_MAX_SIZE = 4.0f;      
    static constexpr float PARTICLE_MIN_ALPHA = 0.0f;     // 最小透明度为0，用于淡入
    static constexpr float PARTICLE_MAX_ALPHA = 0.2f;     
    static constexpr float PARTICLE_FADE_TIME = 1.5f;     // 增加淡入淡出时间
    static constexpr float PARTICLE_SPAWN_RADIUS = 450.0f; // 生成范围
    static constexpr float PARTICLE_SPAWN_HEIGHT = 200.0f;  // 高度范围
    static constexpr float PARTICLE_LIFE_MIN = 2.0f;      // 最小生命周期
    static constexpr float PARTICLE_LIFE_MAX = 4.0f;      // 最大生命周期
    
    std::vector<WaterParticle> waterParticles;
    GLuint waterParticleTexture;
    
    void initWaterParticles();
    void generateWaterParticle(WaterParticle& particle, const glm::vec3& targetPos = glm::vec3(0.0f));
};

#endif // WATER_H