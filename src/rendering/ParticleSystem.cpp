#include "ParticleSystem.hpp"
#include "AssetPaths.hpp"
#include <iostream>
#include <algorithm>
#include <random>


ParticleSystem::ParticleSystem() {
    init();
}

ParticleSystem::~ParticleSystem() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
}

void ParticleSystem::init() {
    initShaders();
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    
    // Enable point size control in vertex shader
    glEnable(GL_PROGRAM_POINT_SIZE);
}

void ParticleSystem::initShaders() {
    m_shaderProgram.load(
        AssetPaths::shader("particles.vert"),
        AssetPaths::shader("particles.frag"));
}

void ParticleSystem::update(float deltaTime, bool beatDetected) {
    if (beatDetected) {
        spawnParticles(20); 
    }

    for (auto it = m_particles.begin(); it != m_particles.end();) {
        it->life -= deltaTime;
        if (it->life <= 0.0f) {
            it = m_particles.erase(it);
        } else {
            it->position += it->velocity * deltaTime;
            // Fade out
            it->color.a = it->life; 
            ++it;
        }
    }
}

void ParticleSystem::spawnParticles(int count) {
    if (m_particles.size() >= (size_t)m_maxParticles) return;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);
    std::uniform_real_distribution<> lifeDis(0.5, 1.5);

    for (int i = 0; i < count; ++i) {
        Particle p;
        p.position = glm::vec2(0.0f, -0.5f); // Start near bottom center
        
        // Random velocity direction
        float angle = (dis(gen) + 1.0f) * 3.14159f; // Full circle
        float speed = (std::abs(dis(gen)) + 0.5f) * m_speedMultiplier * 0.5f; // Reduced base speed
        
        p.velocity = glm::vec2(cos(angle), sin(angle)) * speed * m_spread;
        p.color = m_color;
        p.life = lifeDis(gen);
        p.size = m_particleSize * (0.5f + std::abs(dis(gen)));
        
        m_particles.push_back(p);
    }
}

void ParticleSystem::render() {
    if (m_particles.empty()) return;

    std::vector<float> vertices;
    for (const auto& p : m_particles) {
        vertices.push_back(p.position.x);
        vertices.push_back(p.position.y);
        vertices.push_back(p.color.r);
        vertices.push_back(p.color.g);
        vertices.push_back(p.color.b);
        vertices.push_back(p.color.a);
        vertices.push_back(p.size);
    }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STREAM_DRAW);

    // Position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Size
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // Ensure no z-fighting
    
    glUseProgram(m_shaderProgram);
    glDrawArrays(GL_POINTS, 0, m_particles.size());
    
    glDisable(GL_PROGRAM_POINT_SIZE);
}
