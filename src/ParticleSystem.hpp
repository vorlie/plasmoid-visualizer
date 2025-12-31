#ifndef PARTICLESYSTEM_HPP
#define PARTICLESYSTEM_HPP

#include <GL/glew.h>
#include <vector>
#include <glm/glm.hpp>

struct Particle {
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec4 color;
    float life;
    float size;
};

class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    void init();
    void update(float deltaTime, bool beatDetected);
    void render();

    // Configuration
    void setParticleCount(int count) { m_maxParticles = count; }
    void setSpeed(float speed) { m_speedMultiplier = speed; }
    void setSize(float size) { m_particleSize = size; }
    void setColor(float r, float g, float b, float a) { m_color = glm::vec4(r, g, b, a); }
    void setSpread(float spread) { m_spread = spread; }

private:
    std::vector<Particle> m_particles;
    GLuint m_vao, m_vbo;
    GLuint m_shaderProgram;

    // Settings
    int m_maxParticles = 500;
    float m_speedMultiplier = 1.0f;
    float m_particleSize = 5.0f;
    glm::vec4 m_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float m_spread = 1.0f;

    void initShaders();
    void spawnParticles(int count);
};

#endif // PARTICLESYSTEM_HPP
