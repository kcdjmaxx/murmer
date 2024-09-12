#define OPENAL_DEPRECATED
#define GL_SILENCE_DEPRECATION

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <vector>
#include <random>
#include <cmath>
#include <iostream>
#include <limits>

// Constants
const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;
const int INITIAL_PARTICLE_COUNT = 5000;
const int MIN_LEAD_BIRDS = 1;
const int MAX_LEAD_BIRDS = 5;
const float MAX_SPEED = 0.01f;
const float SEPARATION_RADIUS = 0.1f;
const float ALIGNMENT_RADIUS = 0.2f;
const float COHESION_RADIUS = 0.3f;

// Structs
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float size;
    glm::vec3 color;
};

struct LeadBird {
    glm::vec3 position;
    glm::vec3 velocity;
    std::vector<glm::vec3> path;
};

// Global variables
std::vector<Particle> particles;
std::vector<LeadBird> leadBirds;
float audioSensitivity = 0.5f;
ALCdevice* device;
ALCcontext* context;
ALCdevice* captureDevice;
std::vector<ALshort> samples;

// Shader variables
GLuint shaderProgram;
GLuint VAO, VBO;

// Shader source code
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    uniform mat4 projection;
    uniform mat4 view;
    void main() {
        gl_Position = projection * view * vec4(aPos.x, aPos.y, aPos.z, 1.0);
        gl_PointSize = 5.0;
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0, 1.0, 1.0, 1.0); // White color
    }
)";

// Function prototypes
void errorCallback(int error, const char* description);
void initializeOpenGL();
void initializeOpenAL();
void initializeShaders();
void setupParticles();
void initializeLeadBirds();
void updateParticles();
void updateLeadBirds();
void render(GLFWwindow* window);
void processInput(GLFWwindow* window);
float getAudioAmplitude();
glm::vec3 generateSmoothPath(float t);
glm::vec3 limitSpeed(const glm::vec3& velocity);

// Error callback function
void errorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void initializeOpenGL() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        exit(-1);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
}

void initializeOpenAL() {
    device = alcOpenDevice(nullptr);
    if (!device) {
        std::cerr << "Failed to open OpenAL device" << std::endl;
        return;
    }

    context = alcCreateContext(device, nullptr);
    if (!context) {
        std::cerr << "Failed to create OpenAL context" << std::endl;
        return;
    }

    if (!alcMakeContextCurrent(context)) {
        std::cerr << "Failed to make OpenAL context current" << std::endl;
        return;
    }

    captureDevice = alcCaptureOpenDevice(nullptr, 44100, AL_FORMAT_MONO16, 1024);
    if (!captureDevice) {
        std::cerr << "Failed to open capture device" << std::endl;
        return;
    }

    alcCaptureStart(captureDevice);

    std::cout << "OpenAL initialized successfully" << std::endl;
}

void initializeShaders() {
    // Vertex Shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    // Fragment Shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "Fragment shader compilation failed:\n" << infoLog << std::endl;
    }

    // Shader Program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void setupParticles() {
    particles.resize(INITIAL_PARTICLE_COUNT);
    for (auto& particle : particles) {
        particle.position = glm::vec3(
            (float)rand() / RAND_MAX * 2.0f - 1.0f,
            (float)rand() / RAND_MAX * 2.0f - 1.0f,
            0.0f
        );
        particle.velocity = glm::vec3(0.0f);
        particle.size = 5.0f;
        particle.color = glm::vec3(1.0f);
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)0);
    glEnableVertexAttribArray(0);
}

void initializeLeadBirds() {
    leadBirds.resize(MIN_LEAD_BIRDS);
    for (auto& bird : leadBirds) {
        bird.position = glm::vec3(
            (float)rand() / RAND_MAX * 2.0f - 1.0f,
            (float)rand() / RAND_MAX * 2.0f - 1.0f,
            0.0f
        );
        bird.velocity = glm::vec3(
            (float)rand() / RAND_MAX * 0.02f - 0.01f,
            (float)rand() / RAND_MAX * 0.02f - 0.01f,
            0.0f
        );
    }
}

glm::vec3 limitSpeed(const glm::vec3& velocity) {
    if (glm::length(velocity) > MAX_SPEED) {
        return glm::normalize(velocity) * MAX_SPEED;
    }
    return velocity;
}

void updateLeadBirds() {
    for (auto& bird : leadBirds) {
        // Update position
        bird.position += bird.velocity;

        // Wrap around screen edges
        if (bird.position.x < -1.0f) bird.position.x = 1.0f;
        if (bird.position.x > 1.0f) bird.position.x = -1.0f;
        if (bird.position.y < -1.0f) bird.position.y = 1.0f;
        if (bird.position.y > 1.0f) bird.position.y = -1.0f;

        // Apply flocking behavior
        glm::vec3 separation(0.0f);
        glm::vec3 alignment(0.0f);
        glm::vec3 cohesion(0.0f);
        int separationCount = 0;
        int alignmentCount = 0;
        int cohesionCount = 0;

        for (const auto& otherBird : leadBirds) {
            if (&bird != &otherBird) {
                float distance = glm::distance(bird.position, otherBird.position);

                if (distance < SEPARATION_RADIUS) {
                    separation += bird.position - otherBird.position;
                    separationCount++;
                }

                if (distance < ALIGNMENT_RADIUS) {
                    alignment += otherBird.velocity;
                    alignmentCount++;
                }

                if (distance < COHESION_RADIUS) {
                    cohesion += otherBird.position;
                    cohesionCount++;
                }
            }
        }

        if (separationCount > 0) {
            separation /= static_cast<float>(separationCount);
            separation = glm::normalize(separation) * MAX_SPEED;
            separation -= bird.velocity;
            separation = limitSpeed(separation);
        }

        if (alignmentCount > 0) {
            alignment /= static_cast<float>(alignmentCount);
            alignment = glm::normalize(alignment) * MAX_SPEED;
            alignment -= bird.velocity;
            alignment = limitSpeed(alignment);
        }

        if (cohesionCount > 0) {
            cohesion /= static_cast<float>(cohesionCount);
            cohesion -= bird.position;
            cohesion = glm::normalize(cohesion) * MAX_SPEED;
            cohesion -= bird.velocity;
            cohesion = limitSpeed(cohesion);
        }

        // Apply flocking forces
        bird.velocity += separation * 1.5f + alignment * 1.0f + cohesion * 1.0f;
        bird.velocity = limitSpeed(bird.velocity);
    }
}

void updateParticles() {
    float amplitude = getAudioAmplitude();
    std::cout << "Audio amplitude: " << amplitude << std::endl;
    
    for (auto& particle : particles) {
        // Find the nearest lead bird
        LeadBird* nearestBird = nullptr;
        float minDistance = std::numeric_limits<float>::max();
        
        for (auto& bird : leadBirds) {
            float distance = glm::distance(particle.position, bird.position);
            if (distance < minDistance) {
                minDistance = distance;
                nearestBird = &bird;
            }
        }

        if (nearestBird) {
            // Move towards the nearest lead bird
            glm::vec3 direction = glm::normalize(nearestBird->position - particle.position);
            particle.velocity += direction * 0.001f;
            
            // Add some randomness
            particle.velocity += glm::vec3(
                (float)rand() / RAND_MAX * 0.002f - 0.001f,
                (float)rand() / RAND_MAX * 0.002f - 0.001f,
                0.0f
            );

            // Limit speed
            particle.velocity = limitSpeed(particle.velocity);

            // Update position
            particle.position += particle.velocity;
        }

        // Wrap around screen edges
        if (particle.position.x < -1.0f) particle.position.x = 1.0f;
        if (particle.position.x > 1.0f) particle.position.x = -1.0f;
        if (particle.position.y < -1.0f) particle.position.y = 1.0f;
        if (particle.position.y > 1.0f) particle.position.y = -1.0f;
    }

    // Update VBO with new particle positions
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_DYNAMIC_DRAW);
}

void render(GLFWwindow* window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    glUseProgram(shaderProgram);

    // Set up view and projection matrices (simple orthographic projection for now)
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    GLuint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLuint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // Draw particles
    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, particles.size());

    // Draw lead birds (as larger points)
    glPointSize(10.0f);
    std::vector<glm::vec3> leadBirdPositions;
    for (const auto& bird : leadBirds)
    for (const auto& bird : leadBirds) {
        leadBirdPositions.push_back(bird.position);
    }
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, leadBirdPositions.size() * sizeof(glm::vec3), leadBirdPositions.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_POINTS, 0, leadBirdPositions.size());
    glPointSize(5.0f);  // Reset point size for particles
}

void processInput(GLFWwindow* window) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

float getAudioAmplitude() {
    ALCint samplesAvailable;
    alcGetIntegerv(captureDevice, ALC_CAPTURE_SAMPLES, 1, &samplesAvailable);
    
    if (samplesAvailable > 0) {
        samples.resize(samplesAvailable);
        alcCaptureSamples(captureDevice, samples.data(), samplesAvailable);
        
        float sum = 0.0f;
        for (ALshort sample : samples) {
            sum += std::abs(static_cast<float>(sample) / 32768.0f);
        }
        return sum / samplesAvailable;
    }
    return 0.0f;
}

glm::vec3 generateSmoothPath(float t) {
    // Path generation code here (to be implemented)
    return glm::vec3(0.0f);
}

int main() {
    std::cout << "Starting Bird Murmuration program..." << std::endl;

    glfwSetErrorCallback(errorCallback);

    std::cout << "Initializing OpenGL..." << std::endl;
    initializeOpenGL();
    
    std::cout << "Creating window..." << std::endl;
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Bird Murmuration", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    std::cout << "Initializing OpenAL..." << std::endl;
    initializeOpenAL();

    std::cout << "Initializing shaders..." << std::endl;
    initializeShaders();

    std::cout << "Setting up particles..." << std::endl;
    setupParticles();

    std::cout << "Initializing lead birds..." << std::endl;
    initializeLeadBirds();

    std::cout << "Entering main loop..." << std::endl;
    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        updateLeadBirds();
        updateParticles();
        render(window);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    alcCaptureStop(captureDevice);
    alcCaptureCloseDevice(captureDevice);
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
    alcCloseDevice(device);

    std::cout << "Shutting down..." << std::endl;
    glfwTerminate();
    return 0;
}