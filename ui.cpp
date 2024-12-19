#include "ui.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QPalette>
#include <QGraphicsBlurEffect>
#include <QStyleOption>
#include <QPainter>
#include <GL/glew.h>

// MenuWidget 实现
MenuWidget::MenuWidget(QWidget *parent) 
    : QOpenGLWidget(parent)
    , cameraAngle(0.0f)
{
    setMinimumSize(800, 600);
    
    // 创建独立的游戏实例用于背景显示
    gameWidget = new GameWidget(nullptr);  // 不设置父对象，避免自动删除
    gameWidget->hide();  // 隐藏游戏widget
    gameWidget->pauseGame();  // 暂停游戏状态
    
    // 初始化游戏场景
    gameWidget->resetGame();
    
    initUI();
    
    // 初始化相机动画计时器
    cameraTimer = new QTimer(this);
    connect(cameraTimer, &QTimer::timeout, [this]() {
        cameraAngle += 0.0002f;  // 降低旋转速度
        updateCamera();
        update();
    });
    cameraTimer->start(16);
}

MenuWidget::~MenuWidget() {
    if (gameWidget) {
        delete gameWidget;
        gameWidget = nullptr;
    }
}

void MenuWidget::initUI() {
    // 创建标题标签
    titleLabel = new QLabel("AquaSnake 3D", this);
    QFont titleFont("Arial", 48, QFont::Bold);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: white; text-shadow: 2px 2px 4px #000000;");
    titleLabel->setAlignment(Qt::AlignCenter);
    
    // 创建开始按钮
    startButton = new QPushButton("开始游戏", this);
    startButton->setMinimumSize(200, 60);
    startButton->setFont(QFont("Arial", 16));
    startButton->setStyleSheet(
        "QPushButton {"
        "    background-color: rgba(255, 255, 255, 0.8);"
        "    border: 2px solid white;"
        "    border-radius: 10px;"
        "    color: #2196F3;"
        "    padding: 10px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(255, 255, 255, 1.0);"
        "    color: #1976D2;"
        "}"
    );
    
    connect(startButton, &QPushButton::clicked, this, &MenuWidget::startGameClicked);
    
    // 布局
    titleLabel->setGeometry(0, 100, width(), 100);
    startButton->setGeometry((width() - 200) / 2, height() - 200, 200, 60);
}

void MenuWidget::initializeGL() {
    initializeOpenGLFunctions();
    glewInit();
    
    // 初始化游戏的OpenGL上下文
    gameWidget->initializeGL();
}

void MenuWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    
    // 更新游戏视图大小
    gameWidget->resizeGL(w, h);
    
    // 更新UI元素位置
    titleLabel->setGeometry(0, 100, w, 100);
    startButton->setGeometry((w - 200) / 2, h - 200, 200, 60);
}

void MenuWidget::paintGL() {
    // 使用游戏的渲染函数，但使用我们自定义的相机视角
    if (gameWidget) {
        // 保存游戏原来的相机状态和游戏状态
        glm::mat4 originalView = gameWidget->getViewMatrix();
        glm::mat4 originalProj = gameWidget->getProjectionMatrix();
        GameWidget::GameState originalState = gameWidget->gameState;
        
        // 设置我们的相机视角，并临时将游戏状态设为READY以避免水下效果
        gameWidget->setViewMatrix(viewMatrix);
        gameWidget->setProjectionMatrix(projectionMatrix);
        gameWidget->gameState = GameWidget::GameState::READY;
        
        // 渲染游戏场景
        gameWidget->paintGL();
        
        // 恢复原来的状态
        gameWidget->setViewMatrix(originalView);
        gameWidget->setProjectionMatrix(originalProj);
        gameWidget->gameState = originalState;
    }
}

void MenuWidget::updateCamera() {
    if (!gameWidget) return;
    
    float aquariumSize = gameWidget->getAquariumSize();
    float cameraHeight = aquariumSize * 1.2f;      // 增加高度
    float cameraDistance = aquariumSize * 1.8f;    // 增加距离
    
    // 计算相机位置，使用倾斜角度
    float x = sin(cameraAngle) * cameraDistance;
    float z = cos(cameraAngle) * cameraDistance;
    float y = cameraHeight;
    
    // 调整相机位置和目标点
    glm::vec3 cameraPos(x, y, z);
    glm::vec3 cameraTarget(0.0f, -aquariumSize * 0.1f, 0.0f);  // 略微向下看
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    
    viewMatrix = glm::lookAt(cameraPos, cameraTarget, up);
    
    // 设置投影矩阵
    float aspect = float(width()) / float(height());
    projectionMatrix = glm::perspective(glm::radians(45.0f), aspect, 0.1f, aquariumSize * 20.0f);  // 增加远平面距离
}

// GameHUD 实现
GameHUD::GameHUD(QWidget* parent) 
    : QWidget(parent)
    , m_isPaused(false)
{
    setStyleSheet("background: transparent;");
    
    // 创建长度标签
    lengthLabel = new QLabel("长度: --", this);
    QFont labelFont("Arial", 12, QFont::Bold);  // 减小字体大小
    lengthLabel->setFont(labelFont);
    lengthLabel->setStyleSheet(
        "color: white;"
        "background: rgba(0, 0, 0, 0.5);"
        "padding: 5px 10px;"  // 减小内边距
        "border-radius: 5px;"
    );
    lengthLabel->setFixedWidth(100);  // 固定宽度
    
    // 创建按钮
    pauseResumeButton = new QPushButton("暂停", this);
    restartButton = new QPushButton("重新开始", this);
    
    QFont buttonFont("Arial", 12);
    pauseResumeButton->setFont(buttonFont);
    restartButton->setFont(buttonFont);
    
    QString buttonStyle = 
        "QPushButton {"
        "    background-color: rgba(255, 255, 255, 0.8);"
        "    border: none;"
        "    border-radius: 5px;"
        "    padding: 5px 10px;"
        "    color: #2196F3;"
        "    min-width: 80px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(255, 255, 255, 1.0);"
        "    color: #1976D2;"
        "}";
    
    pauseResumeButton->setStyleSheet(buttonStyle);
    restartButton->setStyleSheet(buttonStyle);
    
    // 创建主布局
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // 左侧放置长度标签
    mainLayout->addWidget(lengthLabel);
    
    // 添加弹性空间
    mainLayout->addStretch();
    
    // 右侧放置按���
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(10);
    buttonLayout->addWidget(pauseResumeButton);
    buttonLayout->addWidget(restartButton);
    mainLayout->addLayout(buttonLayout);
    
    setLayout(mainLayout);
    
    // 修复暂停/继续按钮的逻辑
    connect(pauseResumeButton, &QPushButton::clicked, [this]() {
        if (!m_isPaused) {  // 如果当前未暂停
            m_isPaused = true;  // 设置为暂停
            pauseResumeButton->setText("继续");  // 显示"继续"
        } else {  // 如果当前已暂停
            m_isPaused = false;  // 设置为未暂停
            pauseResumeButton->setText("暂停");  // 显示"暂停"
        }
        emit pauseResumeClicked();
    });
    
    connect(restartButton, &QPushButton::clicked, this, &GameHUD::restartClicked);
}

void GameHUD::updateLength(int length) {
    lengthLabel->setText(QString("长度: %1").arg(length));
}

// UIManager 实现
UIManager::UIManager(QWidget* parent)
    : QStackedWidget(parent)
{
    // 创建并添加菜单界面
    menuWidget = new MenuWidget(this);
    addWidget(menuWidget);
    
    // 创建游戏界面
    gameWidget = new GameWidget(this);
    addWidget(gameWidget);
    
    // 创建游戏HUD
    gameHUD = new GameHUD(this);
    gameHUD->setGeometry(10, 10, width() - 20, 50);  // 减小高度，只占用顶部
    gameHUD->hide();
    
    // 连接信号
    connect(menuWidget, &MenuWidget::startGameClicked, this, &UIManager::startGame);
    connect(gameHUD, &GameHUD::pauseResumeClicked, this, &UIManager::pauseResumeGame);
    connect(gameHUD, &GameHUD::restartClicked, this, &UIManager::restartGame);
    connect(gameWidget, &GameWidget::lengthChanged, gameHUD, &GameHUD::updateLength);
    
    // 显示菜单
    setCurrentWidget(menuWidget);
}

void UIManager::resizeEvent(QResizeEvent* event)
{
    QStackedWidget::resizeEvent(event);
    if (gameHUD) {
        gameHUD->setGeometry(10, 10, width() - 20, 50);  // 保持在顶部，固定高度
    }
}

void UIManager::startGame() {
    // 创建新的游戏实例
    if (gameWidget) {
        delete gameWidget;
    }
    gameWidget = new GameWidget(this);
    addWidget(gameWidget);
    
    // 重新连接信号
    connect(gameWidget, &GameWidget::lengthChanged, gameHUD, &GameHUD::updateLength);
    connect(gameWidget, &GameWidget::scoreChanged, [this](int score) {
        // 可以在这里处理分数更新
    });
    
    // 设置为当前widget并显示HUD
    setCurrentWidget(gameWidget);
    gameHUD->show();
    gameHUD->raise();
    gameWidget->setFocus();
    gameHUD->updateLength(3);  // 设置初始长度
    
    // 确保游戏处于正确的状态
    gameWidget->resetGame();
}

void UIManager::pauseResumeGame() {
    if (gameWidget) {
        if (gameHUD->getIsPaused()) {
            gameWidget->pauseGame();
        } else {
            gameWidget->resumeGame();
            gameWidget->setFocus();  // 确保游戏窗口获得焦点
        }
    }
}

void UIManager::restartGame() {
    gameWidget->resetGame();
    gameHUD->updateLength(3);  // 重��长度
} 