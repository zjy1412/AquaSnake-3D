#include "water.h"
#include <QDebug>

// 修改水体顶点着色器，增加波浪和扭曲效果
const char* Water::waterVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;
    
    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;
    uniform float time;
    uniform float waveHeight;
    uniform float waveSpeed;
    
    out vec2 TexCoord;
    out vec3 FragPos;
    out vec3 Normal;
    out vec4 ClipSpace;
    
    void main()
    {
        vec3 pos = aPos;
        
        // 多层波浪叠加
        float wave1 = sin(pos.x * 0.05 + time * waveSpeed) * 
                     cos(pos.z * 0.05 + time * waveSpeed * 0.8) * waveHeight;
        float wave2 = sin(pos.x * 0.02 - time * waveSpeed * 1.2) * 
                     cos(pos.z * 0.02 + time * waveSpeed * 0.6) * waveHeight * 0.7;
        
        pos.y += wave1 + wave2;
        
        FragPos = vec3(model * vec4(pos, 1.0));
        
        // 计算法线
        vec3 tangentX = vec3(1.0, 
            cos(pos.x * 0.05 + time * waveSpeed) * waveHeight * 0.05,
            0.0);
        vec3 tangentZ = vec3(0.0,
            cos(pos.z * 0.05 + time * waveSpeed) * waveHeight * 0.05,
            1.0);
        Normal = normalize(cross(tangentX, tangentZ));
        
        ClipSpace = projection * view * model * vec4(pos, 1.0);
        gl_Position = ClipSpace;
        TexCoord = aTexCoord;
    }
)";

// 修改水体片段着色器，添加色散效果
const char* Water::waterFragmentShader = R"(
    #version 330 core
    out vec4 FragColor;
    
    in vec2 TexCoord;
    in vec3 FragPos;
    in vec3 Normal;
    in vec4 ClipSpace;
    
    uniform vec3 deepColor;
    uniform vec3 shallowColor;
    uniform float time;
    uniform float causticIntensity;
    uniform float distortionStrength;
    uniform float surfaceRoughness;
    uniform sampler2D causticTex;
    uniform float chromaDispersion;
    uniform float waterDensity;
    uniform float visibilityFalloff;
    uniform float causticBlend;
    uniform int causticLayerCount;
    
    struct CausticLayer {
        float scale;
        float offset;
        vec2 direction;
    };
    uniform CausticLayer causticLayers[4];  // 最大支持4层
    
    vec2 rotateUV(vec2 uv, float angle) {
        float s = sin(angle);
    uniform CausticLayer causticLayers[4];  // 最大支持4层
    
    vec2 rotateUV(vec2 uv, float angle) {
        float s = sin(angle);
        float c = cos(angle);
        mat2 rot = mat2(c, -s, s, c);
        return rot * (uv - 0.5) + 0.5;
    }
    
    float sampleCaustics(vec2 uv) {
        float caustic = 0.0;
        
        for(int i = 0; i < causticLayerCount; i++) {
            vec2 rotatedUV = rotateUV(uv * causticLayers[i].scale, 
                                    causticLayers[i].offset);
            float layer = texture(causticTex, rotatedUV + 
                                causticLayers[i].direction * time).r;
            caustic += layer * (1.0 - float(i) * 0.2);
        }
        
        return caustic * causticBlend;
    }
        float fresnel = pow(1.0 - max(dot(Normal, viewDir), 0.0), 5.0) * surfaceRoughness;
        
        // 水深度混合
        float depth = gl_FragCoord.z / gl_FragCoord.w;
        float fresnel = pow(1.0 - max(dot(Normal, viewDir), 0.0), 5.0) * surfaceRoughness;
        
        // 水深度混合
        float depth = gl_FragCoord.z / gl_FragCoord.w;
        float depthFactor = clamp(depth / 1000.0, 0.0, 1.0);
        // 扭曲UV坐标用于焦散效果
        vec2 distortedUV = TexCoord + vec2(
            sin(TexCoord.y * 10.0 + time) * distortionStrength,
            cos(TexCoord.x * 10.0 + time) * distortionStrength
        );
        
        // 焦散效果
        float caustic = sampleCaustics(distortedUV);
        waterColor += caustic * causticIntensity * (1.0 - depthFactor);
        
        // 添加色散效果
        vec2 dispersion = normalize(FragPos.xz) * chromaDispersion;
        vec3 dispersedColor;
        dispersedColor.r = texture(causticTex, distortedUV * 5.0 + dispersion + time * 0.1).r;
        dispersedColor.g = texture(causticTex, distortedUV * 5.0 + time * 0.1).r;
        dispersedColor.b = texture(causticTex, distortedUV * 5.0 - dispersion + time * 0.1).r;
        
        waterColor += dispersedColor * causticIntensity * (1.0 - depthFactor);
        
        // 添加水深度衰减
        float viewDistance = length(FragPos);
        float visibility = exp(-viewDistance * waterDensity * visibilityFalloff);
        
        // 最终颜色
        vec3 finalColor = mix(waterColor, vec3(0.8, 0.9, 1.0), fresnel * 0.6);
        finalColor = mix(deepColor, finalColor, visibility);
        
        // 透明度处理
        float alpha = mix(0.6, 0.9, depthFactor);
        alpha = mix(0.95, alpha, visibility);
        
        FragColor = vec4(finalColor, alpha);
    }
)";

Water::Water(float size) : 
    size(size), 
    waterTime(0.0f),
    cameraPos(0.0f) // 初始化相机位置
{
    initializeOpenGLFunctions();
}

Water::~Water() {
    // 清理资源...
}

void Water::init() {
    initShaders();
    createWaterSurface();
    initCausticTexture();
    initVolumetricLight();
    createBubbleTexture();  // 添加这行，确保在使用前创建气泡纹理
    initUnderwaterEffects();
    
    // 初始化一些气泡
    for(int i = 0; i < MAX_BUBBLES; ++i) {  // 改为生成所有气泡
        spawnBubble();
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
    // 创建并编译着色器
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &waterVertexShader, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &waterFragmentShader, NULL);
    glCompileShader(fragmentShader);

    // 创建着色器程序
    waterProgram = glCreateProgram();
    glAttachShader(waterProgram, vertexShader);
    glAttachShader(waterProgram, fragmentShader);
    glLinkProgram(waterProgram);

    // 清理
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Water::createWaterSurface() {
    // 创建一个简单的平面网格作为水面
    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);

    // 使用完整的 size 作为水面尺寸，不再乘以2
    float vertices[] = {
        // 位置              // 纹理坐标
        -size, 0.0f, -size,  0.0f, 0.0f,
         size, 0.0f, -size,  1.0f, 0.0f,
         size, 0.0f,  size,  1.0f, 1.0f,
        -size, 0.0f,  size,  0.0f, 1.0f
    };

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
    causticLayers.clear();
    
    // 初始化多层焦散
    for(int i = 0; i < waterParams.causticLayers; ++i) {
        CausticLayer layer;
        layer.scale = 1.0f + i * 0.5f;  // 每层缩放不同
        layer.speed = waterParams.causticSpeed * (1.0f + i * 0.2f);
        layer.offset = (float)i * 1.23f; // 错开相位
        layer.direction = glm::vec2(
            cos(glm::radians(45.0f + i * 30.0f)),
            sin(glm::radians(45.0f + i * 30.0f))
        );
        causticLayers.push_back(layer);
    }

    generateCausticTexture();
}

void Water::generateCausticTexture() {
    const int texSize = 512; // 增加纹理分辨率
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
    
    // 检查FBO是否完整
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        qDebug() << "Volumetric light FBO is not complete!";
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// 修改render函数签名以匹配声明
void Water::render(const glm::mat4& projection, const glm::mat4& view) {
    glUseProgram(waterProgram);

    // 设置uniform变量
    GLint projLoc = glGetUniformLocation(waterProgram, "projection");
    GLint viewLoc = glGetUniformLocation(waterProgram, "view");
    GLint modelLoc = glGetUniformLocation(waterProgram, "model");
    GLint timeLoc = glGetUniformLocation(waterProgram, "time");
    GLint colorLoc = glGetUniformLocation(waterProgram, "waterColor");
    GLint alphaLoc = glGetUniformLocation(waterProgram, "alpha");

    // 修���uniform设置方式
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniform1f(timeLoc, waterTime);
    glUniform3fv(colorLoc, 1, glm::value_ptr(WATER_COLOR));
    glUniform1f(alphaLoc, WATER_ALPHA);

    // 启用混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 修改深度测试
    glDepthMask(GL_FALSE);
    
    // 绘制完整的水体体积（六个面）
    float hs = size * 0.5f;
    glm::mat4 model(1.0f);
    
    // 绘制所有水面，使用正确的尺寸
    const std::vector<glm::mat4> waterFaces = {
        // 顶面
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, size * 0.5f, 0.0f)),
        // 底面
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -size * 0.5f, 0.0f)) * 
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // 前面
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, size)) * 
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // 后面
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -size)) * 
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // 左面
        glm::translate(glm::mat4(1.0f), glm::vec3(-size, 0.0f, 0.0f)) * 
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        // 右面
        glm::translate(glm::mat4(1.0f), glm::vec3(size, 0.0f, 0.0f)) * 
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f))
    };

    for (const auto& transform : waterFaces) {
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(transform));
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    // 设置水体参数
    GLint waveHeightLoc = glGetUniformLocation(waterProgram, "waveHeight");
    GLint waveSpeedLoc = glGetUniformLocation(waterProgram, "waveSpeed");
    GLint deepColorLoc = glGetUniformLocation(waterProgram, "deepColor");
    GLint shallowColorLoc = glGetUniformLocation(waterProgram, "shallowColor");
    GLint causticIntensityLoc = glGetUniformLocation(waterProgram, "causticIntensity");
    GLint distortionStrengthLoc = glGetUniformLocation(waterProgram, "distortionStrength");
    GLint surfaceRoughnessLoc = glGetUniformLocation(waterProgram, "surfaceRoughness");
    
    // 修改 glUniform 调用，使用正确的函数签名
    if (waveHeightLoc >= 0) {
        QOpenGLFunctions::glUniform1f(waveHeightLoc, waterParams.waveHeight);
    }
    if (waveSpeedLoc >= 0) {
        QOpenGLFunctions::glUniform1f(waveSpeedLoc, waterParams.waveSpeed);
    }
    
    // 对于向量类型，需要使用正确的函数签名和指针类型
    if (deepColorLoc >= 0) {
        QOpenGLFunctions::glUniform3fv(deepColorLoc, 1, glm::value_ptr(waterParams.deepColor));
    }
    if (shallowColorLoc >= 0) {
        QOpenGLFunctions::glUniform3fv(shallowColorLoc, 1, glm::value_ptr(waterParams.shallowColor));
    }
    
    if (causticIntensityLoc >= 0) {
        QOpenGLFunctions::glUniform1f(causticIntensityLoc, waterParams.causticIntensity);
    }
    if (distortionStrengthLoc >= 0) {
        QOpenGLFunctions::glUniform1f(distortionStrengthLoc, waterParams.distortionStrength);
    }
    if (surfaceRoughnessLoc >= 0) {
        QOpenGLFunctions::glUniform1f(surfaceRoughnessLoc, waterParams.surfaceRoughness);
    }
    
    // 绑定焦散纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, causticTexture);
    glUniform1i(glGetUniformLocation(waterProgram, "causticTex"), 0);

    // 添加体积光和气泡效果
    GLint volumetricLightLoc = glGetUniformLocation(waterProgram, "volumetricLightTex");
    GLint bubbleDensityLoc = glGetUniformLocation(waterProgram, "bubbleDensity");
    
    glUniform1i(volumetricLightLoc, 1);  // 使用纹理单元1
    glUniform1f(bubbleDensityLoc, waterParams.bubbleDensity);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, volumetricLightTexture);

    // 恢复OpenGL状态
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUseProgram(0);
    
    // 在水体渲染后渲染气泡
    renderBubbles();
    renderUnderwaterEffects(projection, view);
}

void Water::renderUnderwaterEffects(const glm::mat4& projection, const glm::mat4& view) {
    if(cameraPos.y < 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        
        float depth = -cameraPos.y;
        float depthFactor = depth / size;
        
        // 增强水下光束效果
        float godrayIntensity = waterParams.underwaterGodrayIntensity * 
                               exp(-depth * waterParams.underwaterScatteringDensity * 0.5f);
        
        // 渲染更多的水下粒子
        glBindTexture(GL_TEXTURE_2D, underwaterParticleTexture);
        glPointSize(5.0f);  // 增大粒子尺寸
        
        glBegin(GL_POINTS);
        for(const auto& particle : underwaterParticles) {
            float distToCamera = glm::length(particle.position - cameraPos);
            float visibility = exp(-distToCamera * waterParams.underwaterScatteringDensity);
            
            // 增强粒子颜色和透明度
            glColor4f(
                waterParams.underwaterColor.r + 0.2f,
                waterParams.underwaterColor.g + 0.2f,
                waterParams.underwaterColor.b + 0.2f,
                particle.life * visibility * 0.5f  // 增加透明度
            );
            
            glVertex3fv(glm::value_ptr(particle.position));
        }
        glEnd();
        
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void Water::update(float deltaTime) {
    waterTime += waterParams.waveSpeed * deltaTime;
    
    // 更新所有气泡
    for(auto it = bubbles.begin(); it != bubbles.end();) {
        updateBubble(*it, deltaTime);
        
        // 气泡到达顶部时重置到底部
        if(it->position.y > size * 0.45f) {
            // 重置气泡位置到底部随机位置
            float spawnRange = size * 0.95f;
            it->position = glm::vec3(
                (float(rand())/RAND_MAX * 2.0f - 1.0f) * spawnRange,
                -size * 0.45f,
                (float(rand())/RAND_MAX * 2.0f - 1.0f) * spawnRange
            );
            // 重置运动参数
            it->speed = waterParams.bubbleSpeed * (0.8f + float(rand())/RAND_MAX * 0.4f);
            it->phase = float(rand())/RAND_MAX * glm::two_pi<float>();
        }
        ++it;
    }
    
    // 确保气泡数量
    while(bubbles.size() < MAX_BUBBLES) {
        spawnBubble();
    }
    updateCausticAnimation(deltaTime);
    updateUnderwaterParticles(deltaTime);
    
    // 确保在每帧更新水下效果参数
    glUseProgram(waterProgram);
    
    // 更新相机位置
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
    
    glUseProgram(0);
}

void Water::updateUnderwaterParticles(float deltaTime) {
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
    
    // 给予缓慢的随机运动
    particle.velocity = glm::vec3(
        (float(rand()) / RAND_MAX - 0.5f) * 2.0f,
        (float(rand()) / RAND_MAX - 0.3f) * 1.0f,
        (float(rand()) / RAND_MAX - 0.5f) * 2.0f
    ) * 10.0f;
    
    particle.size = 2.0f + float(rand()) / RAND_MAX * 4.0f;
    particle.life = 0.5f + float(rand()) / RAND_MAX * 0.5f;
}

int Water::width() const {
    return 1024; // 默认值，可以根据需要调整
}

int Water::height() const {
    return 768;  // 默认值，可以根据需要调整
}

void Water::initWaterNormalTexture() {
    const int texSize = 256;
    std::vector<unsigned char> texData(texSize * texSize * 3);  // RGB格式
    
    // 生成Perlin噪声基础的水面法线纹理
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
            
            // 将法线转换到[0,1]范围
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
    const int texSize = 64;  // 纹理尺寸
    std::vector<unsigned char> texData(texSize * texSize * 4);  // RGBA��式
    
    float center = texSize / 2.0f;
    float maxDist = center * 0.9f;
    
    for(int y = 0; y < texSize; ++y) {
        for(int x = 0; x < texSize; ++x) {
            float dx = (x - center) / center;
            float dy = (y - center) / center;
            float dist = sqrt(dx * dx + dy * dy);
            
            // 计算气泡效果
            float alpha;
            if(dist < 0.9f) {
                // 创建一个软边缘的圆形
                alpha = dist < 0.7f ? 0.7f : (0.9f - dist) / 0.2f;
                
                // 添加边缘高光
                float edgeHighlight = std::max(0.0f, 1.0f - abs(dist - 0.8f) * 10.0f);
                float highlight = std::max(0.0f, 1.0f - (dist * 2.0f));
                
                int index = (y * texSize + x) * 4;
                // 内部颜色
                texData[index + 0] = static_cast<unsigned char>((0.8f + highlight * 0.2f) * 255);  // R
                texData[index + 1] = static_cast<unsigned char>((0.9f + highlight * 0.1f) * 255);  // G
                texData[index + 2] = static_cast<unsigned char>((1.0f) * 255);                     // B
                texData[index + 3] = static_cast<unsigned char>(alpha * (0.6f + edgeHighlight * 0.4f) * 255); // A
            } else {
                // 完全透明的部分
                int index = (y * texSize + x) * 4;
                texData[index + 0] = 0;
                texData[index + 1] = 0;
                texData[index + 2] = 0;
                texData[index + 3] = 0;
            }
        }
    }
    
    // 创建并设置纹理
    glGenTextures(1, &bubbleTexture);
    glBindTexture(GL_TEXTURE_2D, bubbleTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
    
    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Water::updateCaustics()
{
    causticTime += 0.016f;
    // TODO: 更新焦散效果
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
}

void Water::updateBubbles()
{
    // 更新现有气泡
    for(auto& pos : bubblePositions) {
        pos.y += waterParams.bubbleSpeed;
        
        if(pos.y > size) {
            pos.y = -size;
            pos.x = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * size;
            pos.z = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * size;
        }
    }
    
    // 维持气泡数量
    while(bubblePositions.size() < 100) {
        glm::vec3 newBubble(
            (float(rand()) / RAND_MAX * 2.0f - 1.0f) * size,
            -size,
            (float(rand()) / RAND_MAX * 2.0f - 1.0f) * size
        );
        bubblePositions.push_back(newBubble);
    }
}

void Water::renderBubbles() {
    if(bubbles.empty()) return;

    // 保存当前矩阵状态
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    
    // 启用所需的OpenGL特性
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, bubbleTexture);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    // 设置点大小范围
    glPointSize(MAX_BUBBLE_SIZE);
    glPointParameterf(GL_POINT_SIZE_MIN, MIN_BUBBLE_SIZE);
    glPointParameterf(GL_POINT_SIZE_MAX, MAX_BUBBLE_SIZE * 2.0f);
    
    // 开始渲染气泡
    glBegin(GL_POINTS);
    for(const auto& bubble : bubbles) {
        float distanceToCamera = glm::length(cameraPos - bubble.position);
        float fadeDistance = size * 0.8f;
        float fadeFactor = glm::clamp(1.0f - distanceToCamera / fadeDistance, 0.1f, 1.0f);
        
        // 计算气泡颜色
        float depthFactor = (bubble.position.y + size) / (size * 2.0f);
        glm::vec3 bubbleColor = glm::mix(
            glm::vec3(0.7f, 0.85f, 1.0f),  // 深水中的颜色
            glm::vec3(0.9f, 0.95f, 1.0f),  // 浅水中的颜色
            depthFactor
        );
        
        // 设置颜色和透明度
        glColor4f(
            bubbleColor.r,
            bubbleColor.g,
            bubbleColor.b,
            bubble.alpha * fadeFactor
        );
        
        // 设置点大小
        glPointSize(bubble.size);
        
        // 渲染气泡
        glVertex3fv(glm::value_ptr(bubble.position));
    }
    glEnd();
    
    // 恢复OpenGL状态
    glDisable(GL_POINT_SPRITE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    
    // 恢复矩阵状态
    glPopMatrix();
}

void Water::renderVolumetricLight()
{
    // TODO: 实现体积光渲染
}

void Water::spawnBubble() {
    Bubble bubble;
    
    // 在水体空间内随机生成
    float spawnRange = size * 0.95f;
    bubble.position = glm::vec3(
        (float(rand())/RAND_MAX * 2.0f - 1.0f) * spawnRange,
        -size + float(rand())/RAND_MAX * size * 2.0f,  // 在整个高度范围内生���
        (float(rand())/RAND_MAX * 2.0f - 1.0f) * spawnRange
    );
    
    // 增加运动参数的变化范围
    bubble.size = MIN_BUBBLE_SIZE + float(rand())/RAND_MAX * (MAX_BUBBLE_SIZE - MIN_BUBBLE_SIZE);
    bubble.speed = waterParams.bubbleSpeed * (0.8f + float(rand())/RAND_MAX * 0.4f);  // 基础速度
    bubble.wobble = (0.5f + float(rand())/RAND_MAX * 0.5f) * size / 1000.0f; // 增加摆动幅度
    bubble.phase = float(rand())/RAND_MAX * glm::two_pi<float>();
    bubble.alpha = BUBBLE_BASE_ALPHA * (0.7f + float(rand())/RAND_MAX * 0.3f);
    
    bubbles.push_back(bubble);
}

void Water::updateBubble(Bubble& bubble, float deltaTime) {
    // 基础垂直运动
    bubble.position.y += bubble.speed * deltaTime * 100.0f;  // 增加基础速度
    
    // 添加螺旋上升效果
    float spiralTime = waterTime * 2.0f + bubble.phase;
    float spiralRadius = bubble.size * 0.2f;
    bubble.position.x += sin(spiralTime) * spiralRadius * deltaTime;
    bubble.position.z += cos(spiralTime) * spiralRadius * deltaTime;
    
    // 添加横向漂移
    glm::vec2 drift = glm::vec2(
        sin(waterTime + bubble.phase),
        cos(waterTime * 0.7f + bubble.phase)
    ) * bubble.wobble * deltaTime * 50.0f;  // 增加漂移幅度
    
    bubble.position.x += drift.x;
    bubble.position.z += drift.y;
    
    // 更新相位
    bubble.phase += deltaTime * bubble.speed * 0.5f;
    
    // 根据高度调整速度
    float heightFactor = (bubble.position.y + size) / (size * 2.0f);
    bubble.speed = glm::mix(bubble.speed, 
                           bubble.speed * (1.0f + heightFactor * 0.5f), 
                           deltaTime);
}