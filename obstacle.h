#ifndef OBSTACLE_H
#define OBSTACLE_H

#include <glm/glm.hpp>
#include <vector>

class Obstacle {
public:
    enum class Type {
        CUBE,
        SPIKY_SPHERE,
        ROCK
    };
    
    // 静态常量成员
    static const int MAX_ROCKS = 15;           // 岩石数量固定为15个
    static const int MAX_OTHER_OBSTACLES = 80;  // 其他障碍物总共80个
    
    // 构造函数
    Obstacle(const glm::vec3& pos, float size, Type type = Type::CUBE);
    
    void draw() const;
    bool checkCollision(const glm::vec3& point) const;
    glm::vec3 getPosition() const { return position; }
    float getRadius() const { return size; }
    Type getType() const { return type; }

private:
    glm::vec3 position;
    float size;
    Type type;  // 障碍物类型
    int rockModelIndex;  // 添加实例特定的岩石模型索引
    float rockHeightScale;  // 岩石高度的随机缩放因子
    
    // 岩石的随机颜色
    glm::vec3 rockColor;      // 基础颜色
    glm::vec3 rockSpecular;   // 高光颜色
    glm::vec3 rockAmbient;    // 环境光颜色
    
    // 绘制不同类型的障碍物
    void drawCube() const;
    void drawSpikySphere() const;
    void drawRock() const;
    
    // OBJ模型数据
    static std::vector<glm::vec3> sphereVertices;
    static std::vector<glm::vec3> sphereNormals;
    static std::vector<std::vector<int>> sphereFaces;
    static bool modelLoaded;
    
    // 岩石模型数据
    static std::vector<std::vector<glm::vec3>> rockVertices;  // 每个岩石模型的顶点
    static std::vector<std::vector<std::vector<int>>> rockFaces;  // 每个岩石模型的面
    static std::vector<std::vector<glm::vec3>> rockNormals;  // 每个岩石模型的法线
    static bool rockModelsLoaded;  // 岩石模型是否已加载
    
    // 加载模型
    static void loadSphereModel();
    static void loadRockModels();  // 加载岩石模型

    const float SEGMENT_SIZE = 100.0f;
    const float MIN_FOOD_DISTANCE = 400.0f;
};

#endif // OBSTACLE_H