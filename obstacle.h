#ifndef OBSTACLE_H
#define OBSTACLE_H

#include <glm/glm.hpp>
#include <vector>

class Obstacle {
public:
    // 只声明构造函数，不在此定义
    Obstacle(const glm::vec3& pos, float size);
    
    void draw() const;
    bool checkCollision(const glm::vec3& point) const;
    glm::vec3 getPosition() const { return position; }
    float getRadius() const { return size; }

private:
    glm::vec3 position;
    float size;
    void drawCube() const;
};

#endif // OBSTACLE_H