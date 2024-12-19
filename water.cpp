#define GLM_ENABLE_EXPERIMENTAL
#include "water.h"
#include <QDebug>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>  // 为 std::clamp 添加头文件

// 在文件开头添加 smoothstep 函数的实现
float smoothstep(float edge0, float edge1, float x) {
    // 将 x 限制在 [0,1] 范围内
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    // 使用 3x^2 - 2x^3 插值
    return x * x * (3.0f - 2.0f * x);
}

// 简化水体顶点着色器，减少波浪效果
const char* Water::waterVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;
    
    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;
    uniform float time;
    
    out vec2 TexCoord;
    out vec3 FragPos;
    out vec3 Normal;
    out vec4 ClipSpace;
    
    void main()
    {
        vec3 pos = aPos;
        FragPos = vec3(model * vec4(pos, 1.0));
        Normal = vec3(0.0, 1.0, 0.0);  // 简化法线计算
        ClipSpace = projection * view * model * vec4(pos, 1.0);
        gl_Position = ClipSpace;
        TexCoord = aTexCoord;
    }
)";

// 修改水体片段着色器，确保所有uniform变量都被使用
const char* Water::waterFragmentShader = R"(
    #version 330 core
    out vec4 FragColor;

    in vec2 TexCoord;
    in vec3 FragPos;
    in vec3 Normal;
    in vec4 ClipSpace;

    uniform vec3 deepColor;
    uniform vec3 shallowColor;
    uniform float waterDensity;
    uniform float visibilityFalloff;
    uniform vec3 cameraPos;
    uniform float time;
    uniform sampler2D volumetricLightMap;
    uniform sampler2D causticTexture;
    uniform float volumetricIntensity;
    uniform float waterHeight;  // 添加水面高度uniform

    void main()
    {
        float viewDistance = length(FragPos - cameraPos);
        float depthValue = gl_FragCoord.z / gl_FragCoord.w;
        
        // 计算水下效果
        bool isUnderwater = cameraPos.y < waterHeight;  // 使用水面高度判断
        float waterDepth = abs(waterHeight - cameraPos.y);  // 计算与水面的深度差
        
        // 计算菲涅尔效果
        vec3 viewDir = normalize(cameraPos - FragPos);
        float fresnel = pow(1.0 - max(dot(viewDir, Normal), 0.0), 4.0);
        
        // 增强体积光效果
        vec2 screenCoord = gl_FragCoord.xy / vec2(1024, 768);
        vec3 volumetricLight = texture(volumetricLightMap, screenCoord).rgb;
        volumetricLight *= 2.0; // 增强体积光强度
        
        // 添加焦散效果
        vec2 causticCoord = FragPos.xz * 0.05 + time * 0.03; // 缓慢移动的焦散
        float causticIntensity = texture(causticTexture, causticCoord).r;
        causticIntensity += texture(causticTexture, causticCoord * 1.4 - time * 0.02).r * 0.5;
        
        // 根据观察方向调整颜色
        vec3 waterColor;
        float alpha;
        
        if(isUnderwater) {
            // 水下观察
            float depthFactor = exp(-waterDepth * waterDensity);
            waterColor = mix(deepColor, shallowColor, depthFactor);
            
            // 添加焦散效果
            waterColor += vec3(causticIntensity) * 0.2 * depthFactor;
            
            // 水下时略微降低透明度
            alpha = 0.6;
            
            // 水下体积光增强
            volumetricLight *= 3.0;
            volumetricLight *= exp(-waterDepth * 0.1);
            
            // 随深度加深颜色
            float deepFactor = clamp(waterDepth / 1000.0, 0.0, 1.0);
            waterColor = mix(waterColor, deepColor * 0.5, deepFactor);
        } else {
            // 水上观察
            waterColor = mix(shallowColor, deepColor, fresnel);
            alpha = mix(0.4, 0.8, fresnel);
            
            // 添加水面焦散
            waterColor += vec3(causticIntensity) * 0.1;
        }
        
        // 最终颜色混合
        vec3 finalColor = mix(waterColor, waterColor + volumetricLight, volumetricIntensity);
        
        // 输出最终颜色
        FragColor = vec4(finalColor, alpha);
    }
)";

// 添加体积光顶点着色器
const char* Water::volumetricLightVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;
    
    out vec2 TexCoord;
    
    void main() {
        gl_Position = vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)";

// 添加体积光片段着色器
const char* Water::volumetricLightFragmentShader = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;
    
    uniform sampler2D depthMap;
    uniform vec3 lightPos;
    uniform vec3 lightColor;
    uniform float density;
    uniform float scattering;
    uniform float exposure;
    uniform float decay;
    uniform int numSamples;
    uniform vec3 cameraPos;
    uniform float waterLevel;
    uniform vec2 lightPositionOnScreen;
    uniform float weight;
    
    void main() {
        // 从屏幕空间到世界空间的变换
        vec2 texCoord = TexCoord;
        vec2 deltaTextCoord = (texCoord - lightPos.xy);
        deltaTextCoord *= 1.0 / float(numSamples) * density;
        
        // 当前位置
        vec2 currentTextCoord = texCoord;
        
        // 初始颜色
        vec3 color = vec3(0.0);
        float illuminationDecay = 1.0;
        
        // 水下光束强度增强
        float underwaterBoost = cameraPos.y < waterLevel ? 2.0 : 1.0;
        
        // 沿光线方向采样
        for(int i = 0; i < numSamples; i++) {
            currentTextCoord -= deltaTextCoord;
            
            // 确保采样坐标在有效范围内
            if(currentTextCoord.x < 0.0 || currentTextCoord.x > 1.0 ||
               currentTextCoord.y < 0.0 || currentTextCoord.y > 1.0)
                continue;
            
            // 获取深度值和样本颜色
            float depth = texture(depthMap, currentTextCoord).r;
            vec3 sample = vec3(depth);
            
            // 应用散射和衰减
            sample *= illuminationDecay * scattering;
            
            // 水下效果增强
            sample *= underwaterBoost;
            
            // 添加基于深度的颜色变化
            float depthFactor = 1.0 - depth;
            vec3 waterColor = mix(
                vec3(0.2, 0.4, 0.8),  // 深水颜色
                vec3(0.4, 0.6, 0.9),  // 浅水颜色
                depthFactor
            );
            
            // 将水体颜色与光束混合
            sample *= waterColor;
            
            // 累积颜色
            color += sample;
            
            // 更新衰减 - 水下衰减更慢
            illuminationDecay *= mix(0.99, decay, float(i) / float(numSamples));
        }
        
        // 应用曝光和光线颜色
        color *= exposure * lightColor;
        
        // 确保颜色值在有效范围内
        color = clamp(color, 0.0, 1.0);
        
        // 根据深度调整alpha值
        float alpha = min(1.0, length(color) * 0.8);
        
        // 输出最终颜色
        FragColor = vec4(color, alpha);
    }
)";

// 修改构造函数，初始化纹理ID
Water::Water(float size) : 
    size(size), 
    waterHeight(size * 0.45f),  // 初始化水面高度
    waterTime(0.0f),
    causticTime(0.0f),
    bubbleSpawnTimer(0.0f),
    cameraPos(0.0f),
    waterProgram(0),
    waterVAO(0),
    waterVBO(0),
    causticTexture(0),
    volumetricLightFBO(0),
    volumetricLightTexture(0),
    waterNormalTexture(0),
    bubbleTexture(0),
    underwaterParticleTexture(0),
    waterParticleTexture(0),
    volumetricProgram(0),
    volumetricVAO(0),
    volumetricVBO(0),
    vertexCount(0),
    projectionMatrix(1.0f),
    viewMatrix(1.0f),
    originalState{false, false, false, {0.0f}, {0.0f}}
{
    // 调�����水体参数以优化游戏体验
    waterParams.deepColor = glm::vec3(0.1f, 0.2f, 0.4f);        // 更深的蓝色
    waterParams.shallowColor = glm::vec3(0.3f, 0.5f, 0.7f);     // 更亮的蓝色
    waterParams.waterDensity = 0.0002f;                         // 进一步降低水密度，提高能见度
    waterParams.visibilityFalloff = 0.01f;                      // 降低能见度衰减
    waterParams.underwaterScatteringDensity = 0.001f;           // 降低散射密度
    waterParams.causticIntensity = 0.4f;                        // 增强焦散效果
    waterParams.bubbleSpeed = 20.0f;                            // 降低气泡速度
    waterParams.causticBlend = 0.8f;                            // 增强焦散混合
    waterParams.underwaterGodrayIntensity = 0.6f;               // 增强光束效果
    waterParams.underwaterVisibility = 5000.0f;                 // 增加水下能见度
    waterParams.underwaterParticleDensity = 500.0f;            // 增加粒子数量
    waterParams.causticSpeed = 0.2f;                           // 降低焦散动画速度
    
    // 初始化焦散层
    causticLayers.resize(3);
    causticLayers[0] = {2.0f, 0.3f, 0.0f, glm::vec2(1.0f, 0.0f)};
    causticLayers[1] = {4.0f, 0.2f, 0.0f, glm::vec2(0.0f, 1.0f)};
    causticLayers[2] = {8.0f, 0.1f, 0.0f, glm::vec2(0.7f, 0.7f)};
    
    // 初始化体积光参数
    volumetricParams.density = 0.8f;
    volumetricParams.scattering = 0.7f;
    volumetricParams.exposure = 1.5f;
    volumetricParams.decay = 0.98f;
    volumetricParams.numSamples = 100;
    volumetricParams.lightColor = glm::vec3(1.0f, 0.98f, 0.95f);
}

Water::~Water() {
    // 删除纹理和FBO
    if(causticTexture) glDeleteTextures(1, &causticTexture);
    if(volumetricLightTexture) glDeleteTextures(1, &volumetricLightTexture);
    if(waterNormalTexture) glDeleteTextures(1, &waterNormalTexture);
    if(bubbleTexture) glDeleteTextures(1, &bubbleTexture);
    if(volumetricLightFBO) glDeleteFramebuffers(1, &volumetricLightFBO);
    
    // 删除着色器程
    if(waterProgram) glDeleteProgram(waterProgram);
    if(volumetricProgram) glDeleteProgram(volumetricProgram);
    
    // 删除VAO和VBO
    if(waterVAO) glDeleteVertexArrays(1, &waterVAO);
    if(waterVBO) glDeleteBuffers(1, &waterVBO);
    if(volumetricVAO) glDeleteVertexArrays(1, &volumetricVAO);
    if(volumetricVBO) glDeleteBuffers(1, &volumetricVBO);
}

void Water::initializeGL()
{
    initializeOpenGLFunctions();
}

// 修改init函数，确保气泡正确初始化
void Water::init() {
    qDebug() << "\n=== Initializing Water System ===";
    qDebug() << "Water size:" << size;
    qDebug() << "MAX_BUBBLES:" << MAX_BUBBLES;
    
    // 初始化OpenGL函数
    initializeOpenGLFunctions();
    
    // 检查必要的OpenGL扩展
    qDebug() << "Checking OpenGL extensions...";
    if (!glewIsSupported("GL_ARB_point_sprite")) {
        qDebug() << "Warning: GL_ARB_point_sprite not supported!";
    }
    if (!glewIsSupported("GL_ARB_point_parameters")) {
        qDebug() << "Warning: GL_ARB_point_parameters not supported!";
    }
    
    // 获取OpenGL版本信息
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    qDebug() << "OpenGL Version:" << version;
    qDebug() << "OpenGL Vendor:" << vendor;
    qDebug() << "OpenGL Renderer:" << renderer;
    
    // 初始化着色器
    qDebug() << "\nInitializing shaders...";
    initShaders();
    if (!validateShaderProgram()) {
        qDebug() << "Shader initialization failed!";
        return;
    }
    qDebug() << "Shader initialization successful";
    
    // 创建几何体
    qDebug() << "\nCreating water surface...";
    createWaterSurface();
    qDebug() << "Water surface created with" << vertexCount << "vertices";
    
    // 初始化纹理
    qDebug() << "\nInitializing textures...";
    
    // 删除所有现有纹理
    if(causticTexture != 0) glDeleteTextures(1, &causticTexture);
    if(waterNormalTexture != 0) glDeleteTextures(1, &waterNormalTexture);
    if(bubbleTexture != 0) glDeleteTextures(1, &bubbleTexture);
    if(volumetricLightTexture != 0) glDeleteTextures(1, &volumetricLightTexture);
    if(waterParticleTexture != 0) glDeleteTextures(1, &waterParticleTexture);
    
    causticTexture = 0;
    waterNormalTexture = 0;
    bubbleTexture = 0;
    volumetricLightTexture = 0;
    waterParticleTexture = 0;
    
    // 创建新纹理
    glGenTextures(1, &causticTexture);
    glGenTextures(1, &waterNormalTexture);
    glGenTextures(1, &bubbleTexture);
    glGenTextures(1, &volumetricLightTexture);
    glGenTextures(1, &waterParticleTexture);
    
    // 检查纹理创建是否成功
    qDebug() << "Texture IDs:";
    qDebug() << "- Caustic texture:" << causticTexture;
    qDebug() << "- Water normal texture:" << waterNormalTexture;
    qDebug() << "- Bubble texture:" << bubbleTexture;
    qDebug() << "- Volumetric light texture:" << volumetricLightTexture;
    qDebug() << "- Water particle texture:" << waterParticleTexture;
    
    // 初始化各个组件
    qDebug() << "\nInitializing components...";
    initCausticTexture();
    initWaterNormalTexture();
    createBubbleTexture();
    initVolumetricLight();
    initWaterParticles();
    
    // 验证纹理创建
    bool texturesValid = true;
    if(!glIsTexture(causticTexture)) {
        qDebug() << "Error: Caustic texture not valid!";
        texturesValid = false;
    }
    if(!glIsTexture(waterNormalTexture)) {
        qDebug() << "Error: Water normal texture not valid!";
        texturesValid = false;
    }
    if(!glIsTexture(bubbleTexture)) {
        qDebug() << "Error: Bubble texture not valid!";
        texturesValid = false;
    }
    if(!glIsTexture(volumetricLightTexture)) {
        qDebug() << "Error: Volumetric light texture not valid!";
        texturesValid = false;
    }
    if(!glIsTexture(waterParticleTexture)) {
        qDebug() << "Error: Water particle texture not valid!";
        texturesValid = false;
    }
    
    if(!texturesValid) {
        qDebug() << "One or more textures failed to initialize!";
        return;
    }
    
    qDebug() << "All textures initialized successfully";
    
    // 初始化粒子系统
    qDebug() << "\nInitializing particle system...";
    initParticleSystem();
    
    // 初始化水效果
    qDebug() << "\nInitializing underwater effects...";
    initUnderwaterEffects();
    
    // 确保水下粒子系统正确初始化
    qDebug() << "\nInitializing underwater particles...";
    underwaterParticles.resize(static_cast<size_t>(waterParams.underwaterParticleDensity));
    for(auto& particle : underwaterParticles) {
        generateUnderwaterParticle(particle);
    }
    qDebug() << "Underwater particles initialized:" << underwaterParticles.size();
    
    // 生成初始气泡
    qDebug() << "\nGenerating initial bubbles...";
    bubbles.clear();  // 确保从空列表开始
    for(int i = 0; i < MAX_BUBBLES; ++i) {
        spawnBubble();
        if(i % 100 == 0) {
            qDebug() << "Generated" << i + 1 << "bubbles...";
        }
    }
    
    // 验证初始化状态
    qDebug() << "\nInitialization complete:";
    qDebug() << "- Bubbles:" << bubbles.size() << "/" << MAX_BUBBLES;
    qDebug() << "- Water particles:" << waterParticles.size();
    qDebug() << "- Underwater particles:" << underwaterParticles.size();
    
    if(bubbles.empty()) {
        qDebug() << "Warning: No bubbles were generated!";
    } else {
        const auto& firstBubble = bubbles[0];
        qDebug() << "First bubble state:";
        qDebug() << "- Position:" << firstBubble.position.x << firstBubble.position.y << firstBubble.position.z;
        qDebug() << "- Size:" << firstBubble.size;
        qDebug() << "- Speed:" << firstBubble.speed;
    }
    
    // 检���OpenGL错误
    GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        qDebug() << "OpenGL error after initialization:" << error;
    } else {
        qDebug() << "No OpenGL errors during initialization";
    }
}

void Water::initParticleSystem() {
    qDebug() << "\n=== Initializing Particle System ===";
    
    // ���空并重新生成气泡
    bubbles.clear();
    bubbleSpawnTimer = 0.0f;
    
    qDebug() << "Generating initial bubbles...";
    qDebug() << "MAX_BUBBLES:" << MAX_BUBBLES;
    
    // 初始生成一定数量的气泡
    for(int i = 0; i < MAX_BUBBLES; ++i) {
        spawnBubble();
        if(i == 0 || i == MAX_BUBBLES-1) {
            qDebug() << "Generated bubble" << i + 1 << "of" << MAX_BUBBLES;
        }
    }
    
    qDebug() << "Initialization complete. Total bubbles:" << bubbles.size();
    
    // 验证气泡是否正确生成
    if(!bubbles.empty()) {
        const auto& firstBubble = bubbles[0];
        qDebug() << "First bubble verification:";
        qDebug() << "- Position:" << firstBubble.position.x << firstBubble.position.y << firstBubble.position.z;
        qDebug() << "- Size:" << firstBubble.size;
        qDebug() << "- Speed:" << firstBubble.speed;
        qDebug() << "- Alpha:" << firstBubble.alpha;
    } else {
        qDebug() << "Error: No bubbles were generated!";
    }
    
    // 检查气泡纹理
    if(bubbleTexture == 0) {
        qDebug() << "Creating bubble texture...";
        createBubbleTexture();
    }
    
    if(!glIsTexture(bubbleTexture)) {
        qDebug() << "Error: Bubble texture not created properly!";
    } else {
        qDebug() << "Bubble texture created successfully.";
    }
}

void Water::initUnderwaterEffects() {
    // 创建水下粒子纹理
    const int texSize = 32;
    std::vector<unsigned char> texData(texSize * texSize * 4);
    
    for(int y = 0; y < texSize; ++y) {
        for(int x = 0; x < texSize; ++x) {
            float dx = (x - texSize/2.0f) / (texSize/2.0f);
            float dy = (y - texSize/2.0f) / (texSize/2.0f);
            float dist = std::sqrt(dx*dx + dy*dy);
            
            float alpha = std::max(0.0f, 1.0f - dist);
            alpha = std::pow(alpha, 2.0f);  // 使边缘更柔和
            
            int idx = (y * texSize + x) * 4;
            texData[idx + 0] = 255;  // R
            texData[idx + 1] = 255;  // G
            texData[idx + 2] = 255;  // B
            texData[idx + 3] = static_cast<unsigned char>(alpha * 255);
        }
    }
    
    glGenTextures(1, &underwaterParticleTexture);
    glBindTexture(GL_TEXTURE_2D, underwaterParticleTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // 初始化水下粒子
    underwaterParticles.resize(static_cast<size_t>(waterParams.underwaterParticleDensity));
    for(auto& particle : underwaterParticles) {
        generateUnderwaterParticle(particle);
    }
}

void Water::initShaders() {
    // 创建并编译着��器
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    
    glShaderSource(vertexShader, 1, &waterVertexShader, NULL);
    glShaderSource(fragmentShader, 1, &waterFragmentShader, NULL);
    
    // 编译着色器
    glCompileShader(vertexShader);
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        qDebug() << "Vertex shader compilation failed:\n" << infoLog;
    }
    
    // 编译片段着色器
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        qDebug() << "Fragment shader compilation failed:\n" << infoLog;
        return;  // 如果着色器编译失败，立即返回
    }
    
    // 创建并链接程序
    waterProgram = glCreateProgram();
    glAttachShader(waterProgram, vertexShader);
    glAttachShader(waterProgram, fragmentShader);
    glLinkProgram(waterProgram);
    
    // 检查链接状态
    glGetProgramiv(waterProgram, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(waterProgram, 512, NULL, infoLog);
        qDebug() << "Shader program linking failed:\n" << infoLog;
        return;  // 如果链接失败，立即返回
    }
    
    // 清理
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 验证所必需的uniform变量
    GLint uniformLocation;
    const char* requiredUniforms[] = {
        "projection", "view", "model", "time",
        "deepColor", "shallowColor", "waterDensity", "visibilityFalloff", "cameraPos"
    };

    for(const char* uniform : requiredUniforms) {
        uniformLocation = glGetUniformLocation(waterProgram, uniform);
        if(uniformLocation == -1) {
            qDebug() << "Warning: Uniform" << uniform << "not found in shader program";
        }
    }
}

void Water::createWaterSurface() {
    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);

    // 使用成员变量waterHeight
    float surfaceSize = size * 1.2f;   // 保持水面范围略大于水族箱

    float vertices[] = {
        // 顶面
        -surfaceSize,  waterHeight, -surfaceSize,  0.0f, 0.0f,  // 左后
         surfaceSize,  waterHeight, -surfaceSize,  1.0f, 0.0f,  // 右后
         surfaceSize,  waterHeight,  surfaceSize,  1.0f, 1.0f,  // 右前
        -surfaceSize,  waterHeight, -surfaceSize,  0.0f, 0.0f,  // 左后
         surfaceSize,  waterHeight,  surfaceSize,  1.0f, 1.0f,  // 右前
        -surfaceSize,  waterHeight,  surfaceSize,  0.0f, 1.0f,  // 左前
    };

    vertexCount = 6;

    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 位置属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // 纹理坐标属性
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Water::initCausticTexture() {
    // 确保纹理已创建
    if (causticTexture == 0) {
        glGenTextures(1, &causticTexture);
    }
    
    // 设置纹理参数
    glBindTexture(GL_TEXTURE_2D, causticTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // 生成初始纹理数据
    const int texSize = 512;
    std::vector<float> texData(texSize * texSize, 0.0f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texSize, texSize, 0, GL_RED, GL_FLOAT, texData.data());
    
    // 验证纹理创建
    if (!glIsTexture(causticTexture)) {
        qDebug() << "Failed to create caustic texture!";
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Water::generateCausticTexture() {
    const int texSize = 512; // 增加纹理分率
    std::vector<float> texData(texSize * texSize);
    
    // 生成基于Voronoi图案的焦散纹理
    for(int y = 0; y < texSize; ++y) {
        for(int x = 0; x < texSize; ++x) {
            float fx = (float)x / texSize;
            float fy = (float)y / texSize;
            float value = 0.0f;
            
            // 生成多层Voronoi
            for(const auto& layer : causticLayers) {
                glm::vec2 pos(fx * layer.scale, fy * layer.scale);
                
                // 计算最近的特征点距离
                float minDist = 1.0f;
                for(int i = 0; i < 4; ++i) {
                    glm::vec2 cellPos(
                        floor(pos.x) + float(rand()) / RAND_MAX,
                        floor(pos.y) + float(rand()) / RAND_MAX
                    );
                    float dist = glm::length(pos - cellPos);
                    minDist = glm::min(minDist, dist);
                }
                
                // 添加基于距离的光强变化
                value += glm::smoothstep(0.2f, 0.0f, minDist) * 
                        waterParams.causticBlend / layer.scale;
            }
            
            texData[y * texSize + x] = glm::clamp(value, 0.0f, 1.0f);
        }
    }
    
    // 更新纹理
    glBindTexture(GL_TEXTURE_2D, causticTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texSize, texSize, 0, GL_RED, GL_FLOAT, texData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);

    // 添加纹理状态检查
    GLint width, height;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    
    if(width == 0 || height == 0) {
        qDebug() << "Warning: Invalid caustic texture dimensions";
    }
}

void Water::initVolumetricLight() {
    // 创建FBO和纹理
    glGenFramebuffers(1, &volumetricLightFBO);
    glGenTextures(1, &volumetricLightTexture);
    
    glBindTexture(GL_TEXTURE_2D, volumetricLightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width(), height(), 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindFramebuffer(GL_FRAMEBUFFER, volumetricLightFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, volumetricLightTexture, 0);
    
    // 查FBO是否完整
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        qDebug() << "Volumetric light FBO is not complete!";
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    initVolumetricLightShader();
    createVolumetricScreenQuad();
}

void Water::initVolumetricLightShader() {
    // 创建着色器程序
    volumetricProgram = glCreateProgram();
    
    // 编译顶点着色器
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &volumetricLightVertexShader, NULL);
    glCompileShader(vertexShader);
    
    // 编译片着色器
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &volumetricLightFragmentShader, NULL);
    glCompileShader(fragmentShader);
    
    // 链接着色器程序
    glAttachShader(volumetricProgram, vertexShader);
    glAttachShader(volumetricProgram, fragmentShader);
    glLinkProgram(volumetricProgram);
    
    // 清理
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Water::createVolumetricScreenQuad() {
    // 创建屏四边形
    float quadVertices[] = {
        // 位置          // 纹理坐标
        -1.0f,  1.0f,  0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f, 0.0f,
         1.0f,  1.0f,  0.0f, 1.0f, 1.0f,
         1.0f, -1.0f,  0.0f, 1.0f, 0.0f,
    };
    
    glGenVertexArrays(1, &volumetricVAO);
    glGenBuffers(1, &volumetricVBO);
    glBindVertexArray(volumetricVAO);
    glBindBuffer(GL_ARRAY_BUFFER, volumetricVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
}

// 修改render函数，确保设置所有uniform量
void Water::render(const glm::mat4& projection, const glm::mat4& view) {
    // 从view矩阵中提取相机位置
    glm::mat4 viewInverse = glm::inverse(view);
    cameraPos = glm::vec3(viewInverse[3]); // 获取view矩阵的平移部分
    
    // 保存状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // 保存矩阵
    projectionMatrix = projection;
    viewMatrix = view;
    
    // 设置基本状态
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 禁用面剔除以允许双面渲染
    glDisable(GL_CULL_FACE);
    
    // 启用深度测试但禁用深度写入
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    
    // 使用水体着色器
    glUseProgram(waterProgram);
    
    // 设置着色器参数
    glUniformMatrix4fv(glGetUniformLocation(waterProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(waterProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(waterProgram, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    
    // 设置水体颜色和参数
    glUniform3fv(glGetUniformLocation(waterProgram, "deepColor"), 1, glm::value_ptr(waterParams.deepColor));
    glUniform3fv(glGetUniformLocation(waterProgram, "shallowColor"), 1, glm::value_ptr(waterParams.shallowColor));
    glUniform1f(glGetUniformLocation(waterProgram, "time"), waterTime);
    glUniform1f(glGetUniformLocation(waterProgram, "waterDensity"), waterParams.waterDensity);
    glUniform1f(glGetUniformLocation(waterProgram, "visibilityFalloff"), waterParams.visibilityFalloff);
    glUniform3fv(glGetUniformLocation(waterProgram, "cameraPos"), 1, glm::value_ptr(cameraPos));
    
    // 设置水面高度
    float waterHeight = size * 0.45f;
    glUniform1f(glGetUniformLocation(waterProgram, "waterHeight"), waterHeight);
    
    // 渲染水体
    glBindVertexArray(waterVAO);
    
    // 首先渲染背面
    glCullFace(GL_FRONT);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    
    // 然后渲染正面
    glCullFace(GL_BACK);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    
    glBindVertexArray(0);
    glUseProgram(0);

    // 恢复深度写入
    glDepthMask(GL_TRUE);

    // 检查是否有气泡和粒子
    qDebug() << "\n=== Rendering State ===";
    qDebug() << "Camera position:" << cameraPos.x << cameraPos.y << cameraPos.z;
    qDebug() << "Water height:" << waterHeight;  // 添加水面高度调试输出
    qDebug() << "Bubble count:" << bubbles.size();
    qDebug() << "Particle count:" << waterParticles.size();
    qDebug() << "Is underwater:" << (cameraPos.y < waterHeight);  // 修改水下����态判断
    
    // ��染气泡和粒子
    if(!bubbles.empty()) {
        qDebug() << "Attempting to render" << bubbles.size() << "bubbles...";
        renderBubbles();
    } else {
        qDebug() << "No bubbles to render!";
    }
    
    // 恢复状态
    glPopAttrib();
}

void Water::renderUnderwaterEffects(const glm::mat4& projection, const glm::mat4& view) {
    static glm::vec3 currentUnderwaterColor = waterParams.underwaterColor;
    static float currentScatteringDensity = waterParams.underwaterScatteringDensity;
    
    float waterHeight = size * 0.45f;  // 水面高度
    
    if(cameraPos.y < waterHeight) {  // 使用水面高度判断
        float depth = waterHeight - cameraPos.y;  // 计算与水面的深度差
        
        // 使用更温和的深度因子
        float depthFactor = std::min(0.3f, depth * 0.001f);
        
        // 计算目标颜色
        glm::vec3 targetColor = glm::mix(
            waterParams.underwaterColor,
            waterParams.deepColor,
            depthFactor
        );
        
        // 更快的颜色过渡
        currentUnderwaterColor = glm::mix(
            currentUnderwaterColor,
            targetColor,
            0.05f  // 增加过渡速度
        );
        
        // 使用更温和的散射密度变化
        float targetDensity = waterParams.underwaterScatteringDensity * 
            (1.0f + depth * 0.0001f);  // 进一步降低深度影响
            
        currentScatteringDensity = glm::mix(
            currentScatteringDensity,
            targetDensity,
            0.05f  // 增加过渡速度
        );
    }
    
    // 确保在每帧更新效果参数
    glUseProgram(waterProgram);
    
    // 更新相��位置
    glUniform3fv(glGetUniformLocation(waterProgram, "cameraPosition"), 1,
                 glm::value_ptr(cameraPos));
                 
    // 更新水下效果参数
    if(cameraPos.y < waterHeight) {  // 使用水面高度判断
        float depth = waterHeight - cameraPos.y;  // 计算与水面的深度差
        float adjustedDensity = waterParams.underwaterScatteringDensity * 
                               (1.0f + depth * 0.0001f);  // 降低深度影响
        
        // 设置着色器参数
        glUniform1f(glGetUniformLocation(waterProgram, "underwaterScatteringDensity"), 
                   adjustedDensity);
        glUniform3fv(glGetUniformLocation(waterProgram, "underwaterColor"),
                    1, glm::value_ptr(currentUnderwaterColor));
        glUniform1f(glGetUniformLocation(waterProgram, "waterDepth"),
                   depth);
    }
    
    glUseProgram(0);
}

void Water::updateUnderwaterParticles(float deltaTime) {
    // 添加粒子数量检查
    if(underwaterParticles.size() != waterParams.underwaterParticleDensity) {
        qDebug() << "Particle count mismatch. Expected:" 
                 << waterParams.underwaterParticleDensity 
                 << "Actual:" << underwaterParticles.size();
    }

    for(auto& particle : underwaterParticles) {
        particle.position += particle.velocity * deltaTime;
        particle.life -= deltaTime * 0.2f;
        
        if(particle.life <= 0) {
            generateUnderwaterParticle(particle);
        }
    }
}

void Water::generateUnderwaterParticle(UnderwaterParticle& particle) {
    float range = size * 0.8f;
    particle.position = glm::vec3(
        (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range,
        -range + float(rand()) / RAND_MAX * range * 2.0f,
        (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range
    );
    
    // 给予慢的随机运动
    particle.velocity = glm::vec3(
        (float(rand()) / RAND_MAX - 0.5f) * 2.0f,
        (float(rand()) / RAND_MAX - 0.3f) * 1.0f,
        (float(rand()) / RAND_MAX - 0.5f) * 2.0f
    ) * 10.0f;
    
    // 使用PARTICLE_MIN_SIZE和PARTICLE_MAX_SIZE来设置粒子大小
    particle.size = PARTICLE_MIN_SIZE + float(rand()) / RAND_MAX * (PARTICLE_MAX_SIZE - PARTICLE_MIN_SIZE);
    particle.life = PARTICLE_LIFE_MIN + float(rand()) / RAND_MAX * (PARTICLE_LIFE_MAX - PARTICLE_LIFE_MIN);
}

int Water::width() const {
    return 1024; // 默认值，可以据需要调整
}

int Water::height() const {
    return 768;  // 默认值根据需要调整
}

void Water::initWaterNormalTexture() {
    const int texSize = 256;
    std::vector<unsigned char> texData(texSize * texSize * 3);  // RGB格式
    
    // 生成Perlin噪声基的水面法线理
    for(int y = 0; y < texSize; ++y) {
        for(int x = 0; x < texSize; ++x) {
            // 创建多层噪声
            float fx = float(x) / texSize;
            float fy = float(y) / texSize;
            
            // 计算多层波浪的高度场
            float height = 0.0f;
            float frequency = 1.0f;
            float amplitude = 1.0f;
            const int OCTAVES = 4;
            
            for(int i = 0; i < OCTAVES; ++i) {
                float nx = fx * frequency;
                float ny = fy * frequency;
                
                // 使用多个正弦波叠加模拟Perlin噪声
                float wave = sin(nx * 6.28318f + ny * 4.0f) * 
                           cos(ny * 6.28318f - nx * 2.0f);
                
                height += wave * amplitude;
                
                frequency *= 2.0f;
                amplitude *= 0.5f;
            }
            
            // 计算法线
            float s01 = (x < texSize-1) ? 
                texData[((y) * texSize + (x+1)) * 3] / 255.0f : height;
            float s21 = (x > 0) ? 
                texData[((y) * texSize + (x-1)) * 3] / 255.0f : height;
            float s10 = (y < texSize-1) ? 
                texData[((y+1) * texSize + (x)) * 3] / 255.0f : height;
            float s12 = (y > 0) ? 
                texData[((y-1) * texSize + (x)) * 3] / 255.0f : height;
            
            glm::vec3 normal = glm::normalize(glm::vec3(
                (s21 - s01) * 2.0f,
                2.0f,
                (s12 - s10) * 2.0f
            ));
            
            // 将法转换到[0,1]范围
            normal = normal * 0.5f + glm::vec3(0.5f);
            
            // 存储到纹理数据
            int index = (y * texSize + x) * 3;
            texData[index + 0] = static_cast<unsigned char>(normal.x * 255);
            texData[index + 1] = static_cast<unsigned char>(normal.y * 255);
            texData[index + 2] = static_cast<unsigned char>(normal.z * 255);
        }
    }
    
    // 创建并设置纹理
    glGenTextures(1, &waterNormalTexture);
    glBindTexture(GL_TEXTURE_2D, waterNormalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, texData.data());
    
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void Water::createBubbleTexture() {
    qDebug() << "\n=== Creating Bubble Texture ===";
    
    // 删除旧纹理（如果存在）
    if(bubbleTexture != 0) {
        glDeleteTextures(1, &bubbleTexture);
        bubbleTexture = 0;
    }
    
    // 创建新纹理
    glGenTextures(1, &bubbleTexture);
    qDebug() << "Generated texture ID:" << bubbleTexture;
    
    if(bubbleTexture == 0) {
        qDebug() << "Failed to generate texture!";
        return;
    }
    
    // 生成更真实的气泡纹理
    const int texSize = 128;  // 增加纹理分辨率
    std::vector<unsigned char> texData(texSize * texSize * 4, 0);
    
    float center = texSize / 2.0f;
    for(int y = 0; y < texSize; ++y) {
        for(int x = 0; x < texSize; ++x) {
            float dx = (x - center) / center;
            float dy = (y - center) / center;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            if(dist < 1.0f) {
                // 创建气泡的边缘效果
                float edgeEffect = smoothstep(0.8f, 0.95f, dist);
                
                // 创建内部反光效果
                float highlight1 = std::max(0.0f, 1.0f - std::abs(dist - 0.3f) * 5.0f);
                float highlight2 = std::pow(std::max(0.0f, 1.0f - dist * 1.5f), 2.0f);
                
                // 创建折射效果
                float refraction = smoothstep(0.4f, 0.8f, dist);
                
                int idx = (y * texSize + x) * 4;
                // 基础颜色（略带蓝色）
                texData[idx + 0] = static_cast<unsigned char>((0.95f + highlight1 * 0.05f) * 255);
                texData[idx + 1] = static_cast<unsigned char>((0.97f + highlight1 * 0.03f) * 255);
                texData[idx + 2] = static_cast<unsigned char>((1.0f) * 255);
                
                // 计算透明度
                float alpha = (1.0f - edgeEffect) * (0.7f + highlight1 * 0.3f + highlight2 * 0.4f);
                texData[idx + 3] = static_cast<unsigned char>(alpha * 255);
            }
        }
    }
    
    // 绑定并设置纹理
    glBindTexture(GL_TEXTURE_2D, bubbleTexture);
    checkGLError("glBindTexture");
    
    // 上传纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
    checkGLError("glTexImage2D");
    
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    checkGLError("glTexParameteri");
}

void Water::updateCaustics()
{
    causticTime += 0.016f;
    // TODO: 更新焦散果
}

void Water::updateCausticAnimation(float deltaTime) {
    // 更新每层的偏移
    for(auto& layer : causticLayers) {
        layer.offset += layer.speed * deltaTime;
        if(layer.offset > 2.0f * glm::pi<float>()) {
            layer.offset -= 2.0f * glm::pi<float>();
        }
    }

    // 更新着色器中的焦散参数
    glUseProgram(waterProgram);
    
    // 传递焦散动画参数到着色器
    for(int i = 0; i < causticLayers.size(); ++i) {
        std::string prefix = "causticLayers[" + std::to_string(i) + "].";
        glUniform1f(glGetUniformLocation(waterProgram, (prefix + "scale").c_str()), 
                   causticLayers[i].scale * waterParams.causticScale);
        glUniform1f(glGetUniformLocation(waterProgram, (prefix + "offset").c_str()), 
                   causticLayers[i].offset);
        glUniform2fv(glGetUniformLocation(waterProgram, (prefix + "direction").c_str()),
                     1, glm::value_ptr(causticLayers[i].direction));
    }
    
    glUniform1i(glGetUniformLocation(waterProgram, "causticLayerCount"), 
                waterParams.causticLayers);
    glUniform1f(glGetUniformLocation(waterProgram, "causticBlend"), 
                waterParams.causticBlend);
    
    glUseProgram(0);
}

void Water::updateBubbles(float deltaTime) {
    // 调试输出
    static float debugTimer = 0.0f;
    debugTimer += deltaTime;
    if(debugTimer >= 1.0f) {
        qDebug() << "\n=== Bubble System Status ===";
        qDebug() << "Total bubbles:" << bubbles.size();
        if(!bubbles.empty()) {
            qDebug() << "First bubble:";
            qDebug() << "- Position:" << bubbles[0].position.x << bubbles[0].position.y << bubbles[0].position.z;
            qDebug() << "- Size:" << bubbles[0].size;
            qDebug() << "- Speed:" << bubbles[0].speed;
            qDebug() << "- Alpha:" << bubbles[0].alpha;
        }
        debugTimer = 0.0f;
    }

    // 更新现有气泡
    int removedBubbles = 0;
    int activeBubbles = 0;
    for(auto it = bubbles.begin(); it != bubbles.end();) {
        Bubble& bubble = *it;
        
        // 更新位置
        bubble.position.y += bubble.speed * deltaTime;
        
        // 添加螺旋运动
        float spiralTime = waterTime * 0.5f + bubble.phase;
        bubble.position.x += sin(spiralTime) * bubble.wobble * deltaTime * 2.0f;
        bubble.position.z += cos(spiralTime) * bubble.wobble * deltaTime * 2.0f;
        
        // 更新相位
        bubble.phase += deltaTime * 2.0f;
        
        // 根据高度调整速度和大小
        float heightFactor = (bubble.position.y + size) / (size * 2.0f);
        bubble.speed = glm::mix(bubble.speed, bubble.speed * (1.0f + heightFactor * 0.8f), deltaTime);
        bubble.size = glm::mix(bubble.size, bubble.size * (1.0f + heightFactor * 0.2f), deltaTime);
        
        // 检查是否超出范围
        if(bubble.position.y > size * 0.5f) {
            spawnBubble();  // 生成新气泡
            it = bubbles.erase(it);  // 移除当前气泡
            removedBubbles++;
        } else {
            ++it;
            activeBubbles++;
        }
    }
    
    // 维持气泡数量
    int spawnedBubbles = 0;
    while(bubbles.size() < MAX_BUBBLES) {
        spawnBubble();
        spawnedBubbles++;
    }

    if(removedBubbles > 0 || spawnedBubbles > 0) {
        qDebug() << "Bubble updates:";
        qDebug() << "- Removed:" << removedBubbles;
        qDebug() << "- Spawned:" << spawnedBubbles;
        qDebug() << "- Active:" << activeBubbles;
    }
}

void Water::renderBubbles() {
    if(bubbles.empty()) {
        qDebug() << "No bubbles to render!";
        return;
    }

    // 保存当前OpenGL状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // 设置渲染状态
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 启用点精灵
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    
    // 禁用深度写入但保持深度测试
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    
    // 绑定纹理
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, bubbleTexture);
    
    // 设置投影和模型视图矩阵
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projectionMatrix));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(viewMatrix));
    
    // 渲染气泡
    glBegin(GL_POINTS);
    for(const auto& bubble : bubbles) {
        if(bubble.size <= 0.0f) continue; // 跳过被合并的气泡
        
        // 计算视角相关参数
        glm::vec3 toCam = cameraPos - bubble.position;
        float distToCam = glm::length(toCam);
        float sizeScale = std::min(1.5f, 1000.0f / distToCam);
        
        // 计算变形后的大小
        float deformedSize = bubble.size * (1.0f + bubble.deformation) * sizeScale;
        glPointSize(deformedSize);
        
        // 设置颜色，包含反光效果
        float highlightFactor = bubble.highlightIntensity * (1.0f - distToCam / 2000.0f);
        glColor4f(
            1.0f + highlightFactor * 0.2f,
            1.0f + highlightFactor * 0.2f,
            1.0f + highlightFactor * 0.3f,
            bubble.alpha
        );
        
        glVertex3f(bubble.position.x, bubble.position.y, bubble.position.z);
    }
    glEnd();
    
    // 恢复OpenGL状态
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_POINT_SPRITE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    
    glPopAttrib();
}

void Water::renderWaterParticles() {
    // 调试输出
    qDebug() << "Rendering water particles... Count:" << waterParticles.size();
    qDebug() << "Particle size range:" << PARTICLE_MIN_SIZE << " - " << PARTICLE_MAX_SIZE;
    
    // 保存当前OpenGL状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // 启用点精灵
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    
    // 获取并输出点大小范围
    GLfloat sizes[2];
    glGetFloatv(GL_POINT_SIZE_RANGE, sizes);
    qDebug() << "OpenGL point size range:" << sizes[0] << " - " << sizes[1];
    
    // 启用混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 禁用深度写入但保持深度测试
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    
    // 绑定粒子纹理
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, waterParticleTexture);
    
    // 设置点精灵参数
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    
    // 设置点大小范围
    glPointSize(PARTICLE_MAX_SIZE);  // 设置最大点大小
    
    // 设置点大小衰减
    GLfloat quadratic[] = { 0.0f, 0.0f, 0.00001f };  // 减小衰减系数
    glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, quadratic);
    
    // 开始渲染粒子
    glBegin(GL_POINTS);
    int visibleParticles = 0;
    for(const auto& particle : waterParticles) {
        if(particle.life <= 0.0f) continue;
        
        // 计算基于距离的大小衰减
        glm::vec3 toCamera = cameraPos - particle.position;
        float distanceToCamera = glm::length(toCamera);
        
        // 改进距离衰减计算
        float sizeScale = 1.0f;
        if(distanceToCamera > 0.0f) {
            sizeScale = std::min(2.0f, 2000.0f / distanceToCamera);
        }
        
        // 确保粒子大小在有效范围内
        float finalSize = particle.size * sizeScale;
        finalSize = std::max(PARTICLE_MIN_SIZE, std::min(PARTICLE_MAX_SIZE, finalSize));
        
        // 设置粒子大小
        glPointSize(finalSize);
        
        // 设置粒子颜色和透明度
        glColor4f(
            particle.color.r,
            particle.color.g,
            particle.color.b,
            particle.alpha * std::min(1.0f, sizeScale)  // 根据距离调整透明度
        );
        
        // 输出一些粒子的大小信息（仅输出前几个粒子）
        if(visibleParticles < 5) {
            qDebug() << "Particle" << visibleParticles 
                     << "original size:" << particle.size
                     << "final size:" << finalSize 
                     << "distance:" << distanceToCamera 
                     << "scale:" << sizeScale;
        }
        
        // 渲染粒子
        glVertex3f(
            particle.position.x,
            particle.position.y,
            particle.position.z
        );
        
        visibleParticles++;
    }
    glEnd();
    
    qDebug() << "Visible particles rendered:" << visibleParticles;
    
    // 恢复OpenGL状态
    glDepthMask(GL_TRUE);
    glDisable(GL_POINT_SPRITE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    
    glPopAttrib();
}

void Water::spawnBubble() {
    // 调试输出
    qDebug() << "\n=== Spawning Bubble ===";
    qDebug() << "Current bubble count:" << bubbles.size();
    qDebug() << "MAX_BUBBLES:" << MAX_BUBBLES;
    
    Bubble bubble;
    
    // 在底部区域随机生成
    float radius = std::pow(rand() / float(RAND_MAX), 2.0f) * size * 0.3f;
    float angle = (rand() / float(RAND_MAX)) * glm::two_pi<float>();
    
    bubble.position = glm::vec3(
        radius * cos(angle),
        -size * 0.45f,
        radius * sin(angle)
    );
    
    // 基础属性设置
    bubble.size = MIN_BUBBLE_SIZE + 
                 std::pow(rand() / float(RAND_MAX), 2.0f) * (MAX_BUBBLE_SIZE - MIN_BUBBLE_SIZE);
    bubble.speed = 15.0f + (rand() / float(RAND_MAX)) * 10.0f;
    bubble.wobble = 0.2f + (rand() / float(RAND_MAX)) * 0.3f;
    bubble.phase = rand() / float(RAND_MAX) * glm::two_pi<float>();
    bubble.alpha = BUBBLE_BASE_ALPHA;
    
    // 新属性初始化
    bubble.deformation = 0.0f;
    bubble.pulsePhase = rand() / float(RAND_MAX) * glm::two_pi<float>();
    bubble.refractionIndex = 1.2f + (rand() / float(RAND_MAX)) * 0.1f;
    bubble.highlightIntensity = 0.8f + (rand() / float(RAND_MAX)) * 0.2f;
    bubble.merging = false;
    bubble.mergeProgress = 0.0f;
    bubble.mergingWith = nullptr;
    
    bubbles.push_back(bubble);
}

void Water::saveGLState() {
    glGetBooleanv(GL_FOG, &originalState.fog);
    glGetBooleanv(GL_LIGHTING, &originalState.lighting);
    glGetBooleanv(GL_DEPTH_TEST, &originalState.depthTest);
    glGetFloatv(GL_FOG_COLOR, originalState.fogParams);
    glGetFloatv(GL_LIGHT_MODEL_AMBIENT, originalState.lightModelAmbient);
}

void Water::restoreGLState() {
    // 恢复原始状态
    if(originalState.fog)
        glEnable(GL_FOG);
    else
        glDisable(GL_FOG);
        
    if(originalState.lighting)
        glEnable(GL_LIGHTING);
    else
        glDisable(GL_LIGHTING);
        
    if(originalState.depthTest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
        
    glFogfv(GL_FOG_COLOR, originalState.fogParams);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, originalState.lightModelAmbient);
}

void Water::beginUnderwaterEffect(const glm::mat4& proj, const glm::mat4& view) {
    // 保存当前状态
    saveGLState();
    
    // 确保启用必要的状态
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
    
    // 设置更柔和的雾气参数
    glFogi(GL_FOG_MODE, GL_EXP2);
    glHint(GL_FOG_HINT, GL_NICEST);
    
    // 调整雾气参数 - 使用更温和的基础值
    float baseDensity = 0.001f;  // 降低基础雾密度
    float depthFactor = std::max(0.0f, -cameraPos.y);  // 直接使用深度��不再除以size
    float currentDensity = baseDensity * (1.0f + depthFactor * 0.0005f);  // 大幅降低深度影响
    
    // 使用更柔和的颜色过渡
    glm::vec3 currentFogColor = glm::mix(
        waterParams.shallowColor,  // 使用原始浅水颜色
        waterParams.deepColor,
        std::min(0.3f, depthFactor * 0.001f)  // 降低深度对颜色的影响
    );
    
    // 应用雾气参数
    GLfloat fogColor[] = {
        currentFogColor.r,
        currentFogColor.g,
        currentFogColor.b,
        1.0f
    };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, currentDensity);
    
    // 调整雾气起始和结束距离
    glFogf(GL_FOG_START, 0.0f);  // ���相机位置开始
    glFogf(GL_FOG_END, size * 5.0f);  // 延长雾气结束距离
    
    // 增强环境光以改善可见度
    GLfloat ambient[] = {
        currentFogColor.r * 1.2f,  // 显著增加环境光强度
        currentFogColor.g * 1.2f,
        currentFogColor.b * 1.2f,
        1.0f
    };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    
    // 保存当前视图和投影矩阵
    projectionMatrix = proj;
    viewMatrix = view;
}

void Water::endUnderwaterEffect() {
    // 恢复原始状态
    restoreGLState();
}

void Water::update(float deltaTime) {
    waterTime += deltaTime;
    
    // 更新水下粒子
    updateUnderwaterParticles(deltaTime);
    
    // 调试输出当前状态
    qDebug() << "\n=== Water System Update ===";
    qDebug() << "Water time:" << waterTime;
    qDebug() << "Current bubble count:" << bubbles.size();
    
    // 确保始终维持足够的气泡数量
    while(bubbles.size() < MAX_BUBBLES) {
        qDebug() << "Spawning new bubble to maintain count";
        spawnBubble();
    }
    
    // 更新现有气泡
    for(auto it = bubbles.begin(); it != bubbles.end();) {
        Bubble& bubble = *it;
        
        // 更新气泡位置和状态
        updateBubble(bubble, deltaTime);
        
        // 检查气泡是否超出范围
        if(bubble.position.y > size * 0.5f) {
            qDebug() << "Bubble reached top, removing and spawning new one";
            it = bubbles.erase(it);
            spawnBubble();  // 立即生成新气泡
        } else {
            ++it;
        }
    }
    
    // 验证气泡数量
    if(bubbles.size() != MAX_BUBBLES) {
        qDebug() << "Warning: Bubble count mismatch!";
        qDebug() << "Expected:" << MAX_BUBBLES;
        qDebug() << "Actual:" << bubbles.size();
    }
    
    // 更新焦散动画
    updateCausticAnimation(deltaTime);
}

bool Water::validateShaderProgram() {
    GLint status;
    glGetProgramiv(waterProgram, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint logLength;
        glGetProgramiv(waterProgram, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength);
        glGetProgramInfoLog(waterProgram, logLength, nullptr, log.data());
        qDebug() << "Shader program linking failed:" << log.data();
        return false;
    }
    
    // 验证uniform变量位置
    GLint loc;
    loc = glGetUniformLocation(waterProgram, "time");
    if(loc == -1) qDebug() << "Warning: Uniform 'time' not found";
    
    loc = glGetUniformLocation(waterProgram, "waterDensity");
    if(loc == -1) qDebug() << "Warning: Uniform 'waterDensity' not found";
    
    loc = glGetUniformLocation(waterProgram, "visibilityFalloff");
    if(loc == -1) qDebug() << "Warning: Uniform 'visibilityFalloff' not found";
    
    return true;
}

bool Water::checkTextureState() {
    bool success = true;
    if (glIsTexture(causticTexture) == GL_FALSE) {
        qDebug() << "Caustic texture not valid!";
        success = false;
    }
    if (glIsTexture(waterNormalTexture) == GL_FALSE) {
        qDebug() << "Water normal texture not valid!";
        success = false;
    }
    if (glIsTexture(bubbleTexture) == GL_FALSE) {
        qDebug() << "Bubble texture not valid!";
        success = false;
    }
    return success;
}

void Water::initWaterParticles() {
    // 创建水下颗粒纹理
    const int texSize = 256;  // 增大纹理尺寸，从64改为256
    std::vector<unsigned char> texData(texSize * texSize * 4);
    
    for(int y = 0; y < texSize; ++y) {
        for(int x = 0; x < texSize; ++x) {
            float dx = (x - texSize/2.0f) / (texSize/2.0f);
            float dy = (y - texSize/2.0f) / (texSize/2.0f);
            float dist = std::sqrt(dx*dx + dy*dy);
            
            // 创建更大更明显的粒子效果
            float alpha = std::max(0.0f, 1.0f - dist);
            alpha = std::pow(alpha, 1.5f);  // 调整边缘过渡
            
            // 增强发光效果
            float glow = std::max(0.0f, 1.0f - dist * 1.2f);
            glow = std::pow(glow, 1.8f);
            
            int idx = (y * texSize + x) * 4;
            texData[idx + 0] = static_cast<unsigned char>((0.9f + glow * 0.1f) * 255);  // R
            texData[idx + 1] = static_cast<unsigned char>((0.95f + glow * 0.05f) * 255);  // G
            texData[idx + 2] = static_cast<unsigned char>(255);  // B
            texData[idx + 3] = static_cast<unsigned char>(alpha * 255);
        }
    }
    
    glGenTextures(1, &waterParticleTexture);
    glBindTexture(GL_TEXTURE_2D, waterParticleTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
    
    // 修改纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);  // 启用mipmap
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);  // 生成mipmap
    
    // 初始化颗粒
    waterParticles.resize(MAX_WATER_PARTICLES);
    for(auto& particle : waterParticles) {
        generateWaterParticle(particle, glm::vec3(0.0f));
    }
    
    qDebug() << "Water particle texture initialized with size:" << texSize;
    qDebug() << "Initial particle count:" << waterParticles.size();
}

void Water::generateWaterParticle(WaterParticle& particle, const glm::vec3& targetPos) {
    // 在球形空间内随机生成位置，增大生成范围
    float theta = (rand() / float(RAND_MAX)) * glm::two_pi<float>();
    float phi = (rand() / float(RAND_MAX)) * glm::pi<float>();
    float radius = PARTICLE_SPAWN_RADIUS * 2.0f * std::pow(rand() / float(RAND_MAX), 0.3f);
    
    // 使用球坐标系生成偏移
    float x = radius * sin(phi) * cos(theta);
    float y = radius * sin(phi) * sin(theta);
    float z = radius * cos(phi);
    
    // 设置粒子位置，直接使用目标位置加上偏移
    particle.position = targetPos + glm::vec3(x, y, z);
    
    // 调试输出
    static bool firstParticle = true;
    if(firstParticle) {
        qDebug() << "Generating particle at:" 
                 << particle.position.x << particle.position.y << particle.position.z;
        firstParticle = false;
    }
    
    // 给予较小的随机初始速度
    float speedFactor = 0.5f;  // 增加速度
    particle.velocity = glm::vec3(
        (rand() / float(RAND_MAX) - 0.5f) * speedFactor,
        (rand() / float(RAND_MAX) - 0.5f) * speedFactor,
        (rand() / float(RAND_MAX) - 0.5f) * speedFactor
    );
    
    // 使用指数分布生成粒子大小，使小粒子更常见
    float randomValue = rand() / float(RAND_MAX);
    float sizeRange = PARTICLE_MAX_SIZE - PARTICLE_MIN_SIZE;
    float size = PARTICLE_MIN_SIZE + sizeRange * std::pow(randomValue, 2.5f);
    particle.size = size;
    
    // 根据粒子大小调整透明度
    float sizeRatio = (size - PARTICLE_MIN_SIZE) / (PARTICLE_MAX_SIZE - PARTICLE_MIN_SIZE);
    particle.alpha = PARTICLE_MIN_ALPHA + (PARTICLE_MAX_ALPHA - PARTICLE_MIN_ALPHA) * (1.0f - sizeRatio * 0.5f);
    particle.targetAlpha = particle.alpha;
    
    // 设置粒子颜色为亮蓝色，较大粒子颜色更深
    float colorIntensity = 1.0f - sizeRatio * 0.3f;
    particle.color = glm::vec3(
        0.4f * colorIntensity,
        0.6f * colorIntensity,
        1.0f * colorIntensity
    );
    
    // 使用PARTICLE_LIFE_MIN和PARTICLE_LIFE_MAX来设置生命周期
    particle.life = PARTICLE_LIFE_MIN + (rand() / float(RAND_MAX)) * (PARTICLE_LIFE_MAX - PARTICLE_LIFE_MIN);
    particle.fadeState = 0.0f;
    
    // 调试输出前几个粒子的信息
    static int particleCount = 0;
    if(particleCount < 5) {
        qDebug() << "Generated particle" << particleCount 
                 << "size:" << particle.size
                 << "alpha:" << particle.alpha
                 << "color:" << particle.color.x << particle.color.y << particle.color.z;
        particleCount++;
    }
}

void Water::updateWaterParticles(float deltaTime, const glm::vec3& targetPos) {
    static float spawnTimer = 0.0f;
    spawnTimer += deltaTime;
    
    // 增加粒子生成频率
    if (spawnTimer >= 0.01f) {  // 更频繁地生成粒子
        spawnTimer = 0.0f;
        int particlesToSpawn = 100;  // 每次生成更多粒子
        
        for (int i = 0; i < particlesToSpawn; ++i) {
            auto it = std::find_if(waterParticles.begin(), waterParticles.end(),
                [](const WaterParticle& p) { return p.life <= 0.0f; });
            
            if (it != waterParticles.end()) {
                generateWaterParticle(*it, targetPos);
            } else if (waterParticles.size() < MAX_WATER_PARTICLES) {
                WaterParticle newParticle;
                generateWaterParticle(newParticle, targetPos);
                waterParticles.push_back(newParticle);
            }
        }
    }
    
    // 更新现有粒子
    for(auto& particle : waterParticles) {
        if(particle.life <= 0.0f) continue;
        
        // 更新位置
        particle.position += particle.velocity * deltaTime;
        
        // 使用PARTICLE_FADE_TIME进行淡入淡出
        if (particle.life > particle.fadeState + PARTICLE_FADE_TIME) {
            // 淡入阶段
            if (particle.fadeState < PARTICLE_FADE_TIME) {
                particle.fadeState += deltaTime;
                particle.alpha = particle.targetAlpha * (particle.fadeState / PARTICLE_FADE_TIME);
            }
        } else {
            // 淡出阶段
            float fadeOutTime = particle.life;
            particle.alpha = (fadeOutTime / PARTICLE_FADE_TIME) * particle.targetAlpha;
        }
        
        // 更新生命周期
        particle.life -= deltaTime;
        
        // 添加一些随机运动
        particle.velocity += glm::vec3(
            (rand() / float(RAND_MAX) - 0.5f) * 2.0f,
            (rand() / float(RAND_MAX) - 0.5f) * 2.0f,
            (rand() / float(RAND_MAX) - 0.5f) * 2.0f
        ) * deltaTime;
    }
    
    // 调试输出活跃粒子数量
    int activeParticles = std::count_if(waterParticles.begin(), waterParticles.end(),
        [](const WaterParticle& p) { return p.life > 0.0f; });
    qDebug() << "Active particles:" << activeParticles;
}

void Water::updateBubble(Bubble& bubble, float deltaTime) {
    // 基础上升运动
    bubble.position.y += bubble.speed * deltaTime;
    
    // 气泡脉动
    bubble.pulsePhase += PULSE_SPEED * deltaTime;
    float pulseFactor = sin(bubble.pulsePhase) * 0.1f + 1.0f;
    
    // 水平运动使用复合正弦波
    float wobbleFrequency = 1.5f;
    bubble.phase += wobbleFrequency * deltaTime;
    
    // 模拟水流影响
    float waterFlow = sin(bubble.position.y * 0.02f + waterTime * 0.5f) * 0.5f;
    
    // 计算运动
    float primaryWobble = sin(bubble.phase) * bubble.wobble;
    float secondaryWobble = sin(bubble.phase * 0.5f) * bubble.wobble * 0.3f;
    
    // 应用运动和变形
    bubble.position.x += (primaryWobble + secondaryWobble + waterFlow) * deltaTime;
    bubble.position.z += (cos(bubble.phase) * bubble.wobble) * deltaTime;
    
    // 随机扰动
    float randomFactor = 0.05f;
    glm::vec3 randomMotion(
        ((rand() / float(RAND_MAX)) * 2.0f - 1.0f),
        ((rand() / float(RAND_MAX)) * 2.0f - 1.0f),
        ((rand() / float(RAND_MAX)) * 2.0f - 1.0f)
    );
    bubble.position += randomMotion * randomFactor * deltaTime;
    
    // 计算变形
    float targetDeformation = glm::length(randomMotion) * MAX_DEFORMATION;
    bubble.deformation = glm::mix(bubble.deformation, targetDeformation, deltaTime * 2.0f);
    
    // 更新大小和透明度
    float heightFactor = (bubble.position.y + size) / (size * 2.0f);
    float currentSize = bubble.size * pulseFactor;
    bubble.size = glm::mix(currentSize, currentSize * 1.1f, heightFactor * deltaTime);
    bubble.alpha = glm::mix(BUBBLE_BASE_ALPHA, BUBBLE_BASE_ALPHA * 0.6f, heightFactor);
    
    // 处理气泡合并
    if(!bubble.merging) {
        for(auto& other : bubbles) {
            if(&other != &bubble && !other.merging) {
                float dist = glm::distance(bubble.position, other.position);
                if(dist < MERGE_DISTANCE && bubble.size >= other.size) {
                    bubble.merging = true;
                    bubble.mergingWith = &other;
                    bubble.mergeProgress = 0.0f;
                    break;
                }
            }
        }
    } else if(bubble.mergingWith) {
        bubble.mergeProgress += deltaTime;
        float t = std::min(1.0f, bubble.mergeProgress);
        
        // 合并位置和大小
        bubble.position = glm::mix(bubble.position, bubble.mergingWith->position, t);
        bubble.size = glm::mix(bubble.size, 
                             std::sqrt(bubble.size * bubble.size + 
                                     bubble.mergingWith->size * bubble.mergingWith->size), 
                             t);
        
        if(t >= 1.0f) {
            bubble.merging = false;
            bubble.mergingWith->size = 0.0f; // 标记要移除的气泡
        }
    }
}

void Water::dumpOpenGLState() {
    qDebug() << "\n=== OpenGL State Dump ===";
    
    // 获取当前绑定的着色器程序
    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    qDebug() << "Current shader program:" << currentProgram;
    
    // 检查混合状态
    GLint blendEnabled;
    glGetIntegerv(GL_BLEND, &blendEnabled);
    qDebug() << "Blend enabled:" << (blendEnabled == GL_TRUE);
    
    // 检查深度测试状��
    GLint depthTestEnabled;
    glGetIntegerv(GL_DEPTH_TEST, &depthTestEnabled);
    qDebug() << "Depth test enabled:" << (depthTestEnabled == GL_TRUE);
    
    // 检查深度写入状态
    GLint depthMask;
    glGetIntegerv(GL_DEPTH_WRITEMASK, &depthMask);
    qDebug() << "Depth mask enabled:" << (depthMask == GL_TRUE);
    
    // 检查当前���定的纹理
    GLint boundTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    qDebug() << "Current bound texture:" << boundTexture;
    
    // 检查视口设置
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    qDebug() << "Viewport:" << viewport[0] << viewport[1] << viewport[2] << viewport[3];
    
    // 检查点大小范围
    GLfloat pointSizeRange[2];
    glGetFloatv(GL_POINT_SIZE_RANGE, pointSizeRange);
    qDebug() << "Point size range:" << pointSizeRange[0] << "-" << pointSizeRange[1];
    
    // 检查当前点大小
    GLfloat pointSize;
    glGetFloatv(GL_POINT_SIZE, &pointSize);
    qDebug() << "Current point size:" << pointSize;
    
    // 检查错误
    GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        qDebug() << "OpenGL error during state dump:" << error;
    }
}

void Water::checkGLError(const char* operation) {
    GLenum error;
    while((error = glGetError()) != GL_NO_ERROR) {
        QString errorString;
        switch(error) {
            case GL_INVALID_ENUM:
                errorString = "GL_INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                errorString = "GL_INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                errorString = "GL_INVALID_OPERATION";
                break;
            case GL_STACK_OVERFLOW:
                errorString = "GL_STACK_OVERFLOW";
                break;
            case GL_STACK_UNDERFLOW:
                errorString = "GL_STACK_UNDERFLOW";
                break;
            case GL_OUT_OF_MEMORY:
                errorString = "GL_OUT_OF_MEMORY";
                break;
            default:
                errorString = QString("Unknown error %1").arg(error);
        }
        qDebug() << "OpenGL error after" << operation << ":" << errorString;
    }
}

void Water::setCameraPosition(const glm::vec3& pos) {
    cameraPos = pos;
    
    // 水面高度
    float waterHeight = size * 0.45f;
    
    // 判断是否在水面以下
    bool isInside = pos.y < waterHeight;
    
    // 更新水下状态
    if(underwaterState.isUnderwater != isInside) {
        underwaterState.isUnderwater = isInside;
        
        // 根据是否在水下调整参数
        if(underwaterState.isUnderwater) {
            // 水下状态 - 使用更强的水下效果
            underwaterState.fogDensity = 0.002f;  // 增加基础雾气密度
            underwaterState.fogColor = glm::vec3(0.1f, 0.2f, 0.3f);
            underwaterState.ambientIntensity = 0.5f;  // 增加基础环境光强度
        } else {
            // 水上状态
            underwaterState.fogDensity = 0.0f;
            underwaterState.fogColor = glm::vec3(0.5f, 0.7f, 0.9f);
            underwaterState.ambientIntensity = 1.0f;
        }
    } else if(underwaterState.isUnderwater) {
        // 如果已经在水下，持续更新深度相关效果
        float depth = waterHeight - pos.y;  // 计算与水面的深度差
        float depthFactor = std::min(1.0f, depth / (size * 2.0f));  // 降低深度影响
        underwaterState.fogDensity = 0.002f * (1.0f + depthFactor * 0.3f);  // 减小深度对雾气的影响
        underwaterState.fogColor = glm::vec3(0.1f, 0.2f, 0.3f) * (1.0f - depthFactor * 0.2f);  // 减小深度对颜色的影响
        underwaterState.ambientIntensity = 0.5f * (1.0f - depthFactor * 0.3f);  // 减小深度对环境光的影响
    }
}