#define GLM_ENABLE_EXPERIMENTAL
#include <GL/glew.h>
#include <QOpenGLFunctions>
#include "snake.h"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

Snake::Snake(float x, float y, float z)
    : segmentSize(DEFAULT_SEGMENT_SIZE)
    , moveSpeed(DEFAULT_MOVE_SPEED)
    , direction(1.0f, 0.0f, 0.0f)
    , targetDirection(direction)
    , upDirection(0.0f, 1.0f, 0.0f)
    , projectionMatrix(1.0f)
    , viewMatrix(1.0f)
    , frustumPlanesUpdated(false)
{
    // 设置初始位置
    glm::vec3 initialPos(x, y, z);
    
    // 设置初始长度为3段
    const int INITIAL_LENGTH = 3;
    for(int i = 0; i < INITIAL_LENGTH; ++i) {
        // 每一段都在前一段的后面，沿着-X方向排列
        glm::vec3 segmentPos = initialPos - glm::vec3(i * segmentSize, 0.0f, 0.0f);
        body.push_back(segmentPos);
    }
}

void Snake::initializeGL()
{
    initializeOpenGLFunctions();
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
    // 显著增加忽略的节数以避免误判
    const size_t IGNORE_SEGMENTS = 15;  // 增加忽略的节数
    
    if(body.size() <= IGNORE_SEGMENTS) return false;
    
    const glm::vec3& head = body[0];
    
    // 采用渐进式判定：距离头部越远的段，碰撞范围越大
    for(size_t i = IGNORE_SEGMENTS; i < body.size(); ++i) {
        float distance = glm::length(head - body[i]);
        float collisionThreshold = segmentSize * (0.5f + static_cast<float>(i) / body.size() * 0.3f);
        
        if(distance < collisionThreshold) {
            return true;
        }
    }
    return false;
}

void Snake::setGradientColor(float t) const {
    // t 是从0到1的参数，0代表底部，1代表顶部
    float r = GRADIENT_BOTTOM_R + (GRADIENT_TOP_R - GRADIENT_BOTTOM_R) * t;
    float g = GRADIENT_BOTTOM_G + (GRADIENT_TOP_G - GRADIENT_BOTTOM_G) * t;
    float b = GRADIENT_BOTTOM_B + (GRADIENT_TOP_B - GRADIENT_BOTTOM_B) * t;
    glColor3f(r, g, b);
}

void Snake::drawDorsalFin(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& up, float size) {
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    
    // 计算背鳍的基准点（从蛇身体表面开始）
    glm::vec3 finBase = pos + up * size;  // 从球体表面开始
    
    // 计算背鳍的顶点
    glm::vec3 finTop = finBase + up * (size * FIN_HEIGHT_RATIO);  // 从表面向上延伸
    glm::vec3 finFrontBase = finBase + dir * (size * FIN_LENGTH_RATIO * 0.5f);
    glm::vec3 finBackBase = finBase - dir * (size * FIN_LENGTH_RATIO * 0.5f);
    
    // 设置背鳍材质
    GLfloat finSpecular[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat finShininess[] = { 32.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, finSpecular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, finShininess);
    
    // 绘制背鳍（三角形）
    glBegin(GL_TRIANGLES);
    
    // 正面
    setGradientColor(1.0f);  // 顶部颜色
    glVertex3fv(glm::value_ptr(finTop));
    setGradientColor(0.0f);  // 底部颜色
    glVertex3fv(glm::value_ptr(finFrontBase));
    glVertex3fv(glm::value_ptr(finBackBase));
    
    // 背面（反向绘制以确保双面可见）
    setGradientColor(1.0f);
    glVertex3fv(glm::value_ptr(finTop));
    setGradientColor(0.0f);
    glVertex3fv(glm::value_ptr(finBackBase));
    glVertex3fv(glm::value_ptr(finFrontBase));
    
    glEnd();
}

void Snake::extractFrustumPlanes() {
    // 计算视图投影矩阵
    glm::mat4 viewProj = projectionMatrix * viewMatrix;
    
    // 提取左平面
    frustumPlanes[0].x = viewProj[0][3] + viewProj[0][0];
    frustumPlanes[0].y = viewProj[1][3] + viewProj[1][0];
    frustumPlanes[0].z = viewProj[2][3] + viewProj[2][0];
    frustumPlanes[0].w = viewProj[3][3] + viewProj[3][0];

    // 提取右平面
    frustumPlanes[1].x = viewProj[0][3] - viewProj[0][0];
    frustumPlanes[1].y = viewProj[1][3] - viewProj[1][0];
    frustumPlanes[1].z = viewProj[2][3] - viewProj[2][0];
    frustumPlanes[1].w = viewProj[3][3] - viewProj[3][0];

    // 提取底平面
    frustumPlanes[2].x = viewProj[0][3] + viewProj[0][1];
    frustumPlanes[2].y = viewProj[1][3] + viewProj[1][1];
    frustumPlanes[2].z = viewProj[2][3] + viewProj[2][1];
    frustumPlanes[2].w = viewProj[3][3] + viewProj[3][1];

    // 提取顶平面
    frustumPlanes[3].x = viewProj[0][3] - viewProj[0][1];
    frustumPlanes[3].y = viewProj[1][3] - viewProj[1][1];
    frustumPlanes[3].z = viewProj[2][3] - viewProj[2][1];
    frustumPlanes[3].w = viewProj[3][3] - viewProj[3][1];

    // 提取近平面
    frustumPlanes[4].x = viewProj[0][3] + viewProj[0][2];
    frustumPlanes[4].y = viewProj[1][3] + viewProj[1][2];
    frustumPlanes[4].z = viewProj[2][3] + viewProj[2][2];
    frustumPlanes[4].w = viewProj[3][3] + viewProj[3][2];

    // 提取远平面
    frustumPlanes[5].x = viewProj[0][3] - viewProj[0][2];
    frustumPlanes[5].y = viewProj[1][3] - viewProj[1][2];
    frustumPlanes[5].z = viewProj[2][3] - viewProj[2][2];
    frustumPlanes[5].w = viewProj[3][3] - viewProj[3][2];

    // 规范化所有平面
    for(int i = 0; i < 6; ++i) {
        float length = sqrt(frustumPlanes[i].x * frustumPlanes[i].x +
                          frustumPlanes[i].y * frustumPlanes[i].y +
                          frustumPlanes[i].z * frustumPlanes[i].z);
        frustumPlanes[i] /= length;
    }

    frustumPlanesUpdated = true;
}

bool Snake::isSegmentInFrustum(const glm::vec3& position, float radius) const {
    // 对于每个平面
    for(int i = 0; i < 6; ++i) {
        float distance = frustumPlanes[i].x * position.x +
                        frustumPlanes[i].y * position.y +
                        frustumPlanes[i].z * position.z +
                        frustumPlanes[i].w;
        
        // 如果球体完全在平面的负面，则它在视锥体外
        if(distance < -radius) {
            return false;
        }
    }
    return true;
}

void Snake::draw() {
    // 如果视锥体平面需要更新，则更新它们
    if(!frustumPlanesUpdated) {
        extractFrustumPlanes();
    }

    // 保存当前的OpenGL状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();

    // 设置材质属性
    GLfloat matSpecular[] = { 0.6f, 0.8f, 0.6f, 1.0f };
    GLfloat matShininess[] = { 48.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, matShininess);
    
    // 绘制蛇的每个段
    for(size_t i = 0; i < body.size(); ++i) {
        // 进行视锥体剔除测试
        float segmentRadius = (i == 0) ? segmentSize * 1.3f : segmentSize * 1.1f;
        if(!isSegmentInFrustum(body[i], segmentRadius)) {
            continue; // 如果这个段不在视锥体内，跳过它的渲染
        }

        glPushMatrix();
        glTranslatef(body[i].x, body[i].y, body[i].z);
        
        // 计算当前段的方向
        glm::vec3 segmentDir = (i < body.size() - 1) ? 
            glm::normalize(body[i+1] - body[i]) : 
            glm::normalize(body[i] - body[i-1]);
            
        if(i == 0) {
            // 蛇头
            setGradientColor(1.0f);
            drawSphere(segmentSize * 1.3f, 24, 24);
            drawDorsalFin(glm::vec3(0), direction, upDirection, segmentSize * 1.4f);
        } else {
            // 蛇身
            float t = 1.0f - (float)i / body.size() * 0.3f;
            setGradientColor(t);
            drawSphere(segmentSize * 1.1f, 20, 20);
            
            if(i % 2 == 0) {
                drawDorsalFin(glm::vec3(0), segmentDir, upDirection, segmentSize * 0.9f);
            }
        }
        
        glPopMatrix();
    }
    
    // 恢复OpenGL状态
    glPopMatrix();
    glPopAttrib();

    // 标记视锥体平面需要在下一帧更新
    frustumPlanesUpdated = false;
}

void Snake::drawSphere(float radius, int sectors, int stacks)  // 移除 const 限定符
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