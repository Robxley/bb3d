#include <iostream>
#include <glm/gtc/noise.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <iomanip>

/**
 * @brief Unit test for GLM noise functionality.
 * Verifies that Perlin and Simplex noise functions are available and producing values.
 */
int main() {
    std::cout << "--- GLM Noise Verification ---" << std::endl;

    // Test Perlin noise (3D)
    glm::vec3 p1(0.5f, 0.5f, 0.5f);
    float perlinValue = glm::perlin(p1);
    std::cout << "Perlin(0.5, 0.5, 0.5): " << perlinValue << std::endl;

    // Test Simplex noise (3D)
    float simplexValue = glm::simplex(p1);
    std::cout << "Simplex(0.5, 0.5, 0.5): " << simplexValue << std::endl;

    // Test periodicity/continuity (basic check)
    glm::vec3 p2(0.51f, 0.51f, 0.51f);
    float perlinValue2 = glm::perlin(p2);
    float delta = std::abs(perlinValue - perlinValue2);
    
    std::cout << "Perlin(0.51, 0.51, 0.51): " << perlinValue2 << std::endl;
    std::cout << "Delta: " << delta << std::endl;

    if (delta < 0.1f) {
        std::cout << "SUCCESS: Noise seems continuous." << std::endl;
        return 0;
    } else {
        std::cout << "WARNING: Large delta in noise values." << std::endl;
        return 1;
    }
}
