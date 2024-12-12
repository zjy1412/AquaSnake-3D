#include <GL/glew.h>
#include <QOpenGLFunctions>
#include "snake.h"
#include <cmath>
#include <glm/glm.hpp>

Snake::Snake(float startX, float startY, float startZ)
    : direction(1.0f, 0.0f, 0.0f)
    , targetDirection(1.0f, 0.0f, 0.0f)
    , upDirection(0.0f, 1.0f, 0.0f)  // 初始上方向为世界空间的上
    , segmentSize(DEFAULT_SEGMENT_SIZE)
    , moveSpeed(DEFAULT_MOVE_SPEED)
{
    // 使用传入的参数设置起始位置
    glm::vec3 startPos(startX, startY, startZ);
    body.push_back(startPos);

    // 向左延伸初始身体，但长度更短
    for(int i = 1; i < 3; ++i) {
        body.push_back(startPos - glm::vec3(i * segmentSize, 0.0f, 0.0f));
    }
}

void Snake::move()
{
    // 平滑转向，但速度更快
    if (glm::length(targetDirection - direction) > 0.01f) {
        direction = glm::normalize(
            glm::mix(direction, targetDirection, TURN_SPEED)
        );
        updateDirections();
    }

    // 更新蛇的位置
    glm::vec3 newHead = body.front() + direction * moveSpeed;
    
    // 移动蛇身（每个段都跟随前一个段）
    for(size_t i = body.size() - 1; i > 0; --i) {
        body[i] = body[i-1];
    }
    body[0] = newHead;

    // 检查自身碰撞
    if(checkSelfCollision()) {
        // 通知游戏结束
        // 这里需要添加一个回调机制
    }
}

void Snake::grow()
{
    // 在尾部添加新段，位置与最后一段相同
    if(!body.empty()) {
        body.push_back(body.back());
    }
}

void Snake::setDirection(const glm::vec3& newDir)
{
    if (glm::length(newDir) < 0.01f) return;

    // 保存旧的方向用于平滑过渡
    glm::vec3 oldDirection = direction;
    
    // 规范化新方向
    targetDirection = glm::normalize(newDir);
    
    // 使用glm::rotate函数代替四元数直接操作
    float angle = glm::acos(glm::dot(glm::normalize(oldDirection), targetDirection));
    if (angle > 0.01f) {  // 确保有足够的角度差异
        glm::vec3 rotationAxis = glm::cross(oldDirection, targetDirection);
        if (glm::length(rotationAxis) > 0.01f) {  // 确保旋转轴有效
            upDirection = glm::rotate(upDirection, angle, glm::normalize(rotationAxis));
        }
    }
}

void Snake::rotateAroundAxis(const glm::vec3& axis, float angle)
{
    // 使用glm::rotate函数进行旋转
    glm::vec3 normalizedAxis = glm::normalize(axis);
    direction = glm::rotate(direction, angle, normalizedAxis);
    upDirection = glm::rotate(upDirection, angle, normalizedAxis);
    targetDirection = direction;
}

void Snake::updateDirections()
{
    // 确保direction和upDirection保持垂直
    upDirection = glm::normalize(upDirection - direction * glm::dot(direction, upDirection));
}

bool Snake::checkCollision(const glm::vec3& point) const
{
    for(const glm::vec3& segment : body) {
        float distance = glm::length(segment - point);
        if(distance < segmentSize * 1.5f) {  // 增加碰撞检测的容差
            return true;
        }
    }
    return false;
}

bool Snake::checkSelfCollision() const
{
    // 显著增加忽略的节数，避免误判
    const size_t IGNORE_SEGMENTS = 15;  // 增加忽略的节数
    
    if(body.size() <= IGNORE_SEGMENTS) return false;
    
    const glm::vec3& head = body[0];
    
    // 使用渐进式判定：距离头部越远的段，碰撞范围越大
    for(size_t i = IGNORE_SEGMENTS; i < body.size(); ++i) {
        float distance = glm::length(head - body[i]);
        float collisionThreshold = segmentSize * (0.5f + static_cast<float>(i) / body.size() * 0.3f);
        
        if(distance < collisionThreshold) {
            return true;
        }
    }
    return false;
}

void Snake::draw() const
{
    for(size_t i = 0; i < body.size(); ++i) {
        glPushMatrix();
        glTranslatef(body[i].x, body[i].y, body[i].z);
        
        if(i == 0) {
            // 红色蛇头，比身体大一些
            glColor3f(1.0f, 0.2f, 0.2f);
            drawSphere(segmentSize * 1.2f, 20, 20);  // 使用 segmentSize
        } else {
            // 渐变的绿色蛇身
            float greenIntensity = 1.0f - (float)i / body.size() * 0.3f;
            glColor3f(0.2f, greenIntensity, 0.2f);
            drawSphere(segmentSize, 16, 16);  // 使用 segmentSize
        }
        glPopMatrix();
    }
}

void Snake::drawSphere(float radius, int sectors, int stacks) const
{
    const float PI = 3.14159265359f;
    
    glBegin(GL_TRIANGLES);
    for(int i = 0; i < stacks; ++i) {
        float phi1 = PI * float(i) / float(stacks);
        float phi2 = PI * float(i + 1) / float(stacks);
        
        for(int j = 0; j < sectors; ++j) {
            float theta1 = 2.0f * PI * float(j) / float(sectors);
            float theta2 = 2.0f * PI * float(j + 1) / float(sectors);
            
            // 第一个三角形
            glVertex3f(
                radius * sin(phi1) * cos(theta1),
                radius * cos(phi1),
                radius * sin(phi1) * sin(theta1)
            );
            glVertex3f(
                radius * sin(phi2) * cos(theta1),
                radius * cos(phi2),
                radius * sin(phi2) * sin(theta1)
            );
            glVertex3f(
                radius * sin(phi2) * cos(theta2),
                radius * cos(phi2),
                radius * sin(phi2) * sin(theta2)
            );
            
            // 第二个三角形
            glVertex3f(
                radius * sin(phi1) * cos(theta1),
                radius * cos(phi1),
                radius * sin(phi1) * sin(theta1)
            );
            glVertex3f(
                radius * sin(phi2) * cos(theta2),
                radius * cos(phi2),
                radius * sin(phi2) * sin(theta2)
            );
            glVertex3f(
                radius * sin(phi1) * cos(theta2),
                radius * cos(phi1),
                radius * sin(phi1) * sin(theta2)
            );
        }
    }
    glEnd();
}