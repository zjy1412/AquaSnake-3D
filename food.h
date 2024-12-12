
#ifndef FOOD_H
#define FOOD_H

#include <glm/glm.hpp>
#include <vector>

class Food {
public:
    Food(const glm::vec3& pos);
    
    void draw() const;
    glm::vec3 getPosition() const { return position; }
    static constexpr float DEFAULT_SIZE = 60.0f;  // 食物默认大小
    float getSize() const { return size; }
    void drawSphere(float radius, int sectors, int stacks) const;

private:
    glm::vec3 position;
    float size;
};

#endif // FOOD_H