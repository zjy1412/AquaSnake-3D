#define GLM_ENABLE_EXPERIMENTAL
#include "gamewidget.h"
#include <QKeyEvent>
#include <chrono> 
#include <glm/gtc/type_ptr.hpp>
#include <GL/glew.h>
#include "food.h"
#include <QDebug>
#include <QTime> 

static constexpr int INVINCIBLE_FRAMES_AFTER_FOOD = 20;    // 吃到食物后的无敌帧数
static constexpr float FOOD_COLLISION_MULTIPLIER = 2.5f;   // 食物碰撞范围倍数
static constexpr float OBSTACLE_COLLISION_MULTIPLIER = 0.7f; // 障碍物碰撞范围倍数

GameWidget::GameWidget(QWidget *parent) 
    : QOpenGLWidget(parent)
    , water(nullptr)  // 初始化水体指针
    , deltaTime(0.016f)  // 初始化时间步长(假设60fps)
    , gameTimer(nullptr)
    , rotationAngle(0.0f)
    , cameraDistance(DEFAULT_CAMERA_DISTANCE)    // 减小相机距离
    , cameraHeight(DEFAULT_CAMERA_HEIGHT)        // 降低相机高度
    , cameraAngle(CAMERA_DEFAULT_ANGLE)
    , snake(nullptr)
    , cameraPos(0.0f, DEFAULT_CAMERA_HEIGHT, DEFAULT_CAMERA_DISTANCE) // 调整初始相机位置
    , cameraTarget(0.0f, 0.0f, 0.0f)  // 看向原点
    , projectionMatrix(1.0f)
    , viewMatrix(1.0f)
    , aquariumSize(AQUARIUM_DEFAULT_SIZE)  // 确保这个在使用前初始化
    , gameOver(false)
    , waterLevel(0.0f)
    , waterShader(0)
    , gameState(GameState::READY)  // 改为 READY 状态
    , score(0)
    , invincibleFrames(0)  // 初始化无敌帧计数
    , currentHeight(CAMERA_SETTINGS.minHeight)
    , targetHeight(CAMERA_SETTINGS.minHeight)
    , currentFOV(CAMERA_SETTINGS.baseFOV)
    , targetFOV(CAMERA_SETTINGS.baseFOV)
    , currentCameraRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
    , targetCameraRotation(currentCameraRotation)
    , rotationSmoothFactor(CAMERA_SETTINGS.rotationSmoothing)
    , causticTexture(0)
    , volumetricLightFBO(0)
    , volumetricLightTexture(0)
    , waterNormalTexture(0)
    , bubbleTexture(0)
    , causticTime(0.0f)
    , bubblePositions()  // 初始化气泡位置数组
{
    // 初始化定时器
    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, [this]() {
        if(gameState == GameState::PLAYING) {
            updateGame();
            updateCamera();
        }
        update();
    });
    
    // 确保边界已经设置好后再创建蛇
    if(aquariumSize <= 0) {
        aquariumSize = AQUARIUM_DEFAULT_SIZE;
    }
    
    // 创建蛇，位置在水族箱左侧安全区域
    float startX = -aquariumSize * 0.4f;  // 从水族箱40%处开始
    snake = new Snake(startX, 0.0f, 0.0f);
    snake->setDirection(glm::vec3(1.0f, 0.0f, 0.0f));

    // 设置相机位置
    cameraTarget = snake->getHeadPosition();
    cameraPos = cameraTarget + glm::vec3(0.0f, 15.0f, 15.0f);

    // 生成食物
    spawnFood();
    
    // 设置游戏状态
    gameState = GameState::PLAYING;
    
    // 启动定时器
    gameTimer->start(16);

    // 调试输出
    glm::vec3 initialPos = snake->getHeadPosition();
    qDebug() << "Initial setup -"
             << "\nAquariumSize:" << aquariumSize
             << "\nMargin:" << (aquariumSize * 0.1f)
             << "\nSnake position:" << initialPos.x << initialPos.y << initialPos.z
             << "\nIn bounds:" << isInAquarium(initialPos);
}

GameWidget::~GameWidget()
{
    makeCurrent();
    
    delete water;  
    
    // 清理纹理和FBO
    if(causticTexture) glDeleteTextures(1, &causticTexture);
    if(volumetricLightTexture) glDeleteTextures(1, &volumetricLightTexture);
    if(waterNormalTexture) glDeleteTextures(1, &waterNormalTexture);
    if(bubbleTexture) glDeleteTextures(1, &bubbleTexture);
    if(volumetricLightFBO) glDeleteFramebuffers(1, &volumetricLightFBO);
    
    delete snake;
    doneCurrent();
}

// 简化初始化，确保能看到场景
void GameWidget::initializeGL()
{
    // 首先���始化OpenGL函数
    initializeOpenGLFunctions();
    glewInit();

    // 然后初始化蛇的OpenGL函数
    if(snake) {
        snake->initializeGL();
    }
    
    // 初始化水体的OpenGL函数
    if(water) {
        water->initializeGL();
    }
    
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
    
    // 设置全局光照模型
    glEnable(GL_LIGHTING);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    
    GLfloat globalAmbient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    // 初始化各个组件
    initObstacles();
    spawnFood();
    
    // 设置初始相机位置
    cameraPos = glm::vec3(0.0f, 25.0f, 35.0f);
    cameraTarget = glm::vec3(0.0f);
    viewMatrix = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

    // 初始化水体
    water = new Water(aquariumSize);
    water->init();
    
    // 初始化光源系统
    initLights();
    
    // 检查水体初始化状态
    if (!water) {
        qDebug() << "Warning: Water not initialized";
        return;
    }
    
    // 验证水体纹理是否创建成功
    GLuint waterTextures[] = {
        water->getCausticTexture(),
        water->getVolumetricLightTexture(),
        water->getWaterNormalTexture(),
        water->getBubbleTexture()
    };
    
    for(GLuint tex : waterTextures) {
        if(tex == 0) {
            qDebug() << "Warning: Water texture not created properly";
        }
    }
    
    qDebug() << "=== OpenGL Initialization ===";
    qDebug() << "Vendor:" << (const char*)glGetString(GL_VENDOR);
    qDebug() << "Renderer:" << (const char*)glGetString(GL_RENDERER);
    qDebug() << "Version:" << (const char*)glGetString(GL_VERSION);
    
    GLint maxLights;
    glGetIntegerv(GL_MAX_LIGHTS, &maxLights);
    qDebug() << "Maximum lights supported:" << maxLights;
    
    // 验证光照初始化
    GLboolean lightingEnabled;
    glGetBooleanv(GL_LIGHTING, &lightingEnabled);
    qDebug() << "Initial GL_LIGHTING state:" << (lightingEnabled ? "true" : "false");
}

void GameWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    float aspect = float(w) / float(h);
    
    // 修改投影矩阵参数，增加远平面距离
    projectionMatrix = glm::perspective(
        glm::radians(90.0f),  // 增大FOV角度
        aspect,
        1.0f,                 // 减小近平面距离
        10000.0f              // 大幅增加远平面距离
    );
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glLoadMatrixf(glm::value_ptr(projectionMatrix));
}

// 打印蛇头位置和相机位置（用于调试）
void GameWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 确保基本状态设置正确
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 更新光照
    updateLights();
    
    // 设置变换矩阵
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projectionMatrix));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(viewMatrix));
    
    // 1. 绘制水族箱
    drawAquarium();
    
    // 2. 绘制场景对象（蛇、食物、障碍物）
    drawSceneObjects();
    
    // 3. 如果水体存在，渲染水体效果
    if(water) {
        // 保存当前状态
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        
        // 渲染水体
        water->render(projectionMatrix, viewMatrix);
        
        // 恢复状态
        glPopAttrib();
    }
    
    // 确保所有渲染完成
    glFlush();
}

// 统一管理场景对象的绘制
void GameWidget::drawSceneObjects()
{
    // 保存当前状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    
    // 重新确保光照启用
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    
    // 绘制障碍物
    for(const auto& obstacle : obstacles) {
        // 设置物体材质
        glColor4f(0.6f, 0.6f, 0.6f, 1.0f);  // 基础颜色
        GLfloat matSpecular[] = { 0.8f, 0.8f, 0.8f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
        
        obstacle.draw();
    }

    // 绘制食物
    for(const auto& food : foods) {
        // 设置物材质
        glColor4f(1.0f, 0.5f, 0.0f, 1.0f);  // 橙色基础颜色
        GLfloat foodSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, foodSpecular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 64.0f);
        
        food.draw();
    }
    
    // 绘制蛇
    if(snake) {
        // 设置蛇的材质
        glColor4f(0.2f, 0.8f, 0.2f, 1.0f);  // 绿色基础颜色
        GLfloat snakeSpecular[] = { 0.6f, 0.8f, 0.6f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, snakeSpecular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 48.0f);
        
        snake->draw();
    }
    
    // 恢复状态
    glPopMatrix();
    glPopAttrib();
}

void GameWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_R) {
        qDebug() << "Resetting game via R key";
        resetGame();
        return;
    }

    if(gameState != GameState::PLAYING || !snake) return;

    const float ROTATION_ANGLE = glm::radians(90.0f);
    glm::vec3 currentDir = snake->getDirection();
    glm::vec3 currentUp = snake->getUpDirection();
    glm::vec3 currentRight = snake->getRightDirection();

    switch(event->key()) {
        case Qt::Key_W:
            snake->rotateAroundAxis(currentRight, ROTATION_ANGLE);
            break;
        case Qt::Key_S:
            snake->rotateAroundAxis(currentRight, -ROTATION_ANGLE);
            break;
        case Qt::Key_A:
            snake->rotateAroundAxis(currentUp, ROTATION_ANGLE);
            break;
        case Qt::Key_D:
            snake->rotateAroundAxis(currentUp, -ROTATION_ANGLE);
            break;
    }
}

void GameWidget::updateGame()
{
    if(gameState != GameState::PLAYING || !snake) return;

    // 预判下一个位置
    glm::vec3 nextPos = snake->getHeadPosition() + 
                        glm::normalize(snake->getDirection()) * 
                        snake->getMovementSpeed();
    
    // 如果下一个位置会出界，停止移动
    if(!isInAquarium(nextPos)) {
        qDebug() << "Prevented out-of-bounds movement at:" 
                 << nextPos.x << nextPos.y << nextPos.z;
        return;
    }

    // 移动蛇
    snake->move();
    
    // 修改食物检测逻辑，使用更大的判定范围
    bool foodEaten = false;
    std::vector<size_t> foodToRemove;
    
    // 先检查所有需要移除的食物
    for(size_t i = 0; i < foods.size(); ++i) {
        float collisionDistance = snake->getSegmentSize() * FOOD_COLLISION_MULTIPLIER;
        float distance = glm::distance(snake->getHeadPosition(), foods[i].getPosition());
        
        // 使判定范围更大
        if (distance < collisionDistance) {
            foodToRemove.push_back(i);
            foodEaten = true;
        }
    }
    
    // 如果吃到了食物
    if(foodEaten) {
        score += 10 * foodToRemove.size();
        
        // 从后向前移除食物
        for(auto it = foodToRemove.rbegin(); it != foodToRemove.rend(); ++it) {
            if(*it < foods.size()) {
                foods.erase(foods.begin() + *it);
            }
        }
        
        // 让蛇生长
        for(size_t i = 0; i < foodToRemove.size() * Snake::GROWTH_FACTOR; ++i) {
            snake->grow();
        }
        
        // 设置无敌帧
        invincibleFrames = INVINCIBLE_FRAMES_AFTER_FOOD;
        
        // 生成新的食物
        spawnFood();
    }
    
    // 处理无敌帧
    if(invincibleFrames > 0) {
        invincibleFrames--;
    } else {
        checkCollisions();
    }
    
    // 更新水体，使用正确的deltaTime
    if (water) {
        water->update(deltaTime);
    }
    
    update();
}

void GameWidget::updateCamera()
{
    if (!snake) return;

    glm::vec3 snakeHead = snake->getHeadPosition();
    glm::vec3 snakeDir = snake->getDirection();
    glm::vec3 snakeUp = snake->getUpDirection();
    glm::vec3 snakeRight = snake->getRightDirection();

    // 计算目标相机旋转
    glm::vec3 idealLookDir = -snakeDir;  // 相机应该看向蛇头的反方向
    glm::vec3 idealUp = snakeUp;
    
    // 构建目标旋转矩阵
    glm::mat3 targetRotationMat = glm::mat3(
        glm::cross(idealLookDir, idealUp),  // right
        idealUp,                            // up
        -idealLookDir                       // forward
    );
    
    // 转换为四元数
    targetCameraRotation = glm::quat_cast(targetRotationMat);
    
    // 平滑插值当前旋转到目标旋转
    currentCameraRotation = glm::slerp(
        currentCameraRotation,
        targetCameraRotation,
        rotationSmoothFactor
    );
    
    // 应用旋转获取相机位置
    glm::mat4 rotationMatrix = glm::mat4_cast(currentCameraRotation);
    glm::vec3 baseOffset = glm::vec3(
        0.0f,
        CAMERA_SETTINGS.minHeight * 0.5f,  // 减小高度偏移
        -CAMERA_SETTINGS.distance * 0.4f    // 减小距离到原来的0.4倍
    );
    
    // 计算相机位置和观察点
    glm::vec3 rotatedOffset = glm::vec3(rotationMatrix * glm::vec4(baseOffset, 1.0f));
    glm::vec3 idealCameraPos = snakeHead + rotatedOffset;
    
    // 添加侧向偏移
    glm::vec3 sideOffset = snakeRight * (CAMERA_SETTINGS.distance * SIDE_OFFSET_FACTOR);
    idealCameraPos += sideOffset;
    
    // 平滑过渡到新的相机位置
    cameraPos = glm::mix(cameraPos, idealCameraPos, CAMERA_SETTINGS.smoothFactor);
    
    // 计算并平滑过渡到新的观察点
    glm::vec3 idealLookAtPoint = snakeHead 
        + snakeDir * (FORWARD_OFFSET * 0.5f)  // 减小前方观察点偏移到原来的0.5倍
        + snakeUp * (CAMERA_SETTINGS.minHeight * 0.1f);  // 减小上方偏移
    
    cameraTarget = glm::mix(cameraTarget, idealLookAtPoint, CAMERA_SETTINGS.smoothFactor);

    // 构建视图矩阵
    viewMatrix = glm::lookAt(
        cameraPos,
        cameraTarget,
        glm::vec3(glm::mat4_cast(currentCameraRotation) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f))
    );

    // 修改投影矩阵
    float aspect = width() / static_cast<float>(height());
    projectionMatrix = glm::perspective(
        glm::radians(currentFOV),
        aspect,
        1.0f,                // 减小近平面距离
        10000.0f             // 大幅增加远平面距离
    );
}

void GameWidget::spawnFood()
{
    // 每次生成多个食物
    int foodToSpawn = MIN_FOOD_COUNT - foods.size();
    for (int i = 0; i < foodToSpawn; ++i) {
        int maxAttempts = 100;
        int attempts = 0;
        glm::vec3 newFoodPos;
        bool validPosition = false;
        
        do {
            float range = aquariumSize * 0.8f;
            newFoodPos = glm::vec3(
                (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range,
                ((float(rand()) / RAND_MAX) - 0.5f) * range * 0.5f,
                (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range
            );
            
            // 检查与其他食物的距离
            bool tooClose = false;
            for (const auto& existingFood : foods) {
                if (glm::distance(newFoodPos, existingFood.getPosition()) < MIN_FOOD_DISTANCE) {
                    tooClose = true;
                    break;
                }
            }
            
            validPosition = !tooClose && isInAquarium(newFoodPos);
            
            // 检查与障碍物的碰撞
            for(const auto& obstacle : obstacles) {
                if(obstacle.checkCollision(newFoodPos)) {
                    validPosition = false;
                    break;
                }
            }
            
        } while (!validPosition && ++attempts < maxAttempts);

        if (validPosition) {
            foods.emplace_back(newFoodPos);
        }
    }
}

bool GameWidget::isValidFoodPosition(const glm::vec3& pos) const
{
    // 检查是否在水族箱内
    if(!isInAquarium(pos)) return false;
    
    // 检查是否与障碍物重叠
    for(const auto& obstacle : obstacles) {
        if(obstacle.checkCollision(pos)) {
            return false;
        }
    }
    
    // 检查是否与蛇重叠
    if(snake->checkCollision(pos)) {
        return false;
    }
    
    return true;
}

bool GameWidget::isInAquarium(const glm::vec3& pos) const
{
    float margin = aquariumSize * 0.1f;  // 10%的边界预留量
    float limit = aquariumSize - margin;
    float heightLimit = limit * 0.5f;    // 高度限制为一半
    
    bool inX = pos.x >= -limit && pos.x <= limit;
    bool inY = pos.y >= -heightLimit && pos.y <= heightLimit;
    bool inZ = pos.z >= -limit && pos.z <= limit;

    return inX && inY && inZ;
}

void GameWidget::drawAquarium()
{
    // 绘制底面网格
    glColor3f(0.5f, 0.5f, 0.5f);  // 更亮的网格颜色
    glBegin(GL_LINES);
    float gridSize = aquariumSize / 40.0f;  // 增加网格密度
    for(float i = -aquariumSize; i <= aquariumSize; i += gridSize) {
        glVertex3f(i, -aquariumSize * 0.5f, -aquariumSize);
        glVertex3f(i, -aquariumSize * 0.5f, aquariumSize);
        glVertex3f(-aquariumSize, -aquariumSize * 0.5f, i);
        glVertex3f(aquariumSize, -aquariumSize * 0.5f, i);
    }
    glEnd();

    // 绘制不透明的边界线框
    glLineWidth(8.0f);  // 更粗的边界线
    glColor3f(0.0f, 0.7f, 1.0f);  // 更亮的蓝色边界
    
    // 绘制12条边界线
    glBegin(GL_LINES);
    float hs = aquariumSize * 0.5f;  // 高度限制为一半
    
    // 底部四条线
    glVertex3f(-aquariumSize, -hs, -aquariumSize); glVertex3f(aquariumSize, -hs, -aquariumSize);
    glVertex3f(-aquariumSize, -hs, aquariumSize); glVertex3f(aquariumSize, -hs, aquariumSize);
    glVertex3f(-aquariumSize, -hs, -aquariumSize); glVertex3f(-aquariumSize, -hs, aquariumSize);
    glVertex3f(aquariumSize, -hs, -aquariumSize); glVertex3f(aquariumSize, -hs, aquariumSize);
    
    // 顶部四条线
    glVertex3f(-aquariumSize, hs, -aquariumSize); glVertex3f(aquariumSize, hs, -aquariumSize);
    glVertex3f(-aquariumSize, hs, aquariumSize); glVertex3f(aquariumSize, hs, aquariumSize);
    glVertex3f(-aquariumSize, hs, -aquariumSize); glVertex3f(-aquariumSize, hs, aquariumSize);
    glVertex3f(aquariumSize, hs, -aquariumSize); glVertex3f(aquariumSize, hs, aquariumSize);
    
    // 竖直四条线
    glVertex3f(-aquariumSize, -hs, -aquariumSize); glVertex3f(-aquariumSize, hs, -aquariumSize);
    glVertex3f(aquariumSize, -hs, -aquariumSize); glVertex3f(aquariumSize, hs, -aquariumSize);
    glVertex3f(-aquariumSize, -hs, aquariumSize); glVertex3f(-aquariumSize, hs, aquariumSize);
    glVertex3f(aquariumSize, -hs, aquariumSize); glVertex3f(aquariumSize, hs, aquariumSize);
    glEnd();

    // 修改深度测试设置并绘制透明面
    glDepthMask(GL_FALSE);  // 禁用深度写入
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    hs = aquariumSize * 0.5f;
    
    // 判断相机是否在水族箱内
    bool cameraInside = isInAquarium(cameraPos);
    
    // 相机在外部时将透明度降到更低
    float baseAlpha = cameraInside ? 0.2f : 0.02f;  // 降低外部时的透明度
    
    // 按照距离相机的远近对面进行排序
    struct Face {
        int index;
        float distance;
        glm::vec3 normal;
        glm::vec3 center;
    };
    
    std::vector<Face> faces;
    for(int i = 0; i < 6; ++i) {
        Face face;
        face.index = i;
        
        // 设置面的法线和中心点
        switch(i) {
            case 0: // 前面 (Z+)
                face.normal = glm::vec3(0.0f, 0.0f, 1.0f);
                face.center = glm::vec3(0.0f, 0.0f, aquariumSize);
                break;
            case 1: // 后面 (Z-)
                face.normal = glm::vec3(0.0f, 0.0f, -1.0f);
                face.center = glm::vec3(0.0f, 0.0f, -aquariumSize);
                break;
            case 2: // 左面 (X-)
                face.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
                face.center = glm::vec3(-aquariumSize, 0.0f, 0.0f);
                break;
            case 3: // 右面 (X+)
                face.normal = glm::vec3(1.0f, 0.0f, 0.0f);
                face.center = glm::vec3(aquariumSize, 0.0f, 0.0f);
                break;
            case 4: // 顶面 (Y+)
                face.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                face.center = glm::vec3(0.0f, hs, 0.0f);
                break;
            case 5: // 底面 (Y-)
                face.normal = glm::vec3(0.0f, -1.0f, 0.0f);
                face.center = glm::vec3(0.0f, -hs, 0.0f);
                break;
        }
        
        face.distance = glm::length(cameraPos - face.center);
        faces.push_back(face);
    }
    
    // 从远到近排序
    std::sort(faces.begin(), faces.end(),
        [](const Face& a, const Face& b) { return a.distance > b.distance; });
    
    // 按距离从远到近绘制透明面
    glBegin(GL_QUADS);
    for(const Face& face : faces) {
        // 计算面的透明度
        float dotProduct = glm::dot(glm::normalize(cameraPos - face.center), face.normal);
        
        // 动态调整透明度
        float alpha;
        if (!cameraInside) {
            // 相机在外部时，面向相���的面完全透明
            alpha = (dotProduct < 0.0f) ? 0.01f : baseAlpha;  // 更低的透明度
        } else {
            // 相机在内部时使用正常透明度
            alpha = baseAlpha;
        }
        
        // 设置颜色和透明度
        glColor4f(0.2f, 0.4f, 0.8f, alpha);
        
        // 绘制面的顶点
        switch(face.index) {
            case 0: // 前面 (Z+)
                glVertex3f(-aquariumSize, -hs, aquariumSize);
                glVertex3f(aquariumSize, -hs, aquariumSize);
                glVertex3f(aquariumSize, hs, aquariumSize);
                glVertex3f(-aquariumSize, hs, aquariumSize);
                break;
            case 1: // 后面 (Z-)
                glVertex3f(-aquariumSize, -hs, -aquariumSize);
                glVertex3f(-aquariumSize, hs, -aquariumSize);
                glVertex3f(aquariumSize, hs, -aquariumSize);
                glVertex3f(aquariumSize, -hs, -aquariumSize);
                break;
            case 2: // 左面 (X-)
                glVertex3f(-aquariumSize, -hs, -aquariumSize);
                glVertex3f(-aquariumSize, -hs, aquariumSize);
                glVertex3f(-aquariumSize, hs, aquariumSize);
                glVertex3f(-aquariumSize, hs, -aquariumSize);
                break;
            case 3: // 右面 (X+)
                glVertex3f(aquariumSize, -hs, -aquariumSize);
                glVertex3f(aquariumSize, hs, -aquariumSize);
                glVertex3f(aquariumSize, hs, aquariumSize);
                glVertex3f(aquariumSize, -hs, aquariumSize);
                break;
            case 4: // 顶面 (Y+)
                glVertex3f(-aquariumSize, hs, -aquariumSize);
                glVertex3f(-aquariumSize, hs, aquariumSize);
                glVertex3f(aquariumSize, hs, aquariumSize);
                glVertex3f(aquariumSize, hs, -aquariumSize);
                break;
            case 5: // 底面 (Y-)
                glVertex3f(-aquariumSize, -hs, -aquariumSize);
                glVertex3f(aquariumSize, -hs, -aquariumSize);
                glVertex3f(aquariumSize, -hs, aquariumSize);
                glVertex3f(-aquariumSize, -hs, aquariumSize);
                break;
        }
    }
    glEnd();
    // 恢复OpenGL状态
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);   // 重新启用深度写入
    glLineWidth(1.0f);
}

void GameWidget::initObstacles()
{
    obstacles.clear();
    for(int i = 0; i < MAX_OBSTACLES; ++i) {
        float range = aquariumSize * 0.8f;
        float x = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range;
        float y = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range * 0.5f;
        float z = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range;
        
        // 确保障碍物不会出现在蛇的初始位置附近
        if(glm::length(glm::vec3(x, y, z) - snake->getHeadPosition()) < 100.0f) {
            continue;
        }
        
        obstacles.emplace_back(glm::vec3(x, y, z), 50.0f);  // 增大障碍物尺寸
    }
}

void GameWidget::resetGame()
{
    qDebug() << "=== GAME RESET ===";
    qDebug() << "Previous state:" << static_cast<int>(gameState);

    // 设置状态（只设置一次）
    gameState = GameState::PLAYING;
    gameOver = false;
    score = 0;
    
    qDebug() << "Game state reset to PLAYING:" << static_cast<int>(gameState);

    // 重置游戏状态
    gameState = GameState::PLAYING;
    gameOver = false;
    score = 0;

    // 删除旧的蛇并创建新的
    delete snake;
    snake = new Snake(-5.0f, 0.0f, 0.0f);
    
    // 如果OpenGL已初始化，则初始化蛇的OpenGL函数
    if (context() && context()->isValid()) {
        snake->initializeGL();
    }
    
    // 如果出界，强制移动到安全位置
    glm::vec3 newPos = snake->getHeadPosition();
    if(!isInAquarium(newPos)) {
        qDebug() << "WARNING: Reset position is out of bounds! Adjusting...";
        newPos = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    // 重置相机位置
    cameraPos = glm::vec3(0.0f, DEFAULT_CAMERA_HEIGHT, DEFAULT_CAMERA_DISTANCE);
    cameraTarget = snake->getHeadPosition();
    cameraAngle = CAMERA_DEFAULT_ANGLE;

    // 重新生成食物
    spawnFood();

    // 重新初始化障碍物
    obstacles.clear();
    initObstacles();

    qDebug() << "New snake position:" << newPos.x << newPos.y << newPos.z
             << "In bounds:" << isInAquarium(newPos);

    // 确保游戏计时器在运行
    if (!gameTimer->isActive()) {
        gameTimer->start(16);
    }

    update();
}

void GameWidget::checkCollisions()
{
    if(gameState != GameState::PLAYING || invincibleFrames > 0) return;
    
    const glm::vec3& headPos = snake->getHeadPosition();
    
    // 检查与障碍物的碰撞 - 使用渐进式判定
    for(const auto& obstacle : obstacles) {
        float distance = glm::length(headPos - obstacle.getPosition());
        float collisionRange = (snake->getSegmentSize() + obstacle.getRadius()) * OBSTACLE_COLLISION_MULTIPLIER;
        
        // 根据距离计算碰撞概率
        if(distance < collisionRange) {
            qDebug() << "Game over! Collision with obstacle at distance:" << distance;
            gameState = GameState::GAME_OVER;
            gameOver = true;
            return;
        }
    }

    // 检查与蛇身的碰撞
    if(snake->checkSelfCollision()) {
        qDebug() << "Game over! Self collision";
        gameState = GameState::GAME_OVER;
        gameOver = true;
        return;
    }
}

void GameWidget::initLights() {
    lightSources.clear();
    
    // 添加主光源
    lightSources.push_back({
        glm::vec3(0.0f, aquariumSize * 1.5f, 0.0f), // 光源位置
        glm::vec3(0.0f, -1.0f, 0.0f),              // 方向
        glm::vec3(1.0f, 0.95f, 0.9f),              // 暖色调
        5.0f,                                       // 增强强度
        aquariumSize * 2.0f,                        // 范围
        0.0001f                                     // 衰减
    });
    
    // 添加水下补光
    for(int i = 0; i < 4; ++i) {
        float angle = i * glm::pi<float>() * 0.5f;
        lightSources.push_back({
            glm::vec3(cos(angle) * aquariumSize * 0.5f, 
                     -aquariumSize * 0.3f,
                     sin(angle) * aquariumSize * 0.5f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.2f, 0.4f, 0.8f), // 蓝色调
            2.0f,
            aquariumSize,
            0.0005f
        });
    }
}

void GameWidget::applyLightSettings() {
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(glm::value_ptr(viewMatrix));
    
    glEnable(GL_LIGHTING);
    
    GLint maxLights;
    glGetIntegerv(GL_MAX_LIGHTS, &maxLights);
    size_t numLights = std::min(lightSources.size(), static_cast<size_t>(maxLights));

    // 禁用所有光源并重置状态
    for(int i = 0; i < maxLights; ++i) {
        glDisable(GL_LIGHT0 + i);
    }

    // 应用每个光源
    for(size_t i = 0; i < numLights; ++i) {
        const auto& light = lightSources[i];
        GLenum lightEnum = GL_LIGHT0 + i;

        glEnable(lightEnum);

        // 设置光源位置和方向
        GLfloat position[] = {
            light.position.x, light.position.y, light.position.z,
            (light.radius > 0.0f) ? 1.0f : 0.0f
        };
        glLightfv(lightEnum, GL_POSITION, position);

        if(light.spotCutoff > 0.0f) {
            // 配置聚光灯参数
            glLightf(lightEnum, GL_SPOT_CUTOFF, light.spotCutoff);
            glLightf(lightEnum, GL_SPOT_EXPONENT, light.spotExponent);
            GLfloat direction[] = {
                light.direction.x, light.direction.y, light.direction.z
            };
            glLightfv(lightEnum, GL_SPOT_DIRECTION, direction);
        }

        // 设置光照颜色和强度
        float intensityFactor = light.intensity;
        if(isInAquarium(cameraPos) && cameraPos.y < 0) {
            // 水下增强光照
            float depth = -cameraPos.y;
            float depthFactor = std::min(1.0f, depth / (aquariumSize * 0.5f));
            intensityFactor *= (1.0f - depthFactor * 0.3f); // 随深度轻微衰减
        }

        GLfloat ambient[] = {
            light.color.r * 0.3f * intensityFactor,
            light.color.g * 0.3f * intensityFactor,
            light.color.b * 0.3f * intensityFactor,
            1.0f
        };

        GLfloat diffuse[] = {
            light.color.r * intensityFactor,
            light.color.g * intensityFactor,
            light.color.b * intensityFactor,
            1.0f
        };

        GLfloat specular[] = {
            light.color.r * 0.8f * intensityFactor,
            light.color.g * 0.8f * intensityFactor,
            light.color.b * 0.8f * intensityFactor,
            1.0f
        };

        glLightfv(lightEnum, GL_AMBIENT, ambient);
        glLightfv(lightEnum, GL_DIFFUSE, diffuse);
        glLightfv(lightEnum, GL_SPECULAR, specular);

        if(light.radius > 0.0f) {
            // 优化衰减参数
            float dist = light.radius;
            glLightf(lightEnum, GL_CONSTANT_ATTENUATION, 1.0f);
            glLightf(lightEnum, GL_LINEAR_ATTENUATION, 0.5f / dist);
            glLightf(lightEnum, GL_QUADRATIC_ATTENUATION, 0.5f / (dist * dist));
        }
    }

    glPopMatrix();
}

void GameWidget::updateLights() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0f;
    
    // 更新光束位置和方向
    for(size_t i = 4; i < 7; ++i) {  // 更新三束光柱
        if(i >= lightSources.size()) continue;
        
        float phase = time * 0.2f + (i - 4) * glm::pi<float>() * 0.3f;
        float xOffset = sin(phase) * aquariumSize * 0.2f;
        float zOffset = cos(phase) * aquariumSize * 0.2f;
        
        // 移动光源位置
        lightSources[i].position.x = xOffset;
        lightSources[i].position.z = zOffset;
        
        // 更新光束方向，始终指向下方
        lightSources[i].direction = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f));
    }

    // 水下光照调整
    if(isInAquarium(cameraPos)) {
        float depth = std::max(0.0f, -cameraPos.y);
        float depthFactor = std::min(1.0f, depth / (aquariumSize * 0.5f));
        
        // 根据深度调整环境光
        GLfloat ambientColor[] = {
            0.3f * (1.0f - depthFactor * 0.5f),
            0.3f * (1.0f - depthFactor * 0.3f),
            0.4f * (1.0f - depthFactor * 0.2f),
            1.0f
        };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientColor);
    }
    
    // 应用更新后的光照设置
    applyLightSettings();
}

void GameWidget::updateBubbles() {
    if(water) {
        // 传入 deltaTime
        water->updateBubbles(deltaTime);
        return;
    }
}