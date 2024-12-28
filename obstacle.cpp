#include "obstacle.h"
#include <GL/glew.h>
#include <fstream>
#include <sstream>
#include <random>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

// 初始化静态成员
std::vector<glm::vec3> Obstacle::sphereVertices;
std::vector<glm::vec3> Obstacle::sphereNormals;
std::vector<std::vector<int>> Obstacle::sphereFaces;
bool Obstacle::modelLoaded = false;

// 初始化岩石模型相关的静态成员
std::vector<std::vector<glm::vec3>> Obstacle::rockVertices;
std::vector<std::vector<std::vector<int>>> Obstacle::rockFaces;
std::vector<std::vector<glm::vec3>> Obstacle::rockNormals;
bool Obstacle::rockModelsLoaded = false;

// 在cpp文件中实现构造函数
Obstacle::Obstacle(const glm::vec3& pos, float size, Type type) 
    : position(pos)
    , size(size)
    , type(type)
    , rockModelIndex(0)
    , rockHeightScale(1.0f)
    , rockColor(0.6f)
    , rockSpecular(0.2f)
    , rockAmbient(0.4f)
{
    // 如果模型还未加载，先尝试加载
    if (!modelLoaded) {
        loadSphereModel();
    }
    if (!rockModelsLoaded) {
        loadRockModels();
    }
    
    // 如果是���型，随机选择一个岩石模型、高度和颜色
    if (type == Type::ROCK && rockModelsLoaded && !rockVertices.empty()) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> rockDis(0, rockVertices.size() - 1);
        std::uniform_real_distribution<float> heightDis(0.8f, 3.0f);
        std::uniform_real_distribution<float> colorDis(0.0f, 1.0f);
        
        // 随机选择岩石模型和高度
        rockModelIndex = rockDis(gen);
        rockHeightScale = heightDis(gen);
        
        // 直接生成自然的岩石颜色
        const std::vector<glm::vec3> rockColors = {
            glm::vec3(0.5f, 0.3f, 0.2f),  // 褐色
            glm::vec3(0.4f, 0.3f, 0.3f),  // 暗褐色
            glm::vec3(0.6f, 0.5f, 0.4f),  // 浅褐色
            glm::vec3(0.3f, 0.3f, 0.3f),  // 深灰色
            glm::vec3(0.5f, 0.5f, 0.5f),  // 中灰色
            glm::vec3(0.7f, 0.7f, 0.6f),  // 浅灰色
            glm::vec3(0.6f, 0.4f, 0.3f),  // 砂岩色
            glm::vec3(0.4f, 0.4f, 0.3f)   // 橄榄色
        };
        
        // 随机选择一个基础颜色
        int colorIndex = std::uniform_int_distribution<>(0, rockColors.size() - 1)(gen);
        rockColor = rockColors[colorIndex];
        
        // 添加一些随机变化
        float variation = 0.1f;  // 颜色变化范围
        rockColor += glm::vec3(
            colorDis(gen) * variation - variation/2,
            colorDis(gen) * variation - variation/2,
            colorDis(gen) * variation - variation/2
        );
        
        // 设置材质属性
        rockSpecular = glm::vec3(0.1f);  // 保持低高光
        rockAmbient = rockColor * 0.7f;   // 增加环境光强度
        
        qDebug() << "Rock created with color:" 
                 << rockColor.x << rockColor.y << rockColor.z
                 << "height scale:" << rockHeightScale;
    }
}

void Obstacle::loadSphereModel() {
    if (modelLoaded) return;
    
    // 获取可执行文件所在目录
    QString exePath = QCoreApplication::applicationDirPath();
    // 从 build 目录回到项目根目录
    QDir projectDir = QDir(exePath);
    projectDir.cdUp();  // 从 debug 目录出来
    projectDir.cdUp();  // 从 build 目录出来
    projectDir.cdUp();  // 从 out 目录出来
    
    // 构建模型文件的完整路径
    QString filePath = projectDir.absoluteFilePath("objs/spiky_sphere/spiky_sphere_tiny.obj");
    QFile file(filePath);
    qDebug() << "尝试加载模型文件：" << filePath;
    qDebug() << "文件是否存在：" << file.exists();
    qDebug() << "当前目录：" << QDir::currentPath();
    qDebug() << "项目目录：" << projectDir.absolutePath();
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开模型文件：" << filePath;
        qDebug() << "错误信息：" << file.errorString();
        return;
    }
    
    int vertexCount = 0;
    int faceCount = 0;
    
    while (!file.atEnd()) {
        QString line = file.readLine().trimmed();
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        
        if (parts.isEmpty()) continue;
        
        if (parts[0] == "v" && parts.size() >= 4) {
            float x = parts[1].toFloat();
            float y = parts[2].toFloat();
            float z = parts[3].toFloat();
            sphereVertices.push_back(glm::vec3(x, y, z));
            vertexCount++;
        }
        else if (parts[0] == "f" && parts.size() >= 4) {
            std::vector<int> face;
            for (int i = 1; i < parts.size(); ++i) {
                QString indexStr = parts[i];
                int slashPos = indexStr.indexOf('/');
                if (slashPos != -1) {
                    indexStr = indexStr.left(slashPos);
                }
                face.push_back(indexStr.toInt() - 1);
            }
            sphereFaces.push_back(face);
            faceCount++;
        }
    }
    
    file.close();
    
    if (vertexCount == 0 || faceCount == 0) {
        qDebug() << "模型加载失败：没有读取到有效的顶点或面数据";
        return;
    }
    
    // 计算法线
    sphereNormals.resize(sphereVertices.size(), glm::vec3(0.0f));
    for (const auto& face : sphereFaces) {
        if (face.size() < 3) continue;
        glm::vec3 v1 = sphereVertices[face[1]] - sphereVertices[face[0]];
        glm::vec3 v2 = sphereVertices[face[2]] - sphereVertices[face[0]];
        glm::vec3 normal = glm::normalize(glm::cross(v1, v2));
        
        for (int idx : face) {
            sphereNormals[idx] += normal;
        }
    }
    
    // 标准化法线
    for (auto& normal : sphereNormals) {
        normal = glm::normalize(normal);
    }
    
    modelLoaded = true;
    qDebug() << "模型加载成功："
             << "\n顶点数量：" << vertexCount
             << "\n面片数量：" << faceCount
             << "\n当前工作目录：" << QDir::currentPath();
}

void Obstacle::loadRockModels() {
    if (rockModelsLoaded) return;
    
    QString exePath = QCoreApplication::applicationDirPath();
    QDir projectDir = QDir(exePath);
    projectDir.cdUp();
    projectDir.cdUp();
    projectDir.cdUp();
    
    for (int i = 1; i <= 5; ++i) {
        QString filePath = projectDir.absoluteFilePath(QString("objs/rock/rock_%1.obj").arg(i));
        QFile file(filePath);
        
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        
        std::vector<glm::vec3> vertices;
        std::vector<std::vector<int>> faces;
        
        // 一遍：读取所有顶点
        while (!file.atEnd()) {
            QString line = file.readLine().trimmed();
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            
            if (parts.isEmpty()) continue;
            
            if (parts[0] == "v" && parts.size() >= 4) {
                vertices.push_back(glm::vec3(
                    parts[1].toFloat(),
                    parts[2].toFloat(),
                    parts[3].toFloat()
                ));
            }
        }
        
        // 计算模型中心点
        glm::vec3 modelCenter(0.0f);
        for (const auto& vertex : vertices) {
            modelCenter += vertex;
        }
        if (!vertices.empty()) {
            modelCenter /= static_cast<float>(vertices.size());
        }
        
        // 重置文件指针
        file.seek(0);
        
        // 第二遍：读取面数据
        while (!file.atEnd()) {
            QString line = file.readLine().trimmed();
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            
            if (parts.isEmpty()) continue;
            
            if (parts[0] == "f" && parts.size() >= 4) {
                std::vector<int> face;
                for (int j = 1; j < parts.size(); ++j) {
                    QString indexStr = parts[j];
                    QStringList indices = indexStr.split('/');
                    if (!indices.isEmpty()) {
                        face.push_back(indices[0].toInt() - 1);
                    }
                }
                
                // 如果是多边形，分解成三角形
                if (face.size() >= 3) {
                    // 检查第一个三角形的法线方向
                    glm::vec3 v0 = vertices[face[0]];
                    glm::vec3 v1 = vertices[face[1]];
                    glm::vec3 v2 = vertices[face[2]];
                    
                    glm::vec3 edge1 = v1 - v0;
                    glm::vec3 edge2 = v2 - v0;
                    glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
                    
                    // 计算三角形中心
                    glm::vec3 faceCenter = (v0 + v1 + v2) / 3.0f;
                    glm::vec3 toCenter = glm::normalize(modelCenter - faceCenter);
                    
                    // 如果法线朝内需要反转顶点顺序
                    bool needReverse = glm::dot(normal, toCenter) > 0;
                    
                    // 分解多边形，保持正确的顶点顺序
                    for (size_t j = 1; j < face.size() - 1; ++j) {
                        if (needReverse) {
                            faces.push_back({face[0], face[j + 1], face[j]});
                        } else {
                            faces.push_back({face[0], face[j], face[j + 1]});
                        }
                    }
                }
            }
        }
        
        file.close();
        
        if (!vertices.empty() && !faces.empty()) {
            // 计算顶点法线
            std::vector<glm::vec3> normals(vertices.size(), glm::vec3(0.0f));
            std::vector<int> normalCount(vertices.size(), 0);
            
            // 首先计算每个面的法线并累加到顶点
            for (const auto& face : faces) {
                const glm::vec3& v0 = vertices[face[0]];
                const glm::vec3& v1 = vertices[face[1]];
                const glm::vec3& v2 = vertices[face[2]];
                
                glm::vec3 edge1 = v1 - v0;
                glm::vec3 edge2 = v2 - v0;
                glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));
                
                // 计算面积作为权重
                float weight = glm::length(glm::cross(edge1, edge2)) * 0.5f;
                
                // 累加加权法线
                for (int idx : face) {
                    normals[idx] += faceNormal * weight;
                    normalCount[idx]++;
                }
            }
            
            // 标准化法线
            for (size_t i = 0; i < normals.size(); ++i) {
                if (normalCount[i] > 0) {
                    normals[i] = glm::normalize(normals[i]);
                    
                    // 确保法线朝外
                    glm::vec3 toCenter = glm::normalize(modelCenter - vertices[i]);
                    if (glm::dot(normals[i], toCenter) > 0) {
                        normals[i] = -normals[i];
                    }
                } else {
                    // 孤立顶点使用到中心的方向作为法线
                    normals[i] = glm::normalize(vertices[i] - modelCenter);
                }
            }
            
            rockVertices.push_back(vertices);
            rockFaces.push_back(faces);
            rockNormals.push_back(normals);
            
            // 打印调试信息
            qDebug() << "Rock model" << i << "loaded:";
            qDebug() << "  Vertices:" << vertices.size();
            qDebug() << "  Faces:" << faces.size();
            qDebug() << "  Model center:" << modelCenter.x << modelCenter.y << modelCenter.z;
        }
    }
    
    rockModelsLoaded = !rockVertices.empty();
}

void Obstacle::draw() const
{
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    
    if (type == Type::ROCK) {
        // 保存当前的状态
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        
        float rockScale = size * 50.0f;
        glScalef(rockScale, rockScale * rockHeightScale, rockScale);
        
        float baseAngle = static_cast<float>(rockModelIndex * 72);
        glRotatef(baseAngle, 0.0f, 1.0f, 0.0f);
        
        // 启用光照
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        
        // 启用颜色材质
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        
        // 设置基础颜色
        glColor4f(rockColor.x, rockColor.y, rockColor.z, 1.0f);
        
        // 设置材质属性
        GLfloat matSpecular[] = { rockSpecular.x, rockSpecular.y, rockSpecular.z, 1.0f };
        GLfloat matEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        
        // 只设置高光和自发光属性，让颜色材质处理环境光和漫反射
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, matEmission);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 4.0f);
        
        // 设置光照参数
        GLfloat lightAmbient[] = { 0.6f, 0.6f, 0.6f, 1.0f };
        GLfloat lightDiffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
        GLfloat lightSpecular[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        
        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
        
        // 绘制岩石
        glShadeModel(GL_SMOOTH);
        drawRock();
        glShadeModel(GL_FLAT);
        
        // 恢复所有状态
        glPopAttrib();
        
    } else if (type == Type::SPIKY_SPHERE) {
        glScalef(size * 0.3f, size * 0.3f, size * 0.3f);
        
        // 设置铁质材质属性
        GLfloat metalColor[] = { 0.7f, 0.7f, 0.7f, 1.0f };
        GLfloat metalSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        GLfloat metalAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        GLfloat metalEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        
        glMaterialfv(GL_FRONT, GL_AMBIENT, metalAmbient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, metalColor);
        glMaterialfv(GL_FRONT, GL_SPECULAR, metalSpecular);
        glMaterialf(GL_FRONT, GL_SHININESS, 128.0f);
        glMaterialfv(GL_FRONT, GL_EMISSION, metalEmission);
        
        drawSpikySphere();
    } else {
        glScalef(size, size, size);
        glColor4f(0.8f, 0.4f, 0.0f, 1.0f);
        drawCube();
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
        glLineWidth(2.0f);
        drawCube();
        glLineWidth(1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    glPopMatrix();
}

void Obstacle::drawSpikySphere() const {
    if (!modelLoaded) return;
    
    float scale = size;  // 直接使用size作为缩放因子，因为原模型半径为1
    glPushMatrix();
    glScalef(scale, scale, scale);
    
    glBegin(GL_TRIANGLES);
    for (const auto& face : sphereFaces) {
        for (int idx : face) {
            glNormal3fv(&sphereNormals[idx][0]);
            glVertex3fv(&sphereVertices[idx][0]);
        }
    }
    glEnd();
    
    glPopMatrix();
}

void Obstacle::drawRock() const {
    if (!rockModelsLoaded || rockModelIndex >= rockVertices.size()) {
        return;
    }
    
    const auto& vertices = rockVertices[rockModelIndex];
    const auto& faces = rockFaces[rockModelIndex];
    const auto& normals = rockNormals[rockModelIndex];
    
    // 启用深度测试
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // 启用面剔除
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    glBegin(GL_TRIANGLES);
    for (const auto& face : faces) {
        // 渲染三角形
        for (int i = 0; i < 3; ++i) {
            int idx = face[i];
            if (idx >= 0 && idx < vertices.size() && idx < normals.size()) {
                glNormal3f(normals[idx].x, normals[idx].y, normals[idx].z);
                glVertex3f(vertices[idx].x, vertices[idx].y, vertices[idx].z);
            }
        }
    }
    glEnd();
    
    // 恢复状态
    glDisable(GL_CULL_FACE);
}

bool Obstacle::checkCollision(const glm::vec3& point) const
{
    // 获取点到障碍物中心的向量
    glm::vec3 localPoint = point - position;
    
    switch (type) {
        case Type::ROCK: {
            // 岩石使用椭圆体碰撞检测
            // 注意：渲染时的缩放是由多个因素叠加的
            // 1. 基础缩放 size * 50.0f
            // 2. draw()中的额外缩放
            float rockScale = size * 50.0f * 4.0f;  // 匹配实际渲染大小
            
            // 计算碰撞体积，稍微大于渲染大小以确保安全
            float scaleXZ = rockScale * 1.5f;  // 水平方向增加50%碰撞范围
            float scaleY = rockScale * rockHeightScale * 1.5f;  // 垂直方向保持扁平并增加50%碰撞范围
            
            // 应用Y轴旋转（与渲染时的旋转一致）
            float baseAngle = static_cast<float>(rockModelIndex * 72) * 3.14159f / 180.0f;
            float cosA = cos(baseAngle);
            float sinA = sin(baseAngle);
            
            // 旋转后的坐标
            float rotX = localPoint.x * cosA - localPoint.z * sinA;
            float rotZ = localPoint.x * sinA + localPoint.z * cosA;
            
            // 计算点到椭圆体的距离（考虑旋转）
            float x = rotX / scaleXZ;
            float y = localPoint.y / scaleY;
            float z = rotZ / scaleXZ;
            
            // 使用椭圆体方程检查碰撞，增大判定范围
            return (x*x + y*y + z*z) < 1.2f;  // 增大判定范围
        }
        
        case Type::SPIKY_SPHERE: {
            // 尖刺球使用球形碰撞检测
            // 注意：渲染时有多个缩放因素
            float sphereScale = size * 0.3f * 2.0f;  // 匹配实际渲染大小
            return glm::length(localPoint) < sphereScale * 1.5f;  // 增加50%的碰撞范围
        }
        
        case Type::CUBE: {
            // 立方体使用轴对齐包围盒
            float cubeScale = size * 2.0f;  // 匹配实际渲染大小
            float halfSize = cubeScale * 0.75f;  // 增加50%的碰撞范围
            return (abs(localPoint.x) < halfSize &&
                    abs(localPoint.y) < halfSize &&
                    abs(localPoint.z) < halfSize);
        }
        
        default:
            return false;
    }
}

void Obstacle::drawCube() const
{
    float s = size/2;
    glBegin(GL_QUADS);
    // 前面
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-s, -s, s);
    glVertex3f(s, -s, s);
    glVertex3f(s, s, s);
    glVertex3f(-s, s, s);
    
    // 后面
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-s, -s, -s);
    glVertex3f(-s, s, -s);
    glVertex3f(s, s, -s);
    glVertex3f(s, -s, -s);
    
    // 上面
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-s, s, -s);
    glVertex3f(-s, s, s);
    glVertex3f(s, s, s);
    glVertex3f(s, s, -s);
    
    // 下面
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-s, -s, -s);
    glVertex3f(s, -s, -s);
    glVertex3f(s, -s, s);
    glVertex3f(-s, -s, s);
    
    // 右面
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(s, -s, -s);
    glVertex3f(s, s, -s);
    glVertex3f(s, s, s);
    glVertex3f(s, -s, s);
    
    // 左面
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-s, -s, -s);
    glVertex3f(-s, -s, s);
    glVertex3f(-s, s, s);
    glVertex3f(-s, s, -s);
    
    glEnd();
}