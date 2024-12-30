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
    void setProjectionMatrix(const glm::mat4& proj) { projectionMatrix = proj; }
    void setViewMatrix(const glm::mat4& view) { viewMatrix = view; }

private:
    void drawDorsalFin(const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& up, float size);
    void setGradientColor(float t) const;
    bool isSegmentInFrustum(const glm::vec3& position, float radius) const;
    void extractFrustumPlanes();
    
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
    
    static constexpr float FIN_HEIGHT_RATIO = 0.6f;
    static constexpr float FIN_LENGTH_RATIO = 0.8f;
    
    static constexpr float GRADIENT_TOP_R = 0.2f;
    static constexpr float GRADIENT_TOP_G = 0.8f;
    static constexpr float GRADIENT_TOP_B = 0.2f;
    static constexpr float GRADIENT_BOTTOM_R = 0.1f;
    static constexpr float GRADIENT_BOTTOM_G = 0.5f;
    static constexpr float GRADIENT_BOTTOM_B = 0.1f;
    
    void updateDirections();

    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    glm::vec4 frustumPlanes[6];
    bool frustumPlanesUpdated;
};

#endif // SNAKE_H