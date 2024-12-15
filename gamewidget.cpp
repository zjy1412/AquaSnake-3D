#include "gamewidget.h"
#include <QKeyEvent>
#include <chrono>  // 使用chrono代替QTime
#include <glm/gtc/type_ptr.hpp>
#include <GL/glew.h>
#include "food.h"
#include <QDebug>
#include <QTime>  // 添加这行

// 在头文件定义常量，删除这里的类定义
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
    
    delete water;  // 删除水体对象
    
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
    initializeOpenGLFunctions();
    glewInit();
    
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);  // 更暗的背景色
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    
    // 设置光照参数
    GLfloat ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat position[] = { 0.0f, 20.0f, 0.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);

    // 初始化各个组件
    initObstacles();
    spawnFood();
    
    // 设置初始相��位置
    cameraPos = glm::vec3(0.0f, 25.0f, 35.0f);  // 拉远初始视角
    cameraTarget = glm::vec3(0.0f);
    viewMatrix = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

    // 初始化水体时使用实际的水族箱尺寸
    water = new Water(aquariumSize);
    water->init();
    
    // 禁用默认光源
    glDisable(GL_LIGHT0);
    
    // 启用更多光源
    glEnable(GL_LIGHTING);
    for(int i = 0; i < 8; ++i) {  // OpenGL支持最多8个光源
        glEnable(GL_LIGHT0 + i);
    }
    
    initLights();
    
    // 修改光照初始化
    glEnable(GL_LIGHTING);
    
    // 先禁用所有光源
    for(int i = 0; i < GL_MAX_LIGHTS; i++) {
        glDisable(GL_LIGHT0 + i);
    }
    
    // 设置全局光照模型
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    
    GLfloat globalAmbient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);
    
    // 初始化光源
    initLights();
}

void GameWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    float aspect = float(w) / float(h);
    
    // 调整近平面和远平面，确保能看到整个场景
    projectionMatrix = glm::perspective(
        glm::radians(45.0f),  // 减小FOV角度，使视野更集中
        aspect,
        0.1f,                // 近平面
        1000.0f              // 远平面
    );
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glLoadMatrixf(glm::value_ptr(projectionMatrix));
}

// 打印蛇头位置和相机位置（用于调试）
void GameWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 在渲染开始时保存OpenGL状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // 设置基础渲染状态
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projectionMatrix));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(viewMatrix));

    // 更新水下效果状态
    updateUnderwaterEffects();
    
    // 应用水下效果（如果在水中）
    if(isInAquarium(cameraPos)) {
        applyUnderwaterState();
    }

    // 绘制场景内容
    drawAquarium();
    drawSceneObjects();  // 新函数，包含所有场景对象的绘制
    
    // 渲染水体
    if(water) {
        water->setCameraPosition(cameraPos);
        water->render(projectionMatrix, viewMatrix);
    }

    // 恢复OpenGL状态
    glPopAttrib();
    glFlush();
    
    // 在绘制场景之前更新光源
    updateLights();
    
    // 确保启用正确的OpenGL状态
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
}

// 新增函数：统一管理场景对象的绘制
void GameWidget::drawSceneObjects()
{
    // 绘制参考线
    drawReferenceLines();
    
    // 绘制障碍物
    for(const auto& obstacle : obstacles) {
        obstacle.draw();
    }
    
    // 绘制蛇
    if(snake) {
        glColor3f(1.0f, 1.0f, 1.0f);
        snake->draw();
    }

    // 绘制食物
    for(const auto& food : foods) {
        food.draw();
    }
}

// 新增函数：应用水下状态
void GameWidget::applyUnderwaterState()
{
    // 启用雾效果
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    
    // 设置雾参数
    GLfloat fogColor[] = {
        underwaterEffects.fogColor.r,
        underwaterEffects.fogColor.g,
        underwaterEffects.fogColor.b,
        1.0f
    };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, underwaterEffects.fogDensity);
    
    // 设置环境光
    GLfloat ambient[] = {
        underwaterEffects.currentLight,
        underwaterEffects.currentLight * 1.1f,
        underwaterEffects.currentLight * 1.2f,
        1.0f
    };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
}

// 添加绘制参考线函数的实现
void GameWidget::drawReferenceLines()
{
    glBegin(GL_LINES);
    // X轴 - 红色
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(aquariumSize, 0.0f, 0.0f);
    // Y轴 - 绿色
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, aquariumSize, 0.0f);
    // Z轴 - 蓝色
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, aquariumSize);
    glEnd();
}

void GameWidget::keyPressEvent(QKeyEvent *event)
{
    // R键重置游戏 - 这个检查应该放在最前面，无论游戏状态如何都可以重置
    if (event->key() == Qt::Key_R) {
        qDebug() << "Resetting game via R key";
        resetGame();
        return;
    }

    if(gameState != GameState::PLAYING || !snake) return;

    const float ROTATION_ANGLE = glm::radians(90.0f);  // 90度旋转
    glm::vec3 currentDir = snake->getDirection();
    glm::vec3 currentUp = snake->getUpDirection();
    glm::vec3 currentRight = snake->getRightDirection();

    switch(event->key()) {
        case Qt::Key_W: {
            snake->rotateAroundAxis(currentRight, ROTATION_ANGLE);
            break;
        }
        case Qt::Key_S: {
            snake->rotateAroundAxis(currentRight, -ROTATION_ANGLE);
            break;
        }
        case Qt::Key_A: {
            snake->rotateAroundAxis(currentUp, ROTATION_ANGLE);
            break;
        }
        case Qt::Key_D: {
            snake->rotateAroundAxis(currentUp, -ROTATION_ANGLE);
            break;
        }
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
    
    // 首先检查所有需要移除的食物
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
        
        // 从后向前移除食物，避免索引���效
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
        // 注意：所有水体相关的更新都应该在 water 类中进行
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
        CAMERA_SETTINGS.minHeight,
        -CAMERA_SETTINGS.distance
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
        + snakeDir * FORWARD_OFFSET 
        + snakeUp * (CAMERA_SETTINGS.minHeight * 0.2f);
    cameraTarget = glm::mix(cameraTarget, idealLookAtPoint, CAMERA_SETTINGS.smoothFactor);

    // 构建视图矩阵
    viewMatrix = glm::lookAt(
        cameraPos,
        cameraTarget,
        glm::vec3(glm::mat4_cast(currentCameraRotation) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f))
    );

    // 更新投影矩阵
    float aspect = width() / static_cast<float>(height());
    projectionMatrix = glm::perspective(
        glm::radians(currentFOV),
        aspect,
        0.1f,
        3000.0f
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
            // 相机在外部时，面向相机的面完全透明
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

void GameWidget::initShaders()
{
    // 暂时空实现，后续添加着色器
}

void GameWidget::createAquarium()
{
    // 暂时空实现，实际绘制在drawAquarium中
}

// 暂时把障碍物放在地面上
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
    snake = new Snake(-5.0f, 0.0f, 0.0f);  // 起始位置靠左一些
    snake->setDirection(glm::vec3(1.0f, 0.0f, 0.0f));

    glm::vec3 newPos = snake->getHeadPosition();
    if(!isInAquarium(newPos)) {
        qDebug() << "WARNING: Reset position is out of bounds! Adjusting...";
        // 如果出界，强制移动到安全位置
        newPos = glm::vec3(0.0f, 0.0f, 0.0f);
        // 这里需要添加更好的蛇位置的逻辑
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
void GameWidget::updateUnderwaterEffects()
{
    // 始终更新水下效果参数，无论是否在水中
    float depth = -cameraPos.y;
    float depthFactor = std::min(1.0f, std::abs(depth) / (aquariumSize * 0.5f));
    
    // 计算水面过渡效果
    float transitionZone = 100.0f;
    float transitionFactor = 1.0f;
    
    if(std::abs(cameraPos.y) < transitionZone) {
        transitionFactor = std::abs(cameraPos.y) / transitionZone;
    }
    
    // 更新参数
    static float currentFogDensity = underwaterEffects.fogDensity;
    static glm::vec3 currentFogColor = underwaterEffects.fogColor;
    
    // 使用稳定性参数进行插值
    currentFogDensity = glm::mix(currentFogDensity, 
                                0.001f + depthFactor * 0.002f,
                                1.0f - underwaterEffects.lightStability);
                                
    currentFogColor = glm::mix(currentFogColor,
                              glm::mix(glm::vec3(0.2f, 0.3f, 0.4f),
                                      glm::vec3(0.1f, 0.15f, 0.25f),
                                      depthFactor),
                              1.0f - underwaterEffects.lightStability);
    
    // 保存计算结果
    underwaterEffects.fogDensity = currentFogDensity;
    underwaterEffects.fogColor = currentFogColor;
    
    // 更新光照
    float baseLight = glm::mix(underwaterEffects.maxAmbientLight,
                              underwaterEffects.minAmbientLight,
                              depthFactor * underwaterEffects.depthDarkening);
                              
    underwaterEffects.currentLight = glm::mix(underwaterEffects.currentLight,
                                             baseLight,
                                             1.0f - underwaterEffects.lightStability);
}

void GameWidget::renderUnderwaterEffects()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 增强水下雾效果
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    GLfloat fogColor[] = {
        underwaterEffects.fogColor.r,
        underwaterEffects.fogColor.g,
        underwaterEffects.fogColor.b,
        1.0f
    };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, underwaterEffects.fogDensity);
    
    // 添加更明显的水下色调叠加
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // 使用两个半透明层来增强效果
    // 第一层：基础水下色调
    glColor4f(
        underwaterEffects.fogColor.r,
        underwaterEffects.fogColor.g,
        underwaterEffects.fogColor.b,
        0.3f  // 增加基础透明度
    );
    
    glBegin(GL_QUADS);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f( 1.0f, -1.0f);
    glVertex2f( 1.0f,  1.0f);
    glVertex2f(-1.0f,  1.0f);
    glEnd();
    
    // 第二层：渐变效果
    glBegin(GL_QUADS);
    // 底部更深的颜色
    glColor4f(0.0f, 0.1f, 0.2f, 0.4f);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f( 1.0f, -1.0f);
    // 顶部较浅的颜色
    glColor4f(0.1f, 0.3f, 0.5f, 0.0f);
    glVertex2f( 1.0f,  1.0f);
    glVertex2f(-1.0f,  1.0f);
    glEnd();
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDisable(GL_BLEND);

    if(isInAquarium(cameraPos) && cameraPos.y < 0) {
        float depth = -cameraPos.y;
        float depthFactor = std::min(1.0f, depth / (aquariumSize * 0.5f));
        
        // 计算稳定的基础亮度
        float baseLight = glm::mix(
            underwaterEffects.maxAmbientLight,
            underwaterEffects.minAmbientLight,
            depthFactor * underwaterEffects.depthDarkening
        );
        
        // 使用光照稳定性参数来平滑过渡
        static float currentLight = baseLight;
        currentLight = glm::mix(currentLight, baseLight, 1.0f - underwaterEffects.lightStability);
        
        // 调整环境光
        GLfloat ambient[] = {
            currentLight,
            currentLight * 1.1f,  // 略微偏蓝
            currentLight * 1.2f,
            1.0f
        };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
        
        // 调整雾效果，使其更加稳定
        underwaterEffects.fogDensity = 0.001f + depthFactor * 0.002f;
        underwaterEffects.fogColor = glm::mix(
            glm::vec3(0.2f, 0.3f, 0.4f),  // 浅水颜色
            glm::vec3(0.1f, 0.15f, 0.25f), // 深水颜色
            depthFactor
        );
        
        GLfloat fogColor[] = {
            underwaterEffects.fogColor.r,
            underwaterEffects.fogColor.g,
            underwaterEffects.fogColor.b,
            1.0f
        };
        glFogfv(GL_FOG_COLOR, fogColor);
        glFogf(GL_FOG_DENSITY, underwaterEffects.fogDensity);
    }
}

void GameWidget::initLights() {
    // 清除现有光源
    lightSources.clear();
    
    // 1. 主太阳光(方向光)
    lightSources.push_back({
        glm::vec3(0.0f, aquariumSize, 0.0f),          // 位置
        glm::vec3(0.0f, -1.0f, 0.0f),                 // 向下的方向
        glm::vec3(1.0f, 0.95f, 0.8f),                 // 偏暖的白色
        lightingParams.sunlightIntensity,
        0.0f,                                          // 方向光不需要半径
        0.0f                                           // 方向光不需要衰减
    });

    // 2. 添加焦散点光源
    for(int i = 0; i < 4; ++i) {
        lightSources.push_back({
            glm::vec3(0.0f),                          // 位置会动态更新
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.6f, 0.8f, 1.0f),             // 偏蓝的色调
            lightingParams.causticLightIntensity,
            100.0f,                                   // 光照半径
            2.0f                                      // 衰减系数
        });
    }

    // 3. 体积光源
    lightSources.push_back({
        glm::vec3(0.0f, aquariumSize * 0.5f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.4f, 0.6f, 0.8f),                // 偏蓝的色调
        lightingParams.volumetricIntensity,
        200.0f,
        1.0f
    });
    
    lightSources.clear();
    
    // 调整光源参数使其更明显
    // 1. 主太阳光(方向光) - 增强强度
    lightSources.push_back({
        glm::vec3(0.0f, aquariumSize * 1.5f, 0.0f),  // 提高光源位置
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(1.0f, 0.95f, 0.8f),
        2.0f,  // 增加强度
        0.0f,
        0.0f
    });

    // 2. 环境点光源 - 增加数量和强度
    for(int i = 0; i < 4; ++i) {
        float angle = i * glm::pi<float>() * 0.5f;
        lightSources.push_back({
            glm::vec3(sin(angle) * aquariumSize * 0.8f, 
                     aquariumSize * 0.7f,
                     cos(angle) * aquariumSize * 0.8f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.8f, 0.9f, 1.0f),
            1.5f,  // 增加强度
            aquariumSize * 0.5f,  // 增加范围
            1.0f
        });
    }

    // 应用光源到OpenGL
    for(size_t i = 0; i < lightSources.size() && i < 8; ++i) {
        GLenum light = GL_LIGHT0 + i;
        const auto& src = lightSources[i];
        
        // 启用这个光源
        glEnable(light);
        
        // 设置光源属性
        GLfloat position[] = {
            src.position.x, src.position.y, src.position.z,
            (src.radius > 0.0f) ? 1.0f : 0.0f
        };
        GLfloat ambient[] = {
            src.color.r * 0.2f * src.intensity,
            src.color.g * 0.2f * src.intensity,
            src.color.b * 0.2f * src.intensity,
            1.0f
        };
        GLfloat diffuse[] = {
            src.color.r * src.intensity,
            src.color.g * src.intensity,
            src.color.b * src.intensity,
            1.0f
        };
        GLfloat direction[] = {
            src.direction.x,
            src.direction.y,
            src.direction.z,
            0.0f
        };
        
        glLightfv(light, GL_POSITION, position);
        glLightfv(light, GL_AMBIENT, ambient);
        glLightfv(light, GL_DIFFUSE, diffuse);
        glLightfv(light, GL_SPOT_DIRECTION, direction);
        
        // 设置衰减
        if(src.radius > 0.0f) {
            glLightf(light, GL_CONSTANT_ATTENUATION, 1.0f);
            glLightf(light, GL_LINEAR_ATTENUATION, src.attenuation * 0.0001f);
            glLightf(light, GL_QUADRATIC_ATTENUATION, src.attenuation * 0.00001f);
        }
    }
}

void GameWidget::updateLights() {
    // 更新焦散光源位置 - 使用 chrono 代替 QTime
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0f;
    
    for(size_t i = 1; i < 5; ++i) {  // 跳过主太阳光
        float phase = time * 0.5f + i * glm::pi<float>() * 0.5f;
        float radius = aquariumSize * 0.3f;
        
        lightSources[i].position = glm::vec3(
            sin(phase) * radius,
            aquariumSize * 0.4f,
            cos(phase) * radius
        );
    }

    // 应用光源到OpenGL
    for(size_t i = 0; i < lightSources.size() && i < 8; ++i) {
        const auto& light = lightSources[i];
        GLenum lightEnum = GL_LIGHT0 + i;

        // 位置和方向
        GLfloat position[] = {
            light.position.x, light.position.y, light.position.z, 
            (light.radius > 0.0f) ? 1.0f : 0.0f  // w=1表示点光源，w=0表示方向光
        };
        glLightfv(lightEnum, GL_POSITION, position);

        if(light.radius > 0.0f) {
            // 点光源衰减
            glLightf(lightEnum, GL_CONSTANT_ATTENUATION, 1.0f);
            glLightf(lightEnum, GL_LINEAR_ATTENUATION, light.attenuation * 0.001f);
            glLightf(lightEnum, GL_QUADRATIC_ATTENUATION, light.attenuation * 0.0001f);
        }

        // 光照颜色和强��
        GLfloat ambient[] = {
            light.color.r * 0.1f * light.intensity,
            light.color.g * 0.1f * light.intensity,
            light.color.b * 0.1f * light.intensity,
            1.0f
        };
        GLfloat diffuse[] = {
            light.color.r * light.intensity,
            light.color.g * light.intensity,
            light.color.b * light.intensity,
            1.0f
        };
        
        glLightfv(lightEnum, GL_AMBIENT, ambient);
        glLightfv(lightEnum, GL_DIFFUSE, diffuse);
    }

    // 水下光照调整逻辑简化
    if(isInAquarium(cameraPos)) {
        const float lightTransitionSpeed = 0.05f;  // 降低光照变化速度
        
        static float currentLightIntensity = 1.0f;
        float targetIntensity = (cameraPos.y < 0) ? 
            underwaterEffects.currentLight : 1.0f;
            
        currentLightIntensity = glm::mix(currentLightIntensity,
                                        targetIntensity,
                                        lightTransitionSpeed);
        
        // 应用光照设置
        for(auto& light : lightSources) {
            light.intensity *= currentLightIntensity;
        }
        
        // 更新全局环境光
        GLfloat globalAmbient[] = {
            currentLightIntensity * 0.3f,
            currentLightIntensity * 0.35f,
            currentLightIntensity * 0.4f,
            1.0f
        };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);
    }
    // 修改水下光照更新逻辑
    static float lastDepth = 0.0f;
    static float currentIntensity = 1.0f;
    
    if(isInAquarium(cameraPos)) {
        float depth = std::max(0.0f, -cameraPos.y);  // 确保深度不会为负
        float depthFactor = std::min(1.0f, depth / (aquariumSize * 0.5f));
        
        // 平滑深度变化
        lastDepth = glm::mix(lastDepth, depth, 0.1f);
        
        // 在水面附近时也应用过渡效果
        float transitionZone = 100.0f;  // 过渡区域大小
        float waterSurfaceEffect = 1.0f;
        
        if(cameraPos.y > 0 && cameraPos.y < transitionZone) {
            waterSurfaceEffect = 1.0f - (cameraPos.y / transitionZone);
        }
        
        // 计算目标光照强度
        float targetIntensity = waterSurfaceEffect * (1.0f - depthFactor * underwaterEffects.depthDarkening);
        targetIntensity = glm::mix(underwaterEffects.minAmbientLight, 1.0f, targetIntensity);
        
        // 平滑过渡当前光照强度
        currentIntensity = glm::mix(currentIntensity, targetIntensity, 
                                  1.0f - underwaterEffects.lightStability);
        
        // 应用光照设置
        for(auto& light : lightSources) {
            light.intensity *= currentIntensity;
        }
        
        // 设置全局环境光
        GLfloat globalAmbient[] = {
            currentIntensity * 0.3f,
            currentIntensity * 0.35f,
            currentIntensity * 0.4f,
            1.0f
        };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);
    }
}