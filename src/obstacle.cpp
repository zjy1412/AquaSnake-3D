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

// 在cpp文件中实现构造函数
Obstacle::Obstacle(const glm::vec3& pos, float size) 
    : position(pos)
    , size(size) 
{
    // 随机选择障碍物类型
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 1);
    
    // 如果模型还未加载，先尝试加载
    if (!modelLoaded) {
        loadSphereModel();
    }
    
    // 如果模型加载成功，则随机选择类型；否则只能使用立方体
    if (modelLoaded) {
        type = dis(gen) == 0 ? Type::CUBE : Type::SPIKY_SPHERE;
    } else {
        type = Type::CUBE;
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

void Obstacle::draw() const
{
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    
    if (type == Type::SPIKY_SPHERE) {
        // 设置铁质材质属性
        GLfloat metalColor[] = { 0.7f, 0.7f, 0.7f, 1.0f };  // 灰色基础色
        GLfloat metalSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };  // 高光颜色
        GLfloat metalAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };  // 环境光
        GLfloat metalEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f }; // 自发光
        
        glMaterialfv(GL_FRONT, GL_AMBIENT, metalAmbient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, metalColor);
        glMaterialfv(GL_FRONT, GL_SPECULAR, metalSpecular);
        glMaterialf(GL_FRONT, GL_SHININESS, 128.0f);  // 高光度
        glMaterialfv(GL_FRONT, GL_EMISSION, metalEmission);
        
        drawSpikySphere();
    } else {
        glColor4f(0.8f, 0.4f, 0.0f, 1.0f);  // 橙色
        drawCube();
        // 绘制轮廓
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);  // 黑色轮廓
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

bool Obstacle::checkCollision(const glm::vec3& point) const
{
    if (type == Type::SPIKY_SPHERE) {
        // 对于尖刺球，使用球形碰撞检测
        return glm::length(point - position) < size * 0.8f;  // 使用略小的碰撞范围
    } else {
        // 立方体的碰撞检测保持不变
        return (point.x >= position.x - size/2 && point.x <= position.x + size/2 &&
                point.y >= position.y - size/2 && point.y <= position.y + size/2 &&
                point.z >= position.z - size/2 && point.z <= position.z + size/2);
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