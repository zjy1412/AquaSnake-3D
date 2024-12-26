#ifndef OBSTACLE_H
#define OBSTACLE_H

#include <glm/glm.hpp>
#include <vector>

class Obstacle {
public:
    enum class Type {
        CUBE,
        SPIKY_SPHERE
    };
    
    // 构造函数
    Obstacle(const glm::vec3& pos, float size);
    
    void draw() const;
    bool checkCollision(const glm::vec3& point) const;
    glm::vec3 getPosition() const { return position; }
    float getRadius() const { return size; }

private:
    glm::vec3 position;
    float size;
    Type type;  // 障碍物类型
    
    // 绘制不同类型的障碍物
    void drawCube() const;
    void drawSpikySphere() const;
    
    // OBJ模型数据
    static std::vector<glm::vec3> sphereVertices;
    static std::vector<glm::vec3> sphereNormals;
    static std::vector<std::vector<int>> sphereFaces;
    static bool modelLoaded;
    
    // 加载OBJ模型
    static void loadSphereModel();
};

#endif // OBSTACLE_H