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

    struct WaterParams {
        float causticIntensity = 0.3f;    
        float waveHeight = 15.0f;         
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
        float waterDensity = 0.05f;
        float visibilityFalloff = 0.5f;
    };

    WaterParams& getParams() { return waterParams; }

    struct Bubble {
        glm::vec3 position;
        float size;         // 气泡大小
        float speed;        // 上升速度
        float wobble;       // 左右摆动幅度
        float phase;        // 摆动相位
        float alpha;        // 透明度
    };

    // 添加设置相机位置的方法
    void setCameraPosition(const glm::vec3& pos) { cameraPos = pos; }

protected:
    int width() const;
    int height() const;

private:
    void initShaders();
    void createWaterSurface();
    void initCausticTexture();
    void initVolumetricLight();
    void updateBubbles();

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
    
    // 调整气泡参数以使其更明显
    static constexpr int MAX_BUBBLES = 1000;        // 增加气泡数量
    static constexpr float MIN_BUBBLE_SIZE = 4.0f;  // 调整气泡大小
    static constexpr float MAX_BUBBLE_SIZE = 12.0f;
    static constexpr float BUBBLE_SPAWN_RATE = 0.01f;
    static constexpr float BUBBLE_BASE_ALPHA = 0.6f;
    static constexpr float BUBBLE_SPREAD_FACTOR = 0.95f;
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
    void renderVolumetricLight();
    void spawnBubble();
    void updateBubble(Bubble& bubble, float deltaTime);

    // 添加水体渲染参数
    const float WATER_ALPHA = 0.15f;
    const glm::vec3 WATER_COLOR = glm::vec3(0.1f, 0.4f, 0.6f);

    glm::vec3 cameraPos; // 添加相机位置成员变量
};

#endif // WATER_H