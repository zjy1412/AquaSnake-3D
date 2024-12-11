#ifndef SNAKE_H
#define SNAKE_H

#include <vector>
#include <glm/glm.hpp>

class Snake {
public:
    Snake(float startX = 0.0f, float startY = 0.0f, float startZ = 0.0f);
    
    void move();
    void grow();
    void setDirection(const glm::vec3& newDir);
    bool checkCollision(const glm::vec3& point) const;
    void draw() const;
    void drawSphere(float radius, int sectors, int stacks) const;  // 移到public
    glm::vec3 getHeadPosition() const { return body.front(); }
    const std::vector<glm::vec3>& getBody() const { return body; }
    glm::vec3 getDirection() const { return direction; }  // 添加这行
    bool checkSelfCollision() const;  // 移到 public 区域
    float getMovementSpeed() const { return moveSpeed; }  // 添加 getter
    static constexpr float GROWTH_FACTOR = 3;  // 移到 public 区域
    float getSegmentSize() const { return DEFAULT_SEGMENT_SIZE; }  // 添加 getter

private:
    std::vector<glm::vec3> body;
    glm::vec3 direction;
    glm::vec3 targetDirection;  // 目标方向
    float segmentSize;
    float moveSpeed;
    static constexpr float DEFAULT_SEGMENT_SIZE = 100.0f;   // 再次增大蛇身大小
    static constexpr float DEFAULT_MOVE_SPEED = 8.0f;    // 降低移动速度
    static constexpr float TURN_SPEED = 0.3f;   // 加快转向速度
    static constexpr float MIN_DIRECTION_CHANGE = 0.05f; // 降低最小方向变化阈值
    static constexpr float MAX_TURN_ANGLE = 90.0f; // 最大转向角度
};

#endif // SNAKE_H