#define GLM_ENABLE_EXPERIMENTAL
#include "water.h"
#include <QDebug>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/type_ptr.hpp>

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

    void main()
    {
        float viewDistance = length(FragPos - cameraPos);
        float depthValue = gl_FragCoord.z / gl_FragCoord.w;
        
        // 计算水下效果
        bool isUnderwater = cameraPos.y < FragPos.y;
        float waterDepth = abs(FragPos.y - cameraPos.y);
        
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
    
    if (!glIsTexture(causticTexture) || !glIsTexture(waterNormalTexture) ||
        !glIsTexture(bubbleTexture) || !glIsTexture(volumetricLightTexture) ||
        !glIsTexture(waterParticleTexture)) {
        qDebug() << "Failed to create one or more textures!";
        return;
    }
    qDebug() << "All textures created successfully";
    
    // 初始化各个组件
    qDebug() << "\nInitializing components...";
    initCausticTexture();
    initWaterNormalTexture();
    createBubbleTexture();
    initVolumetricLight();
    initWaterParticles();
    
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
    
    // 检查OpenGL错误
    GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        qDebug() << "OpenGL error after initialization:" << error;
    } else {
        qDebug() << "No OpenGL errors during initialization";
    }
}

void Water::initParticleSystem() {
    qDebug() << "\n=== Initializing Particle System ===";
    
    // 清空并重新生成气泡
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

    // 将水设置在水族箱顶部
    float waterHeight = size * 0.5f; // 水族箱高���是总尺寸的一半
    float vertices[] = {
        // 顶面
        -size,  waterHeight, -size,  0.0f, 0.0f,  // 左后
         size,  waterHeight, -size,  1.0f, 0.0f,  // 右后
         size,  waterHeight,  size,  1.0f, 1.0f,  // 右前
        -size,  waterHeight, -size,  0.0f, 0.0f,  // 左后
         size,  waterHeight,  size,  1.0f, 1.0f,  // 右前
        -size,  waterHeight,  size,  0.0f, 1.0f,  // 左前
    };

    vertexCount = 6;

    glBindVertexArray(waterVAO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 位属性
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
    
    // 禁用面剔��以允许双面渲染
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
    qDebug() << "Bubble count:" << bubbles.size();
    qDebug() << "Particle count:" << waterParticles.size();
    qDebug() << "Is underwater:" << (cameraPos.y < 0.0f);
    
    // 渲染气泡和粒子
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
    
    if(cameraPos.y < 0) {
        float depth = -cameraPos.y;
        
        // 移除视角相的影响，只使用深度
        float depthFactor = glm::min(0.3f, depth / (size * 0.8f));
        
        // 计算目标颜色，不虑角
        glm::vec3 targetColor = glm::mix(
            waterParams.underwaterColor,
            waterParams.deepColor,
            depthFactor
        );
        
        currentUnderwaterColor = glm::mix(
            currentUnderwaterColor,
            targetColor,
            0.01f  // 降低过渡速度
        );
        
        float targetDensity = waterParams.underwaterScatteringDensity * 
            (1.0f + depth * 0.00002f);  // 进一步降低深度影响
            
        currentScatteringDensity = glm::mix(
            currentScatteringDensity,
            targetDensity,
            0.01f  // 降低过渡速度
        );
    }
    
    // 确保在每帧更下效果数
    glUseProgram(waterProgram);
    
    // 更新相机位���
    glUniform3fv(glGetUniformLocation(waterProgram, "cameraPosition"), 1,
                 glm::value_ptr(cameraPos));
                 
    // 根据深度更新散射密度
    if(cameraPos.y < 0.0f) {
        float depth = -cameraPos.y;
        float adjustedDensity = waterParams.underwaterScatteringDensity * 
                               (1.0f + depth * 0.01f);  // 随深度增加散射
        glUniform1f(glGetUniformLocation(waterProgram, "underwaterScatteringDensity"), 
                   adjustedDensity);
    }
    
    // 添加视角方向检和平滑过渡
    static glm::vec3 lastCameraDir = glm::vec3(0.0f);
    glm::vec3 currentCameraDir = glm::normalize(-cameraPos);
    
    // 检测视角突变
    float directionChange = glm::dot(lastCameraDir, currentCameraDir);
    if(directionChange < 0.7f) { // 大约45度以上的突变
        // 使用��平滑的过渡
        currentCameraDir = glm::normalize(glm::mix(lastCameraDir, currentCameraDir, 0.1f));
    }
    
    lastCameraDir = currentCameraDir;
    
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
    
    particle.size = 2.0f + float(rand()) / RAND_MAX * 4.0f;
    particle.life = 0.5f + float(rand()) / RAND_MAX * 0.5f;
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
    
    // 检查是否已存在纹理
    if(bubbleTexture != 0) {
        if(glIsTexture(bubbleTexture)) {
            qDebug() << "Deleting existing bubble texture";
            glDeleteTextures(1, &bubbleTexture);
        }
        bubbleTexture = 0;
    }

    const int texSize = 64;  // 纹理尺寸
    std::vector<unsigned char> texData(texSize * texSize * 4);  // RGBA格式
    
    qDebug() << "Creating texture with size:" << texSize << "x" << texSize;
    
    float center = texSize / 2.0f;
    float maxDist = center * 0.9f;
    
    // 生成纹理数据
    for(int y = 0; y < texSize; ++y) {
        for(int x = 0; x < texSize; ++x) {
            float dx = (x - center) / center;
            float dy = (y - center) / center;
            float dist = sqrt(dx * dx + dy * dy);
            
            int index = (y * texSize + x) * 4;
            
            if(dist < 0.9f) {
                // 创建一个软边缘的圆形
                float alpha = dist < 0.7f ? 0.7f : (0.9f - dist) / 0.2f;
                
                // 添加边缘高光
                float edgeHighlight = std::max(0.0f, 1.0f - abs(dist - 0.8f) * 10.0f);
                float highlight = std::max(0.0f, 1.0f - (dist * 2.0f));
                
                // 设置颜色值
                texData[index + 0] = static_cast<unsigned char>((0.8f + highlight * 0.2f) * 255);  // R
                texData[index + 1] = static_cast<unsigned char>((0.9f + highlight * 0.1f) * 255);  // G
                texData[index + 2] = static_cast<unsigned char>((1.0f) * 255);                     // B
                texData[index + 3] = static_cast<unsigned char>(alpha * (0.6f + edgeHighlight * 0.4f) * 255); // A
            } else {
                // 完全透明的部分
                texData[index + 0] = 0;
                texData[index + 1] = 0;
                texData[index + 2] = 0;
                texData[index + 3] = 0;
            }
        }
    }
    
    // 创建纹理
    glGenTextures(1, &bubbleTexture);
    qDebug() << "Generated texture ID:" << bubbleTexture;
    
    // 检查纹理是否创建成功
    if(bubbleTexture == 0) {
        qDebug() << "Failed to generate texture!";
        return;
    }
    
    // 绑定并设置纹理
    glBindTexture(GL_TEXTURE_2D, bubbleTexture);
    
    // 检查是否成功绑定
    GLint boundTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    qDebug() << "Bound texture ID:" << boundTexture;
    
    if(boundTexture != bubbleTexture) {
        qDebug() << "Failed to bind texture!";
        return;
    }
    
    // 上传纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
    
    // 检查是否有错误
    GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        qDebug() << "Error uploading texture data:" << error;
        return;
    }
    
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // 再次检查错误
    error = glGetError();
    if(error != GL_NO_ERROR) {
        qDebug() << "Error setting texture parameters:" << error;
        return;
    }
    
    // 验证纹理创建是否成功
    if(!glIsTexture(bubbleTexture)) {
        qDebug() << "Texture validation failed!";
        return;
    }
    
    qDebug() << "Bubble texture created successfully";
    
    // 解绑纹理
    glBindTexture(GL_TEXTURE_2D, 0);
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

    qDebug() << "\n=== Rendering Bubbles ===";
    qDebug() << "Number of bubbles:" << bubbles.size();

    // 保存当前OpenGL状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // 设置渲染状态
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // 使用加法混合，与粒子保持一致
    
    // 启用点精灵
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    
    // 禁用深度写入但保持深度测试
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    
    // 确保气泡纹理存在并绑定
    if(bubbleTexture == 0) {
        qDebug() << "Creating bubble texture...";
        createBubbleTexture();
    }
    
    // 检查纹理状态
    if (!glIsTexture(bubbleTexture)) {
        qDebug() << "Error: Invalid bubble texture!";
        return;
    }
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, bubbleTexture);
    
    // 检查纹理绑定状态
    GLint boundTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    qDebug() << "Bound texture ID:" << boundTexture;
    
    // 设置投影和模型视图矩阵
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projectionMatrix));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(viewMatrix));
    
    // 获取并设置最大点大小
    GLfloat maxSize;
    glGetFloatv(GL_POINT_SIZE_MAX, &maxSize);
    qDebug() << "Maximum point size:" << maxSize;
    
    // 增加点大小
    glPointSize(50.0f);  // 设置一个较大的固定点大小进行测试
    
    // 设置固定颜色进行测试
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);  // 完全不透明的白色
    
    // 渲染气泡
    glBegin(GL_POINTS);
    int renderedBubbles = 0;
    for(const auto& bubble : bubbles) {
        // 直接渲染，不进行距离计算
        glVertex3f(bubble.position.x, bubble.position.y, bubble.position.z);
        renderedBubbles++;
        
        // 输出第一个气泡的详细信息
        if(renderedBubbles == 1) {
            qDebug() << "First bubble details:";
            qDebug() << "- Position:" << bubble.position.x << bubble.position.y << bubble.position.z;
            qDebug() << "- Size:" << bubble.size;
            qDebug() << "- Alpha:" << bubble.alpha;
        }
    }
    glEnd();
    
    qDebug() << "Rendered" << renderedBubbles << "bubbles";
    
    // 检查渲染后的状态
    GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        qDebug() << "OpenGL error after rendering bubbles:" << error;
    }
    
    // 恢复OpenGL状态
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_POINT_SPRITE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    
    glPopAttrib();
    
    // 再次检查错误
    error = glGetError();
    if(error != GL_NO_ERROR) {
        qDebug() << "OpenGL error after state restoration:" << error;
    }
}

void Water::renderWaterParticles() {
    // 调试输出
    qDebug() << "Rendering water particles... Count:" << waterParticles.size();
    
    // 保存当前OpenGL状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // 启用点精灵
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    // 启用混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // 使用加法混合使粒子更亮
    
    // 禁用深度写入但保持深度测试
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    
    // 绑定粒子纹理
    glBindTexture(GL_TEXTURE_2D, waterParticleTexture);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    
    // 开始渲染粒子
    glBegin(GL_POINTS);
    int visibleParticles = 0;
    for(const auto& particle : waterParticles) {
        if(particle.life <= 0.0f) continue;
        
        // 设置粒子颜色和透明度
        glColor4f(
            particle.color.r,
            particle.color.g,
            particle.color.b,
            particle.alpha
        );
        
        // 设置点大小
        glPointSize(particle.size);
        
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
    glDisable(GL_BLEND);
    
    glPopAttrib();
}

void Water::spawnBubble() {
    // 调试输出
    qDebug() << "\n=== Spawning Bubble ===";
    qDebug() << "Current bubble count:" << bubbles.size();
    qDebug() << "MAX_BUBBLES:" << MAX_BUBBLES;
    
    // 移除大小检查，确保可以生成气泡
    Bubble bubble;
    
    // 在底部区域随机生成，缩小生成范围
    float radius = (rand() / float(RAND_MAX)) * size * 0.4f;  // 减小到40%的尺寸
    float angle = (rand() / float(RAND_MAX)) * glm::two_pi<float>();
    
    // 设置初始位置在底部，但不要太深
    bubble.position = glm::vec3(
        radius * cos(angle),                // X坐标
        -size * 0.3f,                      // Y坐标（靠近底部但不要太深）
        radius * sin(angle)                // Z坐标
    );
    
    // 增大气泡参数
    bubble.size = 30.0f + (rand() / float(RAND_MAX)) * 20.0f;  // 显著增大气泡尺寸
    bubble.speed = 50.0f + (rand() / float(RAND_MAX)) * 30.0f;  // 增加速度
    bubble.wobble = 0.5f + (rand() / float(RAND_MAX)) * 0.5f;   // 减小摆动幅度
    bubble.phase = rand() / float(RAND_MAX) * glm::two_pi<float>();
    bubble.alpha = 1.0f;  // 设置为完全不透明
    bubble.rotationSpeed = (rand() / float(RAND_MAX) - 0.5f) * 1.0f;
    bubble.rotation = rand() / float(RAND_MAX) * glm::two_pi<float>();
    
    bubbles.push_back(bubble);
    
    // 调试输出新生成的气泡信息
    qDebug() << "New bubble generated:";
    qDebug() << "- Position:" << bubble.position.x << bubble.position.y << bubble.position.z;
    qDebug() << "- Size:" << bubble.size;
    qDebug() << "- Speed:" << bubble.speed;
    qDebug() << "- Alpha:" << bubble.alpha;
    qDebug() << "New bubble count:" << bubbles.size();
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
    
    // 计算雾度和颜色
    float baseDensity = 0.0003f;  // 降低��础雾密度
    float depthFactor = std::max(0.0f, -cameraPos.y / size);
    float currentDensity = baseDensity * (1.0f + depthFactor * 0.5f);  // 减小深度影响
    
    // 根据深度混合雾气颜色��使用更柔和的颜色
    glm::vec3 currentFogColor = glm::mix(
        waterParams.shallowColor * 1.2f,  // 稍微提亮浅水颜色
        waterParams.deepColor,
        std::min(0.7f, depthFactor)  // 限制最大深度效果
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
    glFogf(GL_FOG_START, size * 0.1f);  // 延迟雾开始距离
    glFogf(GL_FOG_END, size * 3.0f);    // 增加雾气结距离
    
    // 增强环境光以改善可见度
    GLfloat ambient[] = {
        currentFogColor.r * 0.8f,  // 增加环境光强度
        currentFogColor.g * 0.8f,
        currentFogColor.b * 0.8f,
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
    const int texSize = 64;  // 增大纹理尺寸
    std::vector<unsigned char> texData(texSize * texSize * 4);
    
    for(int y = 0; y < texSize; ++y) {
        for(int x = 0; x < texSize; ++x) {
            float dx = (x - texSize/2.0f) / (texSize/2.0f);
            float dy = (y - texSize/2.0f) / (texSize/2.0f);
            float dist = std::sqrt(dx*dx + dy*dy);
            
            // 创建更明显的粒子效果
            float alpha = std::max(0.0f, 1.0f - dist);
            alpha = std::pow(alpha, 1.2f);  // 使边缘更清晰
            
            // 添加发光效果
            float glow = std::max(0.0f, 1.0f - dist * 1.5f);
            glow = std::pow(glow, 2.0f);
            
            int idx = (y * texSize + x) * 4;
            texData[idx + 0] = static_cast<unsigned char>((0.8f + glow * 0.2f) * 255);  // R
            texData[idx + 1] = static_cast<unsigned char>((0.9f + glow * 0.1f) * 255);  // G
            texData[idx + 2] = static_cast<unsigned char>(255);  // B
            texData[idx + 3] = static_cast<unsigned char>(alpha * 255);
        }
    }
    
    glGenTextures(1, &waterParticleTexture);
    glBindTexture(GL_TEXTURE_2D, waterParticleTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
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
    
    // 增大粒子尺寸
    particle.size = 10.0f + (rand() / float(RAND_MAX)) * 20.0f;
    
    // 增加初始透明度
    particle.alpha = 0.6f;
    particle.targetAlpha = 0.8f + (rand() / float(RAND_MAX)) * 0.2f;
    
    // 设置粒子颜色为亮蓝色
    particle.color = glm::vec3(0.4f, 0.6f, 1.0f);
    
    // 增加生命周期
    particle.life = 2.0f + (rand() / float(RAND_MAX)) * 2.0f;
    particle.fadeState = 0.0f;
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
        
        // 淡入淡出逻辑
        const float FADE_TIME = 0.5f;
        if (particle.life > FADE_TIME) {
            // 淡入阶段
            if (particle.fadeState < 1.0f) {
                particle.fadeState += deltaTime / FADE_TIME;
                particle.alpha = particle.targetAlpha * particle.fadeState;
            }
        } else {
            // 淡出阶段
            particle.alpha = (particle.life / FADE_TIME) * particle.targetAlpha;
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
    // 简化更新逻辑，只保留基本的运动
    bubble.position.y += bubble.speed * deltaTime;
    
    // 添加简单的水平运动
    bubble.phase += deltaTime;
    float wobbleAmount = 5.0f;  // 增大摆动幅度
    bubble.position.x += sin(bubble.phase) * wobbleAmount * deltaTime;
    bubble.position.z += cos(bubble.phase) * wobbleAmount * deltaTime;
    
    // 保持大小和透明度恒定
    bubble.alpha = 1.0f;  // 保持完全不透明
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
    
    // 检查深度测试状态
    GLint depthTestEnabled;
    glGetIntegerv(GL_DEPTH_TEST, &depthTestEnabled);
    qDebug() << "Depth test enabled:" << (depthTestEnabled == GL_TRUE);
    
    // 检查深度写入状态
    GLint depthMask;
    glGetIntegerv(GL_DEPTH_WRITEMASK, &depthMask);
    qDebug() << "Depth mask enabled:" << (depthMask == GL_TRUE);
    
    // 检查当前绑定的纹理
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