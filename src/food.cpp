#include "food.h"
#include <GL/glew.h>
#include <cmath>

Food::Food()
    : position(0.0f)
    , size(DEFAULT_SIZE)
{
}

Food::Food(const glm::vec3& pos)
    : position(pos)
    , size(DEFAULT_SIZE)
{
}

void Food::draw() const 
{
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    glColor3f(1.0f, 0.8f, 0.0f);  // 金黄色
    drawSphere(size, 16, 16);
    glPopMatrix();
}

void Food::drawSphere(float radius, int sectors, int stacks) const
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