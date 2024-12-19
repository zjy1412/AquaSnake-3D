#ifndef SNAKE_H
#define SNAKE_H

#define GLM_ENABLE_EXPERIMENTAL

#include <GL/glew.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <QOpenGLFunctions>

class Snake : protected QOpenGLFunctions {
public:
    Snake(float startX = 0.0f, float startY = 0.0f, float startZ = 0.0f);
    void initializeGL();
    void move();
    void grow();
    void setDirection(const glm::vec3& newDir);
    bool checkCollision(const glm::vec3& point) const;
    void draw();
    void drawSphere(float radius, int sectors, int stacks);
    glm::vec3 getHeadPosition() const { return body.front(); }
    const std::vector<glm::vec3>& getBody() const { return body; }
    glm::vec3 getDirection() const { return direction; }
    bool checkSelfCollision() const;
    float getMovementSpeed() const { return moveSpeed; }
    static constexpr float GROWTH_FACTOR = 3;
    float getSegmentSize() const { return DEFAULT_SEGMENT_SIZE; }
    glm::vec3 getUpDirection() const { return upDirection; }
    glm::vec3 getRightDirection() const { return glm::normalize(glm::cross(direction, upDirection)); }
    void rotateAroundAxis(const glm::vec3& axis, float angle);

private:
    std::vector<glm::vec3> body;
    glm::vec3 direction;
    glm::vec3 targetDirection;
    float segmentSize;
    float moveSpeed;
    static constexpr float DEFAULT_SEGMENT_SIZE = 50.0f;
    static constexpr float DEFAULT_MOVE_SPEED = 10.0f;
    static constexpr float TURN_SPEED = 0.2f;
    static constexpr float MIN_DIRECTION_CHANGE = 0.05f;
    static constexpr float MAX_TURN_ANGLE = 90.0f;
    glm::vec3 upDirection;
    void updateDirections();
};

#endif // SNAKE_H