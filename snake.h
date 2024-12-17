#ifndef SNAKE_H
#define SNAKE_H

#define GLM_ENABLE_EXPERIMENTAL  // 添加这行启用实验性特性

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>  // 添加四元数支持
#include <glm/gtc/quaternion.hpp>  // 添加四元数支持
#include <glm/gtx/rotate_vector.hpp>  // 添加向量旋转支持
#include <glm/gtc/matrix_transform.hpp>
#include <QOpenGLFunctions>

class Snake : protected QOpenGLFunctions {
public:
    Snake(float startX = 0.0f, float startY = 0.0f, float startZ = 0.0f);
    void initializeGL();  // 添加这个方法
    void move();
    void grow();
    void setDirection(const glm::vec3& newDir);
    bool checkCollision(const glm::vec3& point) const;
    void draw();  // 移除 const 限定符
    void drawSphere(float radius, int sectors, int stacks);  // 移除 const 限定符
    glm::vec3 getHeadPosition() const { return body.front(); }
    const std::vector<glm::vec3>& getBody() const { return body; }
    glm::vec3 getDirection() const { return direction; }  // 添加这行
    bool checkSelfCollision() const;  // 移到 public 区域
    float getMovementSpeed() const { return moveSpeed; }  // 添加 getter
    static constexpr float GROWTH_FACTOR = 3;  // 移到 public 区域
    float getSegmentSize() const { return DEFAULT_SEGMENT_SIZE; }  // 添加 getter
    glm::vec3 getUpDirection() const { return upDirection; }
    glm::vec3 getRightDirection() const { return glm::normalize(glm::cross(direction, upDirection)); }
    void rotateAroundAxis(const glm::vec3& axis, float angle);

private:
    std::vector<glm::vec3> body;
    glm::vec3 direction;
    glm::vec3 targetDirection;  // 目标方向
    float segmentSize;
    float moveSpeed;
    static constexpr float DEFAULT_SEGMENT_SIZE = 50.0f;   // 再次增大蛇身大小
    static constexpr float DEFAULT_MOVE_SPEED = 10.0f;    // 降低移动速度
    static constexpr float TURN_SPEED = 0.2f;   // 加快转向速度
    static constexpr float MIN_DIRECTION_CHANGE = 0.05f; // 降低最小方向变化阈值
    static constexpr float MAX_TURN_ANGLE = 90.0f; // 最大转向角度
    glm::vec3 upDirection;      // 蛇的上方向
    void updateDirections();     // 更新方向向量
};

#endif // SNAKE_H