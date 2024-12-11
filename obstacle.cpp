#include "obstacle.h"
#include <GL/glew.h>

// 在cpp文件中实现构造函数
Obstacle::Obstacle(const glm::vec3& pos, float size) 
    : position(pos)
    , size(size) 
{
}

void Obstacle::draw() const
{
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    glColor4f(0.8f, 0.4f, 0.0f, 1.0f);  // 更明亮的橙色
    drawCube();
    // 绘制轮廓
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);  // 黑色轮廓
    glLineWidth(2.0f);
    drawCube();
    glLineWidth(1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();
}

bool Obstacle::checkCollision(const glm::vec3& point) const
{
    return (point.x >= position.x - size/2 && point.x <= position.x + size/2 &&
            point.y >= position.y - size/2 && point.y <= position.y + size/2 &&
            point.z >= position.z - size/2 && point.z <= position.z + size/2);
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