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
    , isGameOver(false)
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
    , currentCameraMode(CameraMode::FOLLOW)
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

    // 发送初始长度
    emit lengthChanged(snake->getBody().size());

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
    // 首先化OpenGL函数
    initializeOpenGLFunctions();
    glewInit();

    // 设置基本状态
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 设置全局光照
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

    // 初始化水体 - 确保在其他组件之后初始化
    if(water) {
        delete water;
        water = nullptr;
    }
    water = new Water(aquariumSize);
    water->initializeGL();  // 确保调用水体的OpenGL初始化
    water->init();
    
    // 初始化光源系统
    initLights();
    
    // 验证水体初始化
    if(water) {
        qDebug() << "Water system initialized successfully";
        qDebug() << "Aquarium size:" << aquariumSize;
        // 设置水体机位置
        water->setCameraPosition(cameraPos);
    } else {
        qDebug() << "Failed to initialize water system!";
    }
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
    
    // 设置基本状态
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
    
    // 检查是否在水下，同时确保游戏状态为PLAYING
    bool isUnderwater = water && gameState == GameState::PLAYING && water->isUnderwater(cameraPos);
    
    if(isUnderwater && water) {
        // 在水下时，先应用水下效果
        water->beginUnderwaterEffect(projectionMatrix, viewMatrix);
    }
    
    // 1. 绘制水族箱
    drawAquarium();
    
    // 2. 绘制场景对象
    drawSceneObjects();
    
    // 3. 渲染水体和水下效果
    if(water) {
        // 保存当前状态
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        
        if(isUnderwater) {
            // 如果在水下，先渲染水下效果
            water->renderUnderwaterEffects(projectionMatrix, viewMatrix);
            renderUnderwaterEffects(); // 渲染额外的水下效果
        }
        
        // 渲染水体表面
        water->render(projectionMatrix, viewMatrix);
        
        // 渲染粒子
        water->renderWaterParticles();
        
        // 恢复状态
        glPopAttrib();
    }
    
    if(isUnderwater && water) {
        water->endUnderwaterEffect();
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
    
    // 重新确保光照用
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

    //绘制食物
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

    // 添加相机模式切换按键 (V键)
    if (event->key() == Qt::Key_V) {
        currentCameraMode = (currentCameraMode == CameraMode::FOLLOW) ? 
                           CameraMode::TOP_DOWN : CameraMode::FOLLOW;
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
    
    // 如果下一个位置会出界，不移动蛇，等待新的输入
    if(!isInAquarium(nextPos)) {
        return;  // 直接返回，不结束游戏
    }

    // 移动蛇
    snake->move();
    
    // 更新并发送当前长度
    emit lengthChanged(snake->getBody().size());
    
    // 更新水体和粒子效果
    if (water) {
        water->update(deltaTime);
        
        // 获取蛇头位置和方向
        glm::vec3 snakePos = snake->getHeadPosition();
        glm::vec3 snakeDir = snake->getDirection();
        
        // 计算粒子生成位置（在蛇头前方）
        glm::vec3 particleSpawnPos = snakePos + snakeDir * 5.0f;
        
        // 更新粒子系统
        water->updateWaterParticles(deltaTime, particleSpawnPos);
    }
    
    // 检查食物碰撞
    bool foodEaten = false;
    std::vector<size_t> foodToRemove;
    
    // 先检查所有需要移除的食物
    for(size_t i = 0; i < foods.size(); ++i) {
        float collisionDistance = snake->getSegmentSize() * FOOD_COLLISION_MULTIPLIER;
        float distance = glm::distance(snake->getHeadPosition(), foods[i].getPosition());
        
        if (distance < collisionDistance) {
            foodToRemove.push_back(i);
            foodEaten = true;
        }
    }
    
    // 如果吃到了食物
    if(foodEaten) {
        score += 10 * foodToRemove.size();
        emit scoreChanged(score);
        
        // 从后向前移除食物
        for(auto it = foodToRemove.rbegin(); it != foodToRemove.rend(); ++it) {
            if(*it < foods.size()) {
                foods.erase(foods.begin() + *it);
            }
        }
        
        // 让蛇长
        for(size_t i = 0; i < foodToRemove.size() * Snake::GROWTH_FACTOR; ++i) {
            snake->grow();
        }
        
        emit lengthChanged(snake->getBody().size());
        
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
    
    // 更新水体
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

    if (currentCameraMode == CameraMode::TOP_DOWN) {
        // 俯视视角的相机位置计算
        glm::vec3 idealCameraPos = snakeHead + glm::vec3(0.0f, TOP_DOWN_HEIGHT, 0.0f);
        
        // 平滑过渡到新的相机位置
        cameraPos = glm::mix(cameraPos, idealCameraPos, TOP_DOWN_SMOOTH_FACTOR);
        
        // 相机始终看向蛇头，保持固定的上方向
        cameraTarget = glm::mix(cameraTarget, snakeHead, TOP_DOWN_SMOOTH_FACTOR);
        
        // 构建视图矩阵，使用固定���世界空间上方向
        viewMatrix = glm::lookAt(
            cameraPos,
            cameraTarget,
            glm::vec3(0.0f, 0.0f, -1.0f)  // 固定世界空间上方
        );
    } else {
        // 原有的跟随视角逻辑
        glm::vec3 idealLookDir = -snakeDir;
        glm::vec3 idealUp = snakeUp;
        
        glm::mat3 targetRotationMat = glm::mat3(
            glm::cross(idealLookDir, idealUp),
            idealUp,
            -idealLookDir
        );
        
        targetCameraRotation = glm::quat_cast(targetRotationMat);
        currentCameraRotation = glm::slerp(
            currentCameraRotation,
            targetCameraRotation,
            rotationSmoothFactor
        );
        
        glm::mat4 rotationMatrix = glm::mat4_cast(currentCameraRotation);
        glm::vec3 baseOffset = glm::vec3(
            0.0f,
            CAMERA_SETTINGS.minHeight * 0.5f,
            -CAMERA_SETTINGS.distance * 0.4f
        );
        
        glm::vec3 rotatedOffset = glm::vec3(rotationMatrix * glm::vec4(baseOffset, 1.0f));
        glm::vec3 idealCameraPos = snakeHead + rotatedOffset;
        
        glm::vec3 sideOffset = snakeRight * (CAMERA_SETTINGS.distance * SIDE_OFFSET_FACTOR);
        idealCameraPos += sideOffset;
        
        cameraPos = glm::mix(cameraPos, idealCameraPos, CAMERA_SETTINGS.smoothFactor);
        
        glm::vec3 idealLookAtPoint = snakeHead 
            + snakeDir * (FORWARD_OFFSET * 0.5f)
            + snakeUp * (CAMERA_SETTINGS.minHeight * 0.1f);
        
        cameraTarget = glm::mix(cameraTarget, idealLookAtPoint, CAMERA_SETTINGS.smoothFactor);

        viewMatrix = glm::lookAt(
            cameraPos,
            cameraTarget,
            glm::vec3(glm::mat4_cast(currentCameraRotation) * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f))
        );
    }

    // 修改投影矩阵
    float aspect = width() / static_cast<float>(height());
    projectionMatrix = glm::perspective(
        glm::radians(currentFOV),
        aspect,
        1.0f,
        10000.0f
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
            
            // ���查与其他食物的距离
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

    // 绘制不透明的边界框
    glLineWidth(8.0f);  // 更粗的边界线
    glColor3f(0.0f, 0.7f, 1.0f);  // 更亮的色边界
    
    // 绘制12条边界线
    glBegin(GL_LINES);
    float hs = aquariumSize * 0.5f;  // 高度限制为一半
    
    // 底部四条线
    glVertex3f(-aquariumSize, -hs, -aquariumSize); glVertex3f(aquariumSize, -hs, -aquariumSize);
    glVertex3f(-aquariumSize, -hs, -aquariumSize); glVertex3f(aquariumSize, -hs, aquariumSize);
    glVertex3f(-aquariumSize, -hs, -aquariumSize); glVertex3f(-aquariumSize, -hs, aquariumSize);
    glVertex3f(aquariumSize, -hs, -aquariumSize); glVertex3f(aquariumSize, -hs, aquariumSize);
    
    // 顶部四条线
    glVertex3f(-aquariumSize, hs, -aquariumSize); glVertex3f(aquariumSize, hs, -aquariumSize);
    glVertex3f(-aquariumSize, hs, aquariumSize); glVertex3f(aquariumSize, hs, aquariumSize);
    glVertex3f(-aquariumSize, hs, -aquariumSize); glVertex3f(-aquariumSize, hs, aquariumSize);
    glVertex3f(aquariumSize, hs, -aquariumSize); glVertex3f(aquariumSize, hs, aquariumSize);
    
    // 直四条线
    glVertex3f(-aquariumSize, -hs, -aquariumSize); glVertex3f(-aquariumSize, hs, -aquariumSize);
    glVertex3f(aquariumSize, -hs, -aquariumSize); glVertex3f(aquariumSize, hs, -aquariumSize);
    glVertex3f(-aquariumSize, -hs, aquariumSize); glVertex3f(-aquariumSize, hs, aquariumSize);
    glVertex3f(aquariumSize, -hs, aquariumSize); glVertex3f(aquariumSize, hs, aquariumSize);
    glEnd();

    // 修深度测试设置并绘制透明面
    glDepthMask(GL_FALSE);  // 禁度写入
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
        
        // 设置面的法线和中点
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
            // 相机在外部时，面向相的面完全透明
            alpha = (dotProduct < 0.0f) ? 0.01f : baseAlpha;  // 低的透明度
        } else {
            // 相机在内部时使用透明度
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
    glDepthMask(GL_TRUE);   // 新启用深度写入
    glLineWidth(1.0f);
}

void GameWidget::initObstacles()
{
    obstacles.clear();
    int maxAttempts = 1000;  // 增加最大尝试次数
    
    // 基于水族箱大小计算障碍物尺寸
    const float BASE_OBSTACLE_SIZE = aquariumSize * 0.0025f;  // 水族箱大小的0.25%
    const float ROCK_SIZE = BASE_OBSTACLE_SIZE * 1.2f;      // 岩石稍大一些
    const float SPIKY_SIZE = BASE_OBSTACLE_SIZE * 1.1f;     // 尖刺球介于两者之间
    
    // 首先放置岩石（固定数量15个）
    int rocksPlaced = 0;
    while (rocksPlaced < Obstacle::MAX_ROCKS && maxAttempts > 0) {
        float range = aquariumSize * 0.8f;  // 在80%的范围内放置
        float x = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range;
        float z = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range;
        float y = -aquariumSize * 0.45f;  // 固定在底部
        
        glm::vec3 newPos(x, y, z);
        
        // 确保岩石不会出现在蛇的初始位置附近
        if (glm::length(newPos - snake->getHeadPosition()) < aquariumSize * 0.1f) {
            maxAttempts--;
            continue;
        }
        
        // 检查与现有障碍物的重叠
        bool overlapping = false;
        for (const auto& obstacle : obstacles) {
            float minDistance = (obstacle.getRadius() + ROCK_SIZE) * 2.0f;  // 增加岩石间距
            if (glm::length(obstacle.getPosition() - newPos) < minDistance) {
                overlapping = true;
                break;
            }
        }
        
        if (!overlapping) {
            obstacles.emplace_back(newPos, ROCK_SIZE, Obstacle::Type::ROCK);
            rocksPlaced++;
        }
        
        maxAttempts--;
    }
    
    // 然后放置其他障碍物（总共80个）
    int otherObstaclesPlaced = 0;
    maxAttempts = 1000;  // 重置尝试次数
    
    while (otherObstaclesPlaced < Obstacle::MAX_OTHER_OBSTACLES && maxAttempts > 0) {
        float range = aquariumSize * 0.8f;
        float x = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range;
        float z = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range;
        float y = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * range * 0.5f;  // 高度范围减半
        
        glm::vec3 newPos(x, y, z);
        
        // 确保不会出现在蛇的初始位置附近
        if (glm::length(newPos - snake->getHeadPosition()) < aquariumSize * 0.1f) {
            maxAttempts--;
            continue;
        }
        
        // 随机选择障碍物类型（尖刺球或立方体）
        Obstacle::Type obstacleType = (float(rand()) / RAND_MAX < 0.6f) ? 
                                    Obstacle::Type::SPIKY_SPHERE : 
                                    Obstacle::Type::CUBE;
        
        float obstacleSize = (obstacleType == Obstacle::Type::SPIKY_SPHERE) ? 
                            SPIKY_SIZE : BASE_OBSTACLE_SIZE * 0.7f;
        
        // 检查与现有障碍物的重叠
        bool overlapping = false;
        for (const auto& obstacle : obstacles) {
            float minDistance = (obstacle.getRadius() + obstacleSize) * 1.5f;
            if (glm::length(obstacle.getPosition() - newPos) < minDistance) {
                overlapping = true;
                break;
            }
        }
        
        if (!overlapping) {
            obstacles.emplace_back(newPos, obstacleSize, obstacleType);
            otherObstaclesPlaced++;
        }
        
        maxAttempts--;
    }
    
    qDebug() << "成功生成障碍物：" 
             << "\n岩石数量:" << rocksPlaced 
             << "\n其他障碍物数量:" << otherObstaclesPlaced;
}

void GameWidget::resetGame()
{
    qDebug() << "=== GAME RESET ===";
    qDebug() << "Previous state:" << static_cast<int>(gameState);

    // 设置状态（只设置一次）
    gameState = GameState::PLAYING;
    isGameOver = false;
    score = 0;
    emit scoreChanged(score);
    
    qDebug() << "Game state reset to PLAYING:" << static_cast<int>(gameState);

    // 删除旧的蛇并创建新的
    delete snake;
    snake = new Snake(-5.0f, 0.0f, 0.0f);
    emit lengthChanged(snake->getBody().size());
    
    // 如果OpenGL已初始化，则初始化蛇的OpenGL函数
    if (context() && context()->isValid()) {
        snake->initializeGL();
    }
    
    // 如果出界，移动到安全位置
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
    
    // 检查与障碍物的碰撞
    for(const auto& obstacle : obstacles) {
        float collisionDistance = glm::length(headPos - obstacle.getPosition());
        float collisionRange = (snake->getSegmentSize() + obstacle.getRadius()) * OBSTACLE_COLLISION_MULTIPLIER;
        
        if(collisionDistance < collisionRange) {
            qDebug() << "Game over! Collision with obstacle at distance:" << collisionDistance;
            gameState = GameState::GAME_OVER;
            isGameOver = true;
            emit gameOver();
            return;
        }
    }

    // 检查与蛇身的碰撞
    if(snake->checkSelfCollision()) {
        qDebug() << "Game over! Self collision";
        gameState = GameState::GAME_OVER;
        isGameOver = true;
        emit gameOver();
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

    // 应用光源
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
            // 配置聚光灯数
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
            // 水增强光照
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
    for(size_t i = 4; i < 7; ++i) {  // 更新束柱
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
    if(water && isInAquarium(cameraPos)) {
        float depth = std::max(0.0f, water->getWaterHeight() - cameraPos.y);
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
    
    // 应用更新的光照设置
    applyLightSettings();
}

void GameWidget::updateBubbles() {
    if(water) {
        // 传入 deltaTime
        water->updateBubbles(deltaTime);
        return;
    }
}

void GameWidget::renderUnderwaterEffects() {
    if (!water) return;

    // 保存当前状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // 启用雾效但降低其强度
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    
    // 计算基于深度的雾效参数，但显著降低其影响
    float depth = water->getWaterHeight() - cameraPos.y;
    float depthFactor = std::min(1.0f, depth * 0.0001f);  // 进一步降低深度影响
    
    // 使用更柔和的雾效颜色
    glm::vec3 fogColor = glm::mix(
        glm::vec3(0.2f, 0.4f, 0.6f),  // 浅水颜色
        glm::vec3(0.1f, 0.2f, 0.4f),  // 深水颜色
        depthFactor
    );
    
    // 设置雾效参数
    float fogDensity = 0.0002f * (1.0f + depthFactor * 0.3f);  // 降低基础雾气密度
    GLfloat fogCol[] = { fogColor.r, fogColor.g, fogColor.b, 1.0f };
    glFogfv(GL_FOG_COLOR, fogCol);
    glFogf(GL_FOG_DENSITY, fogDensity);
    
    // 增强环境光以提高可见度
    float ambientIntensity = glm::mix(0.8f, 0.4f, depthFactor);  // 增强环境光
    GLfloat ambient[] = {
        ambientIntensity * 1.2f,
        ambientIntensity * 1.2f,
        ambientIntensity * 1.3f,
        1.0f
    };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    
    // 添加水下色调
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 渲���全屏色调叠加
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glBegin(GL_QUADS);
    glColor4f(fogColor.r, fogColor.g, fogColor.b, 0.2f);  // 使用更低的透明度
    glVertex2f(-1.0f, -1.0f);
    glVertex2f( 1.0f, -1.0f);
    glVertex2f( 1.0f,  1.0f);
    glVertex2f(-1.0f,  1.0f);
    glEnd();
    glEnable(GL_DEPTH_TEST);
    
    // 复矩阵
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    // 恢复状态
    glPopAttrib();
}

void GameWidget::applyUnderwaterState() {
    // 保存当前状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // 启用深度测试和混合
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 设置水下颜色调整
    glColor4f(
        underwaterEffects.fogColor.r,
        underwaterEffects.fogColor.g,
        underwaterEffects.fogColor.b,
        0.3f
    );
    
    // 绘制全屏四边形以添加水下色调
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glBegin(GL_QUADS);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f( 1.0f, -1.0f);
    glVertex2f( 1.0f,  1.0f);
    glVertex2f(-1.0f,  1.0f);
    glEnd();
    glEnable(GL_DEPTH_TEST);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    // 恢复状态
    glPopAttrib();
}

// 添加暂停游戏功能
void GameWidget::pauseGame()
{
    if(gameState == GameState::PLAYING) {
        gameState = GameState::PAUSED;
        gameTimer->stop();
    }
}

// 添加继续游戏功能
void GameWidget::resumeGame()
{
    if(gameState == GameState::PAUSED) {
        gameState = GameState::PLAYING;
        gameTimer->start(16);
        setFocus();  // 重新获得焦点
    }
}

// 在文件末尾添加 timerEvent 实现
void GameWidget::timerEvent(QTimerEvent* event)
{
    if (gameState == GameState::PLAYING) {
        updateGame();
        updateCamera();
        update();
    }
}