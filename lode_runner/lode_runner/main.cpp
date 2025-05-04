/**
 * Lode Runner Style Game using OpenGL/GLUT/GLEW
 *
 * Based on classic Lode Runner gameplay with digging mechanics, ropes,
 * and gold collection.
 *
 * Controls:
 * Arrow Keys / A/D: Move Left/Right (also on Ropes)
 * Arrow Keys / W/S: Climb Ladders
 * Q: Dig hole to the left-below (if standing on brick/ladder/rope and brick exists there)
 * E: Dig hole to the right-below (if standing on brick/ladder/rope and brick exists there)
 * R: Reset Game
 * ESC: Exit
 */

#include <GL/glew.h>      // Must be included before freeglut.h
#include <GL/freeglut.h>  // Handles window creation, input, and main loop
#include <vector>
#include <string>
#include <iostream>
#include <sstream>      // For formatting text
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <map>          // To store digging timers
#include <chrono>       // For delta time and respawn timer

 // --- Physics & Movement ---
 // --- Game Constants ---
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int GRID_WIDTH = 20;  // Number of tiles horizontally
const int GRID_HEIGHT = 15; // Number of tiles vertically
const float TILE_SIZE = 40.0f; // Pixel size of a grid tile

// --- Physics & Movement ---
const float PLAYER_SPEED = 150.0f; // Pixels per second
const float ENEMY_SPEED = 120.0f;  // Pixels per second
const float GRAVITY = 500.0f;    // Pixels per second squared
const float JUMP_FORCE = 10.0f; // Lode Runner doesn't jump
const float CLIMB_SPEED = 150.0f; // Pixels per second
const float ROPE_SPEED = 150.0f; // Pixels per second (Speed moving horizontally on ropes)

// --- Gameplay ---
const int MAX_ENEMIES = 3;
const int INITIAL_LIVES = 3;
const float DIG_REFILL_TIME = 7.0f; // Seconds for a dug hole to refill
const int POINTS_PER_COLLECTIBLE = 100;
const float ENEMY_RESPAWN_DELAY = 3.0f; // Seconds before enemy respawns

// --- Tile Types ---
enum TileType {
    EMPTY = 0,
    BRICK = 1,       // Diggable
    LADDER = 2,
    ROPE = 3,        // Horizontal traversal
    SOLID_BRICK = 4, // Indestructible
    EXIT_LADDER = 5, // Appears after collecting all gold
    // Removed DIGGING_BRICK, handled by dugHoles map
};

// --- Entity Structure ---
struct Entity {
    float x, y;          // Position (bottom-left corner)
    float vx, vy;          // Velocity (pixels per second)
    bool isJumping;      // << ADD THIS LINE
    bool isClimbing;     // On ladder
    bool isOnRope;       // On rope
    bool isFalling;      //
    bool faceRight;      // Direction facing
    bool isTrapped;      // If stuck in a dug hole
    float trappedTimer;  // How long they've been trapped (seconds)
    bool isAlive;        // Track if enemy is alive or waiting to respawn
    float respawnTimer;  // Timer for enemy respawn (seconds)
    int startGridX, startGridY; // Initial spawn point for respawning
};

// --- Dug Hole Structure ---
struct DugHole {
    int gridX, gridY;
    float timer; // Time remaining until refill (seconds)
    TileType originalType; // What the tile was before digging (should always be BRICK)
};

// --- Global Variables ---
Entity player;
Entity enemies[MAX_ENEMIES];
int numEnemies = MAX_ENEMIES;
TileType level[GRID_HEIGHT][GRID_WIDTH] = { EMPTY };
std::map<std::pair<int, int>, DugHole> dugHoles; // Track dug holes

bool gameOver = false;
bool gameWon = false;
bool levelComplete = false; // True when all gold is collected
bool keyStates[256] = { false }; // For standard keys
bool specialKeyStates[256] = { false }; // For special keys (arrows)

int collectibles[GRID_HEIGHT][GRID_WIDTH] = { 0 }; // 1 if collectible exists
int collectiblesCollected = 0;
int totalCollectibles = 0;
int score = 0;
int lives = INITIAL_LIVES;

float gameTime = 0.0f; // Simple timer for effects

// --- OpenGL Handles ---
GLuint textures[7]; // 0: brick, 1: ladder, 2: player, 3: enemy, 4: collectible, 5: solid_brick, 6: rope
GLuint vao;         // Vertex Array Object
GLuint vboQuad;     // Vertex Buffer Object for a standard quad
GLuint shaderProgram; // Simple shader for texturing

// --- Function Prototypes ---
// Initialization
void init();
bool initGL();
void loadTextures();
void initLevel();
void initEntities();
void initBuffers();
void initShaders();
void resetGame();

// Game Loop
void display();
void reshape(int w, int h);
void update(int value); // GLUT timer callback

// Input Handling
void keyboardDown(unsigned char key, int x, int y);
void keyboardUp(unsigned char key, int x, int y);
void specialKeyDown(int key, int x, int y);
void specialKeyUp(int key, int x, int y);
void handleInput(float deltaTime); // Pass delta time

// Updates
void updatePlayer(float deltaTime);
void updateEnemies(float deltaTime);
void updatePhysics(Entity& entity, float deltaTime);
void updateDigging(float deltaTime);
void checkLevelCompletion();
void revealExitLadder();

// Drawing
void drawGrid();
void drawEntities();
void drawCollectibles();
void drawHUD();
void drawText(float x, float y, const std::string& text, float r = 1.0f, float g = 1.0f, float b = 1.0f);
// Updated drawQuad to handle texture flipping better using model matrix
void drawQuad(float x, float y, float width, float height, GLuint textureId, bool flipH = false);

// Collision & Grid Interaction
bool isColliding(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2);
bool canMoveTo(float x, float y, float width, float height, bool onRope, bool isClimbing);
TileType getTileAt(float x, float y);
int getGridX(float x);
int getGridY(float y);
bool isOnGround(const Entity& entity);
bool isOnLadder(const Entity& entity);
bool checkOnRope(const Entity& entity); // Renamed to avoid conflict
void digHole(int gridX, int gridY);
void killEnemy(Entity& enemy); // Function to handle enemy death/respawn start

// Timer
auto lastUpdateTime = std::chrono::high_resolution_clock::now();

// --- Main Function ---
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Lode Runner Style Game");

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        std::cerr << "Error initializing GLEW: " << glewGetErrorString(err) << std::endl;
        return 1;
    }
    if (!GLEW_VERSION_3_3) {
        std::cerr << "OpenGL 3.3 not supported!" << std::endl;
        return 1;
    }

    if (!initGL()) {
        std::cerr << "Error initializing OpenGL settings!" << std::endl;
        return 1;
    }
    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeyDown);
    glutSpecialUpFunc(specialKeyUp);
    glutTimerFunc(16, update, 0); // Request first update approx 60fps

    lastUpdateTime = std::chrono::high_resolution_clock::now(); // Initialize timer

    glutMainLoop();

    // Cleanup (won't usually be reached with glutMainLoop)
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vboQuad);
    glDeleteProgram(shaderProgram);
    glDeleteTextures(7, textures); // Clean up all textures

    return 0;
}

// --- Initialization Functions ---

bool initGL() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black background like NES
    glEnable(GL_DEPTH_TEST); // Keep depth test, might be useful later
    glEnable(GL_BLEND);      // Enable alpha blending for textures
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    return true;
}

void init() {
    srand(static_cast<unsigned int>(time(0)));
    initShaders();
    initBuffers();
    loadTextures(); // Load textures after GL context is ready
    initLevel();
    initEntities();

    // Reset game state
    score = 0;
    lives = INITIAL_LIVES;
    collectiblesCollected = 0;
    gameOver = false;
    gameWon = false;
    levelComplete = false;
    dugHoles.clear();
    gameTime = 0.0f;
    lastUpdateTime = std::chrono::high_resolution_clock::now(); // Reset timer
}

void initShaders() {
    // Simple Vertex Shader (Pass-through position and UVs, includes Model matrix)
    const char* vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;      // Vertex position (x, y) in model space (-0.5 to 0.5)
        layout(location = 1) in vec2 aTexCoord; // Texture coordinate (u, v)

        out vec2 TexCoord;

        uniform mat4 projection; // Orthographic projection matrix
        uniform mat4 model;      // Model matrix for position, scale, rotation/flip

        void main() {
            // Transform vertex position: Model -> World -> Clip Space
            gl_Position = projection * model * vec4(aPos.x, aPos.y, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    // Simple Fragment Shader (Sample texture)
    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;

        uniform sampler2D textureSampler;
        uniform vec4 tintColor = vec4(1.0, 1.0, 1.0, 1.0); // Default tint is white

        void main() {
            vec4 texColor = texture(textureSampler, TexCoord);
            // Discard fragment if alpha is very low (basic transparency)
            if (texColor.a < 0.1) discard;
            FragColor = texColor * tintColor; // Apply tint
        }
    )";

    // --- Compile and Link Shaders ---
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // Add error checking here (glGetShaderiv, glGetShaderInfoLog)

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // Add error checking here

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // Add error checking here (glGetProgramiv, glGetProgramInfoLog)

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Check for shader compilation/linking errors (basic example)
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    else {
        std::cout << "Shaders compiled and linked successfully." << std::endl;
    }
}


void initBuffers() {
    // Define vertices for a standard quad centered at (0,0) from -0.5 to 0.5
    // Position (x, y), Texture Coords (u, v)
    float quadVertices[] = {
        // Triangle 1
       -0.5f,  0.5f,  0.0f, 1.0f, // Top-left
       -0.5f, -0.5f,  0.0f, 0.0f, // Bottom-left
        0.5f, -0.5f,  1.0f, 0.0f, // Bottom-right
        // Triangle 2
       -0.5f,  0.5f,  0.0f, 1.0f, // Top-left
        0.5f, -0.5f,  1.0f, 0.0f, // Bottom-right
        0.5f,  0.5f,  1.0f, 1.0f  // Top-right
    };


    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vboQuad);
    glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Position attribute (location = 0 in shader)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute (location = 1 in shader)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind VBO
    glBindVertexArray(0);             // Unbind VAO
}


void loadTextures() {
    const int texSize = 16; // Smaller texture size for more retro pixelated look
    unsigned char texData[texSize][texSize][4]; // RGBA

    // Texture IDs: 0: brick, 1: ladder, 2: player, 3: enemy, 4: collectible, 5: solid_brick, 6: rope
    glGenTextures(7, textures);

    for (int i = 0; i < 7; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        memset(texData, 0, sizeof(texData)); // Clear to transparent black

        // Procedurally generate simple pixel art
        for (int y = 0; y < texSize; y++) {
            for (int x = 0; x < texSize; x++) {
                texData[y][x][3] = 0; // Default alpha = 0 (transparent)

                switch (i) {
                case 0: // Brick (Simple Red Brick)
                    if (x > 0 && x < texSize - 1 && y > 0 && y < texSize - 1) { // Inner part
                        texData[y][x][0] = 180; texData[y][x][1] = 50; texData[y][x][2] = 30; // Reddish brown
                        texData[y][x][3] = 255;
                        // Add slight noise/variation
                        if (rand() % 10 == 0) { texData[y][x][0] -= 10; texData[y][x][1] -= 5; }
                    }
                    else { // Border/Mortar (darker)
                        texData[y][x][0] = 100; texData[y][x][1] = 30; texData[y][x][2] = 15;
                        texData[y][x][3] = 255;
                    }
                    break;
                case 1: // Ladder (Gray Vertical Lines)
                    if (x == 1 || x == texSize - 2 || (y % (texSize / 3) == 0 && y > 0 && y < texSize - 1)) { // Poles and rungs
                        texData[y][x][0] = 150; texData[y][x][1] = 150; texData[y][x][2] = 150; // Gray
                        texData[y][x][3] = 255;
                    }
                    break;
                case 2: // Player (Magenta/Cyan - classic look)
                    // Simple blocky shape matching image
                    if (y >= texSize * 0.6f) { // Head (Blue Helmet)
                        texData[y][x][0] = 0; texData[y][x][1] = 0; texData[y][x][2] = 200;
                        texData[y][x][3] = 255;
                    }
                    else if (y >= texSize * 0.2f) { // Body (Red Shirt)
                        texData[y][x][0] = 200; texData[y][x][1] = 0; texData[y][x][2] = 0;
                        texData[y][x][3] = 255;
                    }
                    else { // Legs (Blue Pants)
                        texData[y][x][0] = 0; texData[y][x][1] = 0; texData[y][x][2] = 200;
                        texData[y][x][3] = 255;
                    }
                    // Add some orange skin tone for face/hands area (approximate)
                    if (y > texSize * 0.5f && y < texSize * 0.7f && x > texSize * 0.2f && x < texSize * 0.8f) {
                        texData[y][x][0] = 255; texData[y][x][1] = 165; texData[y][x][2] = 0; // Orange
                        texData[y][x][3] = 255;
                    }
                    // Simple "eye" area (darker)
                    if (y > texSize * 0.65f && y < texSize * 0.75f && x > texSize * 0.4f && x < texSize * 0.6f) {
                        texData[y][x][0] = 50; texData[y][x][1] = 50; texData[y][x][2] = 50; // Dark gray/black
                        texData[y][x][3] = 255;
                    }

                    break;
                case 3: // Enemy (White/Cyan - classic look)
                    // Simple blocky shape matching image
                    if (y >= texSize * 0.6f) { // Head (Cyan Helmet)
                        texData[y][x][0] = 0; texData[y][x][1] = 200; texData[y][x][2] = 200; // Cyan
                        texData[y][x][3] = 255;
                    }
                    else if (y >= texSize * 0.2f) { // Body (White Shirt)
                        texData[y][x][0] = 230; texData[y][x][1] = 230; texData[y][x][2] = 230; // Off-white
                        texData[y][x][3] = 255;
                    }
                    else { // Legs (Cyan Pants)
                        texData[y][x][0] = 0; texData[y][x][1] = 200; texData[y][x][2] = 200; // Cyan
                        texData[y][x][3] = 255;
                    }
                    // Simple "eye" area (darker)
                    if (y > texSize * 0.65f && y < texSize * 0.75f && x > texSize * 0.4f && x < texSize * 0.6f) {
                        texData[y][x][0] = 50; texData[y][x][1] = 50; texData[y][x][2] = 50; // Dark gray/black
                        texData[y][x][3] = 255;
                    }
                    break;
                case 4: // Collectible (Gold Nugget - Yellow/White Shine)
                {
                    int centerX = texSize / 2;
                    int centerY = texSize / 2;
                    int radiusSq = (texSize / 3) * (texSize / 3);
                    // Basic triangle shape for gold pile
                    if (y < texSize * 0.8f && x > centerX - (y * 0.6f) && x < centerX + (y * 0.6f)) {
                        texData[y][x][0] = 255; texData[y][x][1] = 215; texData[y][x][2] = 0; // Gold
                        texData[y][x][3] = 255;
                        // Shine
                        if (y > texSize * 0.5f && x > centerX - 1 && x < centerX + 1) {
                            texData[y][x][0] = 255; texData[y][x][1] = 255; texData[y][x][2] = 200; // Lighter gold/white
                        }
                    }
                }
                break;
                case 5: // Solid Brick (Gray, simple)
                    texData[y][x][0] = 100; texData[y][x][1] = 100; texData[y][x][2] = 100; // Medium Gray
                    texData[y][x][3] = 255;
                    // Add simple border
                    if (x == 0 || x == texSize - 1 || y == 0 || y == texSize - 1) {
                        texData[y][x][0] = 60; texData[y][x][1] = 60; texData[y][x][2] = 60; // Darker Gray
                    }
                    break;
                case 6: // Rope (Horizontal Yellow/Orange Bar)
                    if (y >= texSize / 2 - 1 && y <= texSize / 2 + 1) { // Middle 3 pixels vertically
                        texData[y][x][0] = 255; // Red
                        texData[y][x][1] = (x % 4 < 2) ? 165 : 255; // Alternating Orange/Yellow segments
                        texData[y][x][2] = 0; // No blue
                        texData[y][x][3] = 255;
                    }
                    break;
                }
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Repeat might be better for some textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Pixelated look
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    std::cout << "Textures loaded." << std::endl;
}


void initLevel() {
    totalCollectibles = 0;
    levelComplete = false; // Reset level completion flag
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            level[y][x] = EMPTY;
            collectibles[y][x] = 0;
        }
    }

    // Define a Lode Runner-style level
    // S = Solid, B = Brick, L = Ladder, R = Rope, C = Gold (on Brick), E = Empty, P = Player Start, X = Enemy Start
    // Note: Y=0 is the BOTTOM row
    const char* levelLayout[] = {
       "SSSSSSSSSSSSSSSSSSSS", // 14 - Top boundary (Solid) - Exit area
       "SEEEEEEEEEEEEEEEEEES", // 13 - Potential Exit Ladder spots
       "SCBBCBBLBBBBBLBBCBCS", // 12
       "SLRRRRRLRRRRRLRRRRRS", // 11
       "SL C C L C C L C C LS", // 10
       "SCBBLBBBLELBBBBLBBBS", // 9
       "SRRRRR C L C C RRCRRS", // 8
       "SE E E B L B E E E ES", // 7
       "SBBBEBBBLBLBBBBBBBBS", // 6
       "SC RRRR L L RRRRR CS", // 5
       "SE E E B L B E E E ES", // 4
       "SBCBEBBBLBLBBBEBBEBS", // 3 - Player start area
       "SXXXXXXELPBLXXXXXXBS", // 2 - Enemy start area, Player start 'P'
       "SEEEEE B B B EEEEEES", // 1
       "SSSSSSSSSSSSSSSSSSSS"  // 0 - Ground (Solid)
    };


    int layoutHeight = sizeof(levelLayout) / sizeof(levelLayout[0]);
    int playerStartX = 1, playerStartY = 3; // Default player start if 'P' not found
    std::vector<std::pair<int, int>> enemyStartPositions;

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        int layoutY = layoutHeight - 1 - y; // Read layout from bottom up
        if (layoutY < 0 || layoutY >= layoutHeight) continue;

        std::string row = levelLayout[layoutY];
        for (int x = 0; x < GRID_WIDTH; ++x) {
            if (x >= row.length()) continue;

            char tileChar = row[x];
            switch (tileChar) {
            case 'S': level[y][x] = SOLID_BRICK; break;
            case 'B': level[y][x] = BRICK; break;
            case 'L': level[y][x] = LADDER; break;
            case 'R': level[y][x] = ROPE; break;
            case 'C':
                level[y][x] = BRICK; // Place gold ON a brick
                if (y + 1 < GRID_HEIGHT) { // Ensure space above for the visual
                    collectibles[y + 1][x] = 1; // Place collectible visual *above* the brick
                    totalCollectibles++;
                }
                else { // If gold is on the top row of bricks, place it there
                    collectibles[y][x] = 1;
                    totalCollectibles++;
                }
                break;
            case 'P': // Explicit Player start
                playerStartX = x;
                playerStartY = y;
                level[y][x] = EMPTY; // Start position should be empty
                break;
            case 'X': // Enemy start position marker
                enemyStartPositions.push_back({ x, y });
                level[y][x] = EMPTY; // Keep the space empty
                break;
            case 'E': // Fallthrough intentional
            default:  level[y][x] = EMPTY; break;
            }
        }
    }
    std::cout << "Level initialized. Total Collectibles: " << totalCollectibles << std::endl;

    // Store player start position (used in initEntities)
    player.startGridX = playerStartX;
    player.startGridY = playerStartY;

    // Store enemy start positions (used in initEntities)
    // Assign starting positions to enemies, cycling through markers if needed
    for (int i = 0; i < numEnemies; ++i) {
        if (!enemyStartPositions.empty()) {
            enemies[i].startGridX = enemyStartPositions[i % enemyStartPositions.size()].first;
            enemies[i].startGridY = enemyStartPositions[i % enemyStartPositions.size()].second;
        }
        else {
            // Fallback if no 'X' markers
            enemies[i].startGridX = GRID_WIDTH - 2 - i;
            enemies[i].startGridY = 2;
            std::cerr << "Warning: No 'X' markers found for enemy start positions. Using fallback." << std::endl;
        }
    }
}

void initEntities() {

    // Place player at start position defined in level or default
    player.x = player.startGridX * TILE_SIZE + (TILE_SIZE * 0.1f); // Position bottom-left
    player.y = player.startGridY * TILE_SIZE;
    player.vx = 0.0f;
    player.vy = 0.0f;
    player.isJumping = false; // Removed
    player.isClimbing = false;
    player.isOnRope = false;
    player.isFalling = false;
    player.faceRight = true;
    player.isTrapped = false;
    player.trappedTimer = 0.0f;
    player.isAlive = true; // Player is always "alive" in this context
    player.respawnTimer = 0.0f;


    // Initialize enemies at their designated start positions
    for (int i = 0; i < numEnemies; ++i) {
        enemies[i].x = enemies[i].startGridX * TILE_SIZE + (TILE_SIZE * 0.1f);
        enemies[i].y = enemies[i].startGridY * TILE_SIZE;
        enemies[i].vx = (rand() % 2 == 0 ? 1 : -1) * ENEMY_SPEED / 2.0f; // Random initial horizontal velocity
        enemies[i].vy = 0.0f;
        enemies[i].isClimbing = false;
        enemies[i].isOnRope = false;
        enemies[i].isFalling = false;
        enemies[i].faceRight = (enemies[i].vx > 0);
        enemies[i].isTrapped = false;
        enemies[i].trappedTimer = 0.0f;
        enemies[i].isAlive = true;
        enemies[i].respawnTimer = 0.0f;
    }
    std::cout << "Entities initialized." << std::endl;
}


void resetGame() {
    std::cout << "Resetting game..." << std::endl;
    init(); // Re-initialize everything
}

// --- Game Loop Functions ---

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram);

    // --- Set up orthographic projection matrix ---
    // Maps world coordinates (0,0 to WINDOW_WIDTH, WINDOW_HEIGHT) to NDC (-1,1 to 1,1)
    float left = 0.0f;
    float right = static_cast<float>(WINDOW_WIDTH);
    float bottom = 0.0f;
    float top = static_cast<float>(WINDOW_HEIGHT);
    // Create Ortho matrix (Column-major order for OpenGL)
    float projectionMatrix[16] = {
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f, // Simplified Z for 2D (maps z=0 to z=0 in NDC)
        -(right + left) / (right - left), -(top + bottom) / (top - bottom), 0.0f, 1.0f
    };
    GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projectionMatrix);

    // --- Bind the VAO (contains quad vertex data and attribute pointers) ---
    glBindVertexArray(vao);

    // --- Draw Game Elements ---
    // Reset tint before drawing each category
    GLint tintLoc = glGetUniformLocation(shaderProgram, "tintColor");
    glUniform4f(tintLoc, 1.0f, 1.0f, 1.0f, 1.0f); // White, Opaque
    drawGrid();

    glUniform4f(tintLoc, 1.0f, 1.0f, 1.0f, 1.0f);
    drawCollectibles();

    glUniform4f(tintLoc, 1.0f, 1.0f, 1.0f, 1.0f);
    drawEntities(); // Draws player and living enemies

    // --- Unbind VAO ---
    glBindVertexArray(0);

    // --- Draw HUD (using GLUT's bitmap fonts - doesn't use the shader) ---
    glUseProgram(0); // Stop using the custom shader
    drawHUD();

    glutSwapBuffers();
}


void reshape(int w, int h) {
    if (h == 0) h = 1; // Prevent division by zero
    glViewport(0, 0, w, h);
    // Projection matrix is set in display() based on fixed WINDOW_WIDTH/HEIGHT
    // So reshape only needs to set the viewport.
}

void update(int value) {
    // Calculate delta time
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastUpdateTime).count();
    lastUpdateTime = currentTime;

    // Clamp delta time to avoid large jumps if debugging or paused
    const float maxDeltaTime = 0.1f; // Max time step of 100ms
    if (deltaTime > maxDeltaTime) deltaTime = maxDeltaTime;

    gameTime += deltaTime; // Increment game time

    if (!gameOver && !gameWon) {
        handleInput(deltaTime);
        updatePlayer(deltaTime);
        updateEnemies(deltaTime);
        updateDigging(deltaTime);
        checkLevelCompletion(); // Check if all gold is collected
    }
    else {
        // Allow reset even when game is over/won by pressing 'R'
        // Input handling for 'R' is done in keyboardDown for immediate response
    }


    glutPostRedisplay();          // Request redraw
    glutTimerFunc(16, update, 0); // Request next update (~60fps)
}

// --- Input Handling ---

void keyboardDown(unsigned char key, int x, int y) {
    keyStates[tolower(key)] = true;
    if (key == 27) { // ESC key
        exit(0);
    }
    // Handle reset immediately only if game is over or won
    if ((gameOver || gameWon) && keyStates['r']) {
        resetGame();
        // No need to consume 'r' here, resetGame reinitializes everything
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    keyStates[tolower(key)] = false;
}

void specialKeyDown(int key, int x, int y) {
    specialKeyStates[key] = true;
}

void specialKeyUp(int key, int x, int y) {
    specialKeyStates[key] = false;
}

void handleInput(float deltaTime) {
    // No input if game over, won, player is trapped, or player is not alive (though player is always alive)
    if (gameOver || gameWon || player.isTrapped || !player.isAlive) return;

    player.vx = 0; // Reset horizontal velocity unless a key is pressed

    bool onLadder = isOnLadder(player);
    player.isOnRope = checkOnRope(player); // Update rope status based on current position

    // --- Horizontal Movement ---
    if (keyStates['a'] || specialKeyStates[GLUT_KEY_LEFT]) {
        if (player.isOnRope) {
            player.vx = -ROPE_SPEED; // Move at rope speed if on rope
        }
        else if (!player.isClimbing) { // Allow horizontal move if not actively climbing ladder
            player.vx = -PLAYER_SPEED;
        }
        player.faceRight = false;
        if (!player.isOnRope) player.isClimbing = false; // Stop climbing ladder if moving horizontally off it
    }
    if (keyStates['d'] || specialKeyStates[GLUT_KEY_RIGHT]) {
        if (player.isOnRope) {
            player.vx = ROPE_SPEED;
        }
        else if (!player.isClimbing) {
            player.vx = PLAYER_SPEED;
        }
        player.faceRight = true;
        if (!player.isOnRope) player.isClimbing = false;
    }

    // --- Vertical Movement (Ladders) ---
    if (onLadder) {
        //player.vy = 0; // Stop gravity/fall on ladder ONLY if moving vertically
        player.isFalling = false;
        player.isOnRope = false; // Cannot be on ladder and rope simultaneously

        if (keyStates['w'] || specialKeyStates[GLUT_KEY_UP]) {
            player.vy = CLIMB_SPEED;
            player.isClimbing = true;
        }
        else if (keyStates['s'] || specialKeyStates[GLUT_KEY_DOWN]) {
            player.vy = -CLIMB_SPEED;
            player.isClimbing = true;
        }
        else {
            // If no vertical input, stop vertical movement on ladder
            player.vy = 0;
            // Allow horizontal movement to take precedence if keys are pressed
            if (!keyStates['a'] && !specialKeyStates[GLUT_KEY_LEFT] && !keyStates['d'] && !specialKeyStates[GLUT_KEY_RIGHT]) {
                player.isClimbing = false; // Not actively climbing if no vertical or horizontal input
            }
            else {
                player.isClimbing = false; // Moving horizontally off ladder
            }
        }
    }
    else {
        player.isClimbing = false; // Not on a ladder
        // Gravity will be applied in updatePhysics if not climbing
    }

    // --- Stop vertical movement if on rope and not falling onto it ---
    if (player.isOnRope) {
        // Only stop vertical velocity if actually *on* the rope, not just touching it while falling
        int playerGridY = getGridY(player.y + TILE_SIZE * 0.1f); // Check slightly above feet
        int playerGridX = getGridX(player.x + TILE_SIZE * 0.4f);
        if (level[playerGridY][playerGridX] == ROPE) {
            player.vy = 0;
            player.isClimbing = false;
            player.isFalling = false;
        }
    }

    bool groundCheck = isOnGround(player); // Check if player is on a surface [cite: 465]
    if (keyStates[' '] && groundCheck && !player.isClimbing && !player.isOnRope && !player.isFalling) { // Check for space key (ASCII 32 might be needed if ' ' doesn't work)
        player.vy = JUMP_FORCE;         // Apply upward velocity
        player.isJumping = true;        // Set jumping state
        player.isFalling = false;       // Not falling initially
        keyStates[' '] = false;         // Consume the key press to prevent repeated jumps
    }
    // --- Digging (Lode Runner Style: Down-Left/Right) ---
    int playerGridX = getGridX(player.x + TILE_SIZE * 0.4f); // Center-ish X
    int playerGridY = getGridY(player.y);                  // Bottom Y
    float checkYBelow = player.y - 1.0f;                   // Check slightly below feet

    // Check if player is standing on a valid surface for digging
    TileType tileBelow = getTileAt(player.x + TILE_SIZE * 0.4f, checkYBelow);
    bool canStand = (tileBelow == BRICK || tileBelow == SOLID_BRICK || tileBelow == LADDER || tileBelow == ROPE || isOnLadder(player) || checkOnRope(player));

    if (canStand && !player.isFalling && !player.isClimbing) { // Can only dig if standing stably
        int targetY = playerGridY - 1; // Target is one row below player

        if (keyStates['q']) { // Dig Left-Below
            int targetX = playerGridX - 1;
            if (targetX >= 0 && targetY >= 0) { // Bounds check
                // Check if the target tile is actually a brick
                if (level[targetY][targetX] == BRICK && dugHoles.find({ targetX, targetY }) == dugHoles.end()) {
                    digHole(targetX, targetY);
                }
            }
            keyStates['q'] = false; // Consume key press (optional, prevents rapid digging)
        }
        else if (keyStates['e']) { // Dig Right-Below
            int targetX = playerGridX + 1;
            if (targetX < GRID_WIDTH && targetY >= 0) { // Bounds check
                // Check if the target tile is actually a brick
                if (level[targetY][targetX] == BRICK && dugHoles.find({ targetX, targetY }) == dugHoles.end()) {
                    digHole(targetX, targetY);
                }
            }
            keyStates['e'] = false; // Consume key press
        }
    }
}


// --- Update Functions ---

void updatePhysics(Entity& entity, float deltaTime) {
    if (entity.isTrapped) {
        // If trapped, handle timer and potential freeing, but no movement/gravity
        entity.trappedTimer -= deltaTime;
        entity.vx = 0;
        entity.vy = 0;

        int gridX = getGridX(entity.x + TILE_SIZE * 0.4f);
        int gridY = getGridY(entity.y);

        if (entity.trappedTimer <= 0) {
            // Timer expired. Check if hole still exists.
            auto currentHoleIt = dugHoles.find({ gridX, gridY });
            if (currentHoleIt == dugHoles.end()) { // Hole refilled while trapped!
                if (&entity != &player) { // Only enemies die when hole refills
                    std::cout << "Enemy killed by refilling hole!" << std::endl;
                    killEnemy(entity); // Mark for respawn
                }
                else {
                    // Player gets freed but might be stuck in brick, give boost
                    std::cout << "Player freed by refill!" << std::endl;
                    entity.isTrapped = false;
                    entity.y += 5.0f; // Small boost upwards
                    entity.isFalling = true; // Apply gravity next frame
                }
            }
            else {
                // Hole still exists, but timer ran out? Keep trapped until refill.
                // This case shouldn't ideally happen if trappedTimer is set correctly relative to DIG_REFILL_TIME
                entity.trappedTimer = 0.01f; // Prevent timer going negative indefinitely
            }
        }
        return; // Skip normal physics update if trapped
    }

    // --- Apply Gravity ---
    // Apply gravity if not climbing a ladder AND not on a rope
    if (!entity.isClimbing && !entity.isOnRope) {
        entity.vy -= GRAVITY * deltaTime;
    }
    else if (entity.isClimbing && entity.vy == 0) {
        // If stopped on a ladder, ensure no residual vertical velocity
        entity.vy = 0;
    }


    // --- Update Position based on Velocity ---
    float oldX = entity.x;
    float oldY = entity.y;
    float newX = entity.x + entity.vx * deltaTime;
    float newY = entity.y + entity.vy * deltaTime;

    // --- Collision Detection & Resolution ---
    float entityWidth = TILE_SIZE * 0.8f; // Use slightly smaller collision box
    float entityHeight = TILE_SIZE * 0.95f;

    // Create a bounding box for the entity's potential new position
    float nextLeft = newX;
    float nextRight = newX + entityWidth;
    float nextBottom = newY;
    float nextTop = newY + entityHeight;

    // --- Vertical Collision ---
    if (entity.vy != 0) { // Only check vertical collision if moving vertically
        // Check points slightly inside the horizontal edges at the new bottom/top Y
        float checkXLeft = nextLeft + TILE_SIZE * 0.1f;
        float checkXRight = nextRight - TILE_SIZE * 0.1f;
        float checkY = (entity.vy < 0) ? nextBottom : nextTop; // Check bottom edge when falling, top edge when rising

        TileType tileLeft = getTileAt(checkXLeft, checkY);
        TileType tileRight = getTileAt(checkXRight, checkY);

        bool collision = false;
        if (entity.vy < 0) { // Moving Down (Falling/Landing)
            // Collision if hitting Brick, Solid Brick, or potentially another entity in a hole
            if (tileLeft == BRICK || tileLeft == SOLID_BRICK || tileRight == BRICK || tileRight == SOLID_BRICK) {
                collision = true;
            }
            // Check landing on trapped enemy head (Lode Runner mechanic)
            for (int i = 0; i < numEnemies; ++i) {
                if (&entity != &enemies[i] && enemies[i].isTrapped) { // Check against other trapped enemies
                    float enemyHeadY = enemies[i].y + TILE_SIZE * 0.9f; // Approx head height
                    if (nextBottom <= enemyHeadY && oldY >= enemyHeadY && // Crossing the head level
                        nextRight > enemies[i].x && nextLeft < enemies[i].x + TILE_SIZE * 0.8f) // Horizontal overlap
                    {
                        collision = true;
                        newY = enemyHeadY; // Land exactly on head
                        break; // Stop checking after landing on one
                    }
                }
            }

            if (collision) {
                int gridY = getGridY(checkY); // Grid Y of the tile being collided with
                newY = static_cast<float>(gridY + 1) * TILE_SIZE; // Snap feet to top of the tile below
                entity.vy = 0;
                entity.isFalling = false;
                if (&entity == &player) { // Only reset jump state for player
                    entity.isJumping = false; // << ADD THIS LINE: Reset jump state on landing
                }
            }
        }
        else { // Moving Up
            // Collision if hitting Brick or Solid Brick
            if (tileLeft == BRICK || tileLeft == SOLID_BRICK || tileRight == BRICK || tileRight == SOLID_BRICK) {
                collision = true;
                int gridY = getGridY(checkY); // Grid Y of the tile being collided with
                newY = static_cast<float>(gridY) * TILE_SIZE - entityHeight; // Snap head to bottom of tile above
                entity.vy = 0; // Stop upward movement
            }
        }
        // If no collision detected while moving down and not climbing/on rope, entity is falling
        if (!collision && entity.vy < 0 && !entity.isClimbing && !entity.isOnRope) {
            entity.isFalling = true;
        }
    }

    // --- Horizontal Collision ---
    if (entity.vx != 0) { // Only check horizontal collision if moving horizontally
        // Check points slightly inside the vertical edges at the new Y position
        float checkYBottom = newY + TILE_SIZE * 0.1f;
        float checkYMiddle = newY + entityHeight * 0.5f;
        float checkYTop = newY + entityHeight * 0.9f; // Check near top
        float checkX = (entity.vx < 0) ? nextLeft : nextRight; // Check left edge when moving left, right edge when moving right

        TileType tileBottom = getTileAt(checkX, checkYBottom);
        TileType tileMiddle = getTileAt(checkX, checkYMiddle);
        TileType tileTop = getTileAt(checkX, checkYTop);

        bool collision = false;
        // Collision if hitting Brick or Solid Brick
        if (tileBottom == BRICK || tileBottom == SOLID_BRICK ||
            tileMiddle == BRICK || tileMiddle == SOLID_BRICK ||
            tileTop == BRICK || tileTop == SOLID_BRICK)
        {
            // Special case: Allow moving horizontally *past* a ladder/rope if not climbing/on it
            bool onValidTraversal = entity.isClimbing || entity.isOnRope;
            if (!onValidTraversal || (tileBottom != LADDER && tileBottom != ROPE && tileMiddle != LADDER && tileMiddle != ROPE && tileTop != LADDER && tileTop != ROPE))
            {
                collision = true;
                int gridX = getGridX(checkX);
                if (entity.vx < 0) { // Moving left
                    newX = static_cast<float>(gridX + 1) * TILE_SIZE; // Snap left edge to right edge of tile
                }
                else { // Moving right
                    newX = static_cast<float>(gridX) * TILE_SIZE - entityWidth; // Snap right edge to left edge of tile
                }
                entity.vx = 0; // Stop horizontal movement
            }
        }
    }


    // --- Update final position ---
    entity.x = newX;
    entity.y = newY;

    // --- Boundary Checks (Window edges) ---
    if (entity.x < 0) entity.x = 0;
    if (entity.x + entityWidth > WINDOW_WIDTH) entity.x = WINDOW_WIDTH - entityWidth;
    if (entity.y < -TILE_SIZE) { // Allow falling slightly off before reset
        entity.y = 0; // Reset Y
        entity.vy = 0;
        if (&entity == &player) { // Only player loses life falling off screen
            lives--;
            if (lives <= 0) {
                gameOver = true;
            }
            else {
                // Respawn player at start
                entity.x = entity.startGridX * TILE_SIZE + (TILE_SIZE * 0.1f);
                entity.y = entity.startGridY * TILE_SIZE;
                entity.vx = 0; entity.vy = 0;
                entity.isFalling = false;
            }
        }
        else {
            // Enemy fell off bottom - kill and respawn
            killEnemy(entity);
        }
    }
    // No top boundary check needed if level prevents it


     // --- Check if falling into a dug hole ---
    int gridX = getGridX(entity.x + entityWidth / 2.0f);
    int gridY = getGridY(entity.y + entityHeight / 2.0f); // Check center
    int gridYFeet = getGridY(entity.y + 1.0f); // Check just above feet
    if (entity.isFalling && entity.vy == 0 && isOnGround(entity)) { // Additional check ensure falling state is reset if vy becomes 0 while on ground
        entity.isFalling = false;
        if (&entity == &player) entity.isJumping = false;
    }

    // Check the tile the feet are currently in
    if (gridX >= 0 && gridX < GRID_WIDTH && gridYFeet >= 0 && gridYFeet < GRID_HEIGHT) {
        auto it = dugHoles.find({ gridX, gridYFeet });
        if (it != dugHoles.end() && entity.isFalling) { // Fell into a hole
            if (!entity.isTrapped) {
                std::cout << "Entity trapped in hole at (" << gridX << ", " << gridYFeet << ")" << std::endl;
                entity.isTrapped = true;
                // Set trapped timer slightly less than refill time, allows enemy to be killed by refill
                entity.trappedTimer = it->second.timer - 0.1f;
                if (entity.trappedTimer < 0) entity.trappedTimer = 0.01f; // Ensure positive

                entity.x = gridX * TILE_SIZE + (TILE_SIZE - entityWidth) / 2.0f; // Center in hole horizontally
                entity.y = gridYFeet * TILE_SIZE; // Align feet with bottom of hole
                entity.vx = 0;
                entity.vy = 0;
                entity.isFalling = false;
                // entity.isJumping = false; // Removed
                entity.isClimbing = false;
            }
        }
    }
}

void updatePlayer(float deltaTime) {
    if (!player.isAlive) return; // Should not happen for player, but safety check

    updatePhysics(player, deltaTime);

    // --- Collectibles ---
    // Check a slightly larger area around the player's center for pickup
    float playerCenterX = player.x + (TILE_SIZE * 0.8f) / 2.0f;
    float playerCenterY = player.y + (TILE_SIZE * 0.95f) / 2.0f;
    int centerGridX = getGridX(playerCenterX);
    int centerGridY = getGridY(playerCenterY);

    // Check 3x3 grid around the player's center grid cell
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            int checkX = centerGridX + dx;
            int checkY = centerGridY + dy;

            if (checkX >= 0 && checkX < GRID_WIDTH && checkY >= 0 && checkY < GRID_HEIGHT) {
                // Check if collectible exists at this grid cell
                if (collectibles[checkY][checkX] == 1) {
                    // Check collision between player bounding box and collectible's small area
                    float collectibleX = checkX * TILE_SIZE + TILE_SIZE * 0.2f; // Approx collectible position
                    float collectibleY = checkY * TILE_SIZE + TILE_SIZE * 0.2f;
                    float collectibleSize = TILE_SIZE * 0.6f;
                    if (isColliding(player.x, player.y, TILE_SIZE * 0.8f, TILE_SIZE * 0.95f,
                        collectibleX, collectibleY, collectibleSize, collectibleSize))
                    {
                        collectibles[checkY][checkX] = 0; // Collect it
                        collectiblesCollected++;
                        score += POINTS_PER_COLLECTIBLE;
                        std::cout << "Collected! Score: " << score << ", Total: " << collectiblesCollected << "/" << totalCollectibles << std::endl;
                        // Add sound effect here if possible
                    }
                }
            }
        }
    }

    // --- Check Win Condition ---
    if (levelComplete && !gameWon) {
        // Check if player reached an exit ladder at the top
        int topGridY = GRID_HEIGHT - 1; // Or adjust based on level design
        int playerHeadGridY = getGridY(player.y + TILE_SIZE * 0.9f);
        int playerFeetGridY = getGridY(player.y + 1.0f);

        // Check if player is overlapping with an exit ladder tile near the top
        if (playerHeadGridY >= topGridY - 1) { // Check top two rows
            TileType tileAtHead = getTileAt(playerCenterX, player.y + TILE_SIZE * 0.9f);
            TileType tileAtFeet = getTileAt(playerCenterX, player.y + 1.0f);
            if (tileAtHead == EXIT_LADDER || tileAtFeet == EXIT_LADDER) {
                gameWon = true;
                std::cout << "Level Complete! Player reached the exit!" << std::endl;
            }
        }
    }
}

void updateEnemies(float deltaTime) {
    for (int i = 0; i < numEnemies; ++i) {
        if (!enemies[i].isAlive) {
            // Handle respawn timer
            enemies[i].respawnTimer -= deltaTime;
            if (enemies[i].respawnTimer <= 0) {
                // Respawn the enemy
                enemies[i].x = enemies[i].startGridX * TILE_SIZE + (TILE_SIZE * 0.1f); // [cite: 161, 306]
                enemies[i].y = enemies[i].startGridY * TILE_SIZE; // [cite: 161, 306]
                enemies[i].vx = (rand() % 2 == 0 ? 1 : -1) * ENEMY_SPEED / 2.0f; // [cite: 162, 307]
                enemies[i].vy = 0.0f; // [cite: 162, 307]
                enemies[i].isClimbing = false; // [cite: 163, 307]
                enemies[i].isOnRope = false; // [cite: 163, 307]
                enemies[i].isFalling = false; // [cite: 163, 307]
                enemies[i].faceRight = (enemies[i].vx > 0); // [cite: 163, 307]
                enemies[i].isTrapped = false; // [cite: 164, 308]
                enemies[i].trappedTimer = 0.0f; // [cite: 164, 308]
                enemies[i].isAlive = true; // Bring back to life [cite: 164, 308]
                enemies[i].respawnTimer = 0.0f; // [cite: 164, 308]
                std::cout << "Enemy " << i << " respawned." << std::endl; // [cite: 309]
            }
            continue; // Skip update if waiting to respawn [cite: 310]
        }

        // Skip AI and physics update if trapped (physics handles trapped state)
        if (enemies[i].isTrapped) { // [cite: 311]
            updatePhysics(enemies[i], deltaTime); // Still need physics for timer/freeing [cite: 312]
            continue; // [cite: 312]
        }

        // --- Improved Lode Runner AI ---
        float targetX = player.x; // [cite: 314]
        float targetY = player.y; // [cite: 314]
        float enemyX = enemies[i].x; // [cite: 314]
        float enemyY = enemies[i].y; // [cite: 314]
        float diffX = targetX - enemyX; // [cite: 315]
        float diffY = targetY - enemyY; // [cite: 315]

        float enemyWidth = TILE_SIZE * 0.8f; // [cite: 315]
        float enemyHeight = TILE_SIZE * 0.95f; // [cite: 315]
        float enemyCenterX = enemyX + enemyWidth / 2.0f; // [cite: 316]
        float enemyFeetY = enemyY; // [cite: 316]
        float enemyHeadY = enemyY + enemyHeight;

        int enemyGridX = getGridX(enemyCenterX); // [cite: 316]
        int enemyGridY = getGridY(enemyFeetY); // [cite: 316]
        int enemyHeadGridY = getGridY(enemyHeadY);

        bool enemyOnLadder = isOnLadder(enemies[i]); // [cite: 317]
        bool enemyOnRope = checkOnRope(enemies[i]); // [cite: 317]
        enemies[i].isOnRope = enemyOnRope; // Update state [cite: 317]

        // --- AI Decision Making ---
        float desiredVX = 0; // [cite: 319]
        float desiredVY = 0; // [cite: 319]
        bool wantsToClimb = false; // [cite: 319]

        // --- Check Environment ---
        // Check for ladders/ropes at current X position and surroundings
        bool ladderAtFeet = (getTileAt(enemyCenterX, enemyFeetY) == LADDER);
        bool ladderBelow = (getTileAt(enemyCenterX, enemyFeetY - 1.0f) == LADDER || ladderAtFeet); // [cite: 320, 321] Consider ladder tile itself for going down
        bool ladderAbove = false; // [cite: 319]
        for (int y = enemyHeadGridY; y < GRID_HEIGHT; ++y) { // Check from head upwards [cite: 322]
            TileType t = getTileAt(enemyCenterX, y * TILE_SIZE + 1.0f); // [cite: 322]
            if (t == LADDER) { ladderAbove = true; break; } // [cite: 323]
            if (t == BRICK || t == SOLID_BRICK) break; // Path blocked [cite: 323]
        }
        bool ropeAtLevel = (getTileAt(enemyCenterX, enemyY + enemyHeight * 0.5f) == ROPE); // Check near vertical center for rope [cite: 324]


        // --- START: MODIFIED AI Priorities ---
        bool canMoveLeft = canMoveTo(enemyX - 1, enemyY, enemyWidth, enemyHeight, enemyOnRope, enemyOnLadder); // Basic check left
        bool canMoveRight = canMoveTo(enemyX + 1, enemyY, enemyWidth, enemyHeight, enemyOnRope, enemyOnLadder); // Basic check right

        // Priority 1: Vertical Alignment via Ladders/Ropes
        if (fabs(diffY) > TILE_SIZE * 0.75) { // Player significantly above/below
            if (diffY > 0 && ladderAbove) { // Player Above, Ladder directly above?
                desiredVY = CLIMB_SPEED; // [cite: 327]
                wantsToClimb = true; // [cite: 327]
            }
            else if (diffY < 0 && ladderBelow) { // Player Below, Ladder directly below?
                desiredVY = -CLIMB_SPEED; // [cite: 328]
                wantsToClimb = true; // [cite: 328]
            }
            else if (ropeAtLevel && fabs(diffY) < TILE_SIZE * 1.5) { // Player near rope level, use rope horizontally
                if (diffX > TILE_SIZE * 0.2f && canMoveRight) desiredVX = ROPE_SPEED; // [cite: 330]
                else if (diffX < -TILE_SIZE * 0.2f && canMoveLeft) desiredVX = -ROPE_SPEED; // [cite: 330]
            }
            else {
                // Seek nearest ladder/rope horizontally
                // Simple version: Just move towards player horizontally for now
                if (diffX > TILE_SIZE * 0.2f && canMoveRight) desiredVX = ENEMY_SPEED; // [cite: 337]
                else if (diffX < -TILE_SIZE * 0.2f && canMoveLeft) desiredVX = -ENEMY_SPEED; // [cite: 337]
            }
        }
        // Priority 2: Horizontal Alignment / Rope Traversal
        else { // Player is roughly level
            if (enemyOnRope) { // Already on rope
                if (diffX > TILE_SIZE * 0.2f && canMoveRight) desiredVX = ROPE_SPEED; // [cite: 330]
                else if (diffX < -TILE_SIZE * 0.2f && canMoveLeft) desiredVX = -ROPE_SPEED; // [cite: 330]
                // Ensure enemy stays vertically aligned with the rope [cite: 331]
                int ropeGridY = getGridY(enemyY + enemyHeight * 0.5f); // [cite: 332]
                if (ropeGridY >= 0 && ropeGridY < GRID_HEIGHT) { // [cite: 332]
                    // Gently nudge towards rope center Y if slightly off
                    float targetRopeY = static_cast<float>(ropeGridY) * TILE_SIZE; // [cite: 333]
                    if (fabs(enemies[i].y - targetRopeY) > 1.0f) {
                        enemies[i].y += (targetRopeY - enemies[i].y) * 0.1f; // Smooth adjustment
                    }
                    enemies[i].vy = 0; // [cite: 334]
                    enemies[i].isFalling = false; // [cite: 335]
                }
            }
            else if (ladderAtFeet && fabs(diffX) < TILE_SIZE * 0.6f) { // On a ladder but player is level? Stop climbing.
                desiredVY = 0;
                wantsToClimb = false; // Stay on ladder level
                // Optionally move horizontally if player is to the side
                if (diffX > TILE_SIZE * 0.2f && canMoveRight) desiredVX = ENEMY_SPEED / 2.0f; // Slower on ladder?
                else if (diffX < -TILE_SIZE * 0.2f && canMoveLeft) desiredVX = -ENEMY_SPEED / 2.0f;

            }
            else { // Not on rope, not climbing vertically significantly, move horizontally
                if (diffX > TILE_SIZE * 0.2f && canMoveRight) desiredVX = ENEMY_SPEED; // [cite: 337]
                else if (diffX < -TILE_SIZE * 0.2f && canMoveLeft) desiredVX = -ENEMY_SPEED; // [cite: 337]
            }
        }

        // --- Hazard Avoidance ---
        if (!wantsToClimb && !enemyOnRope && desiredVX != 0 && !enemies[i].isFalling) {
            float nextX = enemyCenterX + (desiredVX > 0 ? TILE_SIZE * 0.6f : -TILE_SIZE * 0.6f); // Check ahead horizontally
            float checkYBelowNext = enemyFeetY - 1.0f; // Check below the potential next step [cite: 468]
            TileType tileBelowNext = getTileAt(nextX, checkYBelowNext); // [cite: 458]
            TileType tileAtNextFeet = getTileAt(nextX, enemyFeetY);

            // Check for Empty space or Dug Hole below the next step
            bool holeBelowNext = dugHoles.count({ getGridX(nextX), getGridY(checkYBelowNext) }); // Check dugHoles map [cite: 23, 447]
            bool emptyBelowNext = (tileBelowNext == EMPTY && !holeBelowNext);

            // Avoid falling blindly unless onto a ladder/rope or if player is below
            if (emptyBelowNext && tileAtNextFeet != LADDER && tileAtNextFeet != ROPE) {
                // Player is NOT significantly below, so avoid the fall
                if (diffY > -TILE_SIZE) { // Avoid falling if player isn't clearly below
                    desiredVX = 0; // Stop horizontal movement to prevent fall
                }
                // If player IS below, allow the fall (desiredVX remains unchanged)
            }
            // NEW: Check for walking into a hole at foot level
            bool holeAtNextFeet = dugHoles.count({ getGridX(nextX), enemyGridY });
            if (holeAtNextFeet && tileAtNextFeet != LADDER && tileAtNextFeet != ROPE) {
                // Found a hole directly in path, stop moving
                desiredVX = 0;
            }

        }
        // --- END: MODIFIED AI Priorities ---


        // --- Set final velocities based on decisions ---
        enemies[i].vx = desiredVX; // [cite: 339]
        enemies[i].vy = desiredVY; // [cite: 339]
        enemies[i].isClimbing = wantsToClimb; // [cite: 339]
        if (desiredVX != 0) enemies[i].faceRight = (desiredVX > 0); // [cite: 339]


        // Apply physics and collision
        updatePhysics(enemies[i], deltaTime); // [cite: 340]

        // --- Check Collision with Player ---
        if (!player.isTrapped && isColliding(player.x, player.y, TILE_SIZE * 0.8f, TILE_SIZE * 0.95f,
            enemies[i].x, enemies[i].y, enemyWidth, enemyHeight)) // [cite: 341]
        {
            if (!gameOver && !gameWon) { // Only trigger once per life/reset [cite: 341]
                std::cout << "Player caught by enemy " << i << "!" << std::endl; // [cite: 342]
                lives--; // [cite: 342]
                if (lives <= 0) { // [cite: 342]
                    gameOver = true; // [cite: 343]
                }
                else {
                    // Reset player/enemy positions after being caught
                    player.x = player.startGridX * TILE_SIZE + (TILE_SIZE * 0.1f); // [cite: 344]
                    player.y = player.startGridY * TILE_SIZE; // [cite: 344]
                    player.vx = 0; player.vy = 0; player.isFalling = false; player.isTrapped = false; player.isJumping = false; // Reset jump state too [cite: 344]

                    // Optionally reset this specific enemy too
                    enemies[i].x = enemies[i].startGridX * TILE_SIZE + (TILE_SIZE * 0.1f); // [cite: 346]
                    enemies[i].y = enemies[i].startGridY * TILE_SIZE; // [cite: 346]
                    enemies[i].vx = (rand() % 2 == 0 ? 1 : -1) * ENEMY_SPEED / 2.0f; // [cite: 347]
                    enemies[i].isAlive = true; // Ensure it's alive [cite: 347]
                    enemies[i].isTrapped = false; // [cite: 347]
                    // Reset enemy state fully
                    enemies[i].vy = 0.0f;
                    enemies[i].isClimbing = false;
                    enemies[i].isOnRope = false;
                    enemies[i].isFalling = false;
                    enemies[i].faceRight = (enemies[i].vx > 0);

                }
            }
        }
    }
}


void updateDigging(float deltaTime) {
    auto it = dugHoles.begin();
    while (it != dugHoles.end()) {
        it->second.timer -= deltaTime; // Decrease timer

        if (it->second.timer <= 0) {
            // Time to refill the hole
            int x = it->second.gridX;
            int y = it->second.gridY;
            if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
                // Restore the original tile type
                level[y][x] = it->second.originalType;
                std::cout << "Hole refilled at (" << x << ", " << y << ")" << std::endl;

                // Check if any entity is currently trapped in this exact spot when it refills
                float checkX = x * TILE_SIZE + TILE_SIZE * 0.4f; // Center X of the grid cell
                float checkY = y * TILE_SIZE;                   // Bottom Y of the grid cell

                // Check Player
                if (player.isTrapped && getGridX(player.x + TILE_SIZE * 0.4f) == x && getGridY(player.y) == y) {
                    player.isTrapped = false;
                    player.y += 5.0f; // Boost slightly to avoid getting stuck in refilled brick
                    player.isFalling = true;
                    std::cout << "Player freed by refill." << std::endl;
                }
                // Check Enemies
                for (int i = 0; i < numEnemies; ++i) {
                    if (enemies[i].isAlive && enemies[i].isTrapped && getGridX(enemies[i].x + TILE_SIZE * 0.4f) == x && getGridY(enemies[i].y) == y) {
                        std::cout << "Enemy " << i << " killed by refilling hole at (" << x << ", " << y << ")" << std::endl;
                        killEnemy(enemies[i]); // Mark enemy for respawn
                    }
                }
            }
            // Erase the hole from the map and advance iterator
            it = dugHoles.erase(it);
        }
        else {
            // Hole still digging, move to the next one
            ++it;
        }
    }
}

void checkLevelCompletion() {
    if (!levelComplete && collectiblesCollected >= totalCollectibles && totalCollectibles > 0) {
        levelComplete = true;
        std::cout << "All gold collected! Revealing exit ladder." << std::endl;
        revealExitLadder();
        // Add sound effect or visual cue here
    }
}

void revealExitLadder() {
    // Find specific locations (e.g., above certain ladders at the top) and change EMPTY to EXIT_LADDER
    for (int x = 0; x < GRID_WIDTH; ++x) {
        // Example: Reveal ladder above the top-most regular ladders
        if (level[GRID_HEIGHT - 2][x] == LADDER) { // Check row below the top empty space
            if (level[GRID_HEIGHT - 1][x] == EMPTY || level[GRID_HEIGHT - 1][x] == LADDER) { // Ensure space above is empty or ladder
                level[GRID_HEIGHT - 1][x] = EXIT_LADDER;
                std::cout << "Exit ladder revealed at (" << x << ", " << GRID_HEIGHT - 1 << ")" << std::endl;
            }
        }
        // Add more complex logic here if needed based on level design
    }
    // Simple fallback: Place one exit ladder at top center if others fail
    bool foundExit = false;
    for (int x = 0; x < GRID_WIDTH; ++x) if (level[GRID_HEIGHT - 1][x] == EXIT_LADDER) foundExit = true;
    if (!foundExit) {
        int centerX = GRID_WIDTH / 2;
        if (level[GRID_HEIGHT - 2][centerX] == LADDER || level[GRID_HEIGHT - 2][centerX] == EMPTY) {
            level[GRID_HEIGHT - 1][centerX] = EXIT_LADDER;
            std::cout << "Fallback exit ladder revealed at (" << centerX << ", " << GRID_HEIGHT - 1 << ")" << std::endl;
        }
    }

}

void killEnemy(Entity& enemy) {
    if (!enemy.isAlive) return; // Already dead/respawning

    enemy.isAlive = false;
    enemy.isTrapped = false; // Ensure not marked as trapped anymore
    enemy.respawnTimer = ENEMY_RESPAWN_DELAY; // Start respawn timer
    enemy.vx = 0;
    enemy.vy = 0;
    // Position will be reset when respawn timer finishes
    std::cout << "Enemy marked for respawn." << std::endl;
}


// --- Drawing Functions ---

// Draws a textured quad using the bound VAO and current shader program
// Uses a model matrix to position, scale, and flip the quad
void drawQuad(float x, float y, float width, float height, GLuint textureId, bool flipH) {
    // Bind the specific texture to texture unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glUniform1i(glGetUniformLocation(shaderProgram, "textureSampler"), 0); // Tell shader sampler to use unit 0

    // --- Create Model Matrix ---
    // 1. Scale: Scale the base quad (1x1 centered at origin) to desired width/height
    // 2. Flip: Apply horizontal flip if needed (scale X by -1)
    // 3. Translate: Move the scaled quad to the desired world position (x, y)
    // Note: OpenGL matrices are column-major. Order of multiplication is Translate * Rotate * Scale

    // Identity matrix
    float modelMatrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    // 3. Translate to position (x, y) - Adjust for bottom-left origin
    // The quad vertices are -0.5 to 0.5, so center is at 0,0.
    // We want the bottom-left corner to be at (x, y).
    float translateX = x + width / 2.0f;
    float translateY = y + height / 2.0f;
    modelMatrix[12] = translateX; // Apply translation in the last column
    modelMatrix[13] = translateY;

    // 2. Flip (Scale X by -1 if flipH is true)
    float scaleX = width * (flipH ? -1.0f : 1.0f);
    float scaleY = height;

    // 1. Scale
    // Apply scaling factors to the appropriate columns
    modelMatrix[0] *= scaleX; // Scale X
    modelMatrix[5] *= scaleY; // Scale Y

    // --- Send Model Matrix to Shader ---
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelMatrix);

    // --- Draw the Quad ---
    // Assumes VAO with quad vertices/UVs is already bound
    glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vertices define the two triangles of the quad

    // Unbind texture (optional, good practice)
    glBindTexture(GL_TEXTURE_2D, 0);
}


void drawGrid() {
    GLint tintLoc = glGetUniformLocation(shaderProgram, "tintColor");

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            float drawX = static_cast<float>(x) * TILE_SIZE;
            float drawY = static_cast<float>(y) * TILE_SIZE;
            GLuint textureId = 0;
            bool draw = true;
            TileType currentTile = level[y][x];
            glUniform4f(tintLoc, 1.0f, 1.0f, 1.0f, 1.0f); // Reset tint

            // Check if this tile is a dug hole
            auto dugIt = dugHoles.find({ x, y });
            if (dugIt != dugHoles.end()) {
                // Draw digging effect: Darker background (Solid Brick texture tinted)
                textureId = textures[5]; // Use solid brick as background for hole
                float progress = dugIt->second.timer / DIG_REFILL_TIME; // 1 = just dug, 0 = about to refill
                float tint = 0.2f + 0.3f * progress; // Fade from darker to slightly less dark
                glUniform4f(tintLoc, tint, tint, tint, 1.0f); // Tint it gray/darker
            }
            else {
                // Not a dug hole, draw normally based on tile type
                switch (currentTile) {
                case BRICK:       textureId = textures[0]; break;
                case LADDER:      textureId = textures[1]; break;
                case ROPE:        textureId = textures[6]; break; // Use rope texture
                case SOLID_BRICK: textureId = textures[5]; break;
                case EXIT_LADDER: textureId = textures[1]; // Use ladder texture for exit
                    // Optional: Tint exit ladder differently
                    glUniform4f(tintLoc, 0.8f, 1.0f, 0.8f, 1.0f); // Light green tint
                    break;
                case EMPTY:       // Fallthrough intentional
                default:          draw = false; break; // Don't draw empty tiles
                }
            }

            if (draw && textureId != 0) {
                // Pass flipH = false for static grid tiles
                drawQuad(drawX, drawY, TILE_SIZE, TILE_SIZE, textureId, false);
            }
        }
    }
    // Reset tint after drawing grid
    glUniform4f(tintLoc, 1.0f, 1.0f, 1.0f, 1.0f);
}

void drawEntities() {
    GLint tintLoc = glGetUniformLocation(shaderProgram, "tintColor");
    glUniform4f(tintLoc, 1.0f, 1.0f, 1.0f, 1.0f); // Ensure no default tint

    // Draw player
    if (player.isAlive) { // Player should always be alive unless game over logic changes
        float playerWidth = TILE_SIZE * 0.8f;
        float playerHeight = TILE_SIZE * 0.95f;
        // Flip texture based on facing direction
        drawQuad(player.x, player.y, playerWidth, playerHeight, textures[2], !player.faceRight);
    }

    // Draw enemies
    for (int i = 0; i < numEnemies; ++i) {
        if (enemies[i].isAlive) { // Only draw living enemies
            float enemyWidth = TILE_SIZE * 0.8f;
            float enemyHeight = TILE_SIZE * 0.95f;

            // Tint slightly red if trapped (optional visual cue)
            if (enemies[i].isTrapped) {
                glUniform4f(tintLoc, 1.0f, 0.7f, 0.7f, 1.0f); // Light red tint
            }
            else {
                glUniform4f(tintLoc, 1.0f, 1.0f, 1.0f, 1.0f); // No tint
            }

            // Flip texture based on facing direction
            drawQuad(enemies[i].x, enemies[i].y, enemyWidth, enemyHeight, textures[3], !enemies[i].faceRight);
        }
    }
    glUniform4f(tintLoc, 1.0f, 1.0f, 1.0f, 1.0f); // Reset tint
}

void drawCollectibles() {
    GLint tintLoc = glGetUniformLocation(shaderProgram, "tintColor");
    glUniform4f(tintLoc, 1.0f, 1.0f, 1.0f, 1.0f); // Ensure no tint

    float collectibleSize = TILE_SIZE * 0.6f; // Make gold smaller than tile
    float offsetX = (TILE_SIZE - collectibleSize) / 2.0f; // Center it horizontally
    float offsetY = TILE_SIZE * 0.1f; // Position slightly above bottom of cell

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            if (collectibles[y][x] == 1) {
                float drawX = static_cast<float>(x) * TILE_SIZE + offsetX;
                float drawY = static_cast<float>(y) * TILE_SIZE + offsetY;
                // Add slight bobbing effect using gameTime
                drawY += sin(gameTime * 4.0f + x * 0.5f) * TILE_SIZE * 0.08f;
                drawQuad(drawX, drawY, collectibleSize, collectibleSize, textures[4], false);
            }
        }
    }
}


void drawHUD() {
    // --- Use fixed-function pipeline for text rendering (simpler than shader text) ---
    glMatrixMode(GL_PROJECTION);
    glPushMatrix(); // Save the current projection matrix (likely the shader's ortho)
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT); // Set ortho mode for screen coordinates

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix(); // Save the current modelview matrix
    glLoadIdentity();

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT); // Save enable states and color
    glDisable(GL_DEPTH_TEST); // Draw HUD on top
    glDisable(GL_TEXTURE_2D); // Don't need textures for basic text
    glDisable(GL_LIGHTING);   // Ensure lighting is off if it were enabled

    // Draw Score
    std::stringstream ssScore;
    ssScore << "Score: " << score;
    drawText(10, WINDOW_HEIGHT - 25, ssScore.str(), 1.0f, 1.0f, 0.0f); // Yellow text

    // Draw Lives
    std::stringstream ssLives;
    ssLives << "Lives: " << lives;
    drawText(WINDOW_WIDTH - 100, WINDOW_HEIGHT - 25, ssLives.str(), 1.0f, 0.2f, 0.2f); // Red text

    // Draw Collectibles count
    std::stringstream ssCollectibles;
    ssCollectibles << "Gold: " << collectiblesCollected << " / " << totalCollectibles;
    drawText(10, WINDOW_HEIGHT - 50, ssCollectibles.str(), 0.9f, 0.9f, 0.9f); // Light Gray text


    // Draw Game Over / You Win Message Centered
    if (gameOver) {
        std::string msg = "GAME OVER! Press 'R' to Restart";
        // Estimate text width (GLUT doesn't provide an easy way, this is approximate)
        float textWidth = msg.length() * 10.0f; // Adjust multiplier based on font size
        drawText((WINDOW_WIDTH - textWidth) / 2, WINDOW_HEIGHT / 2, msg, 1.0f, 0.2f, 0.2f);
    }
    else if (gameWon) {
        std::string msg = "YOU WIN! Press 'R' to Play Again";
        float textWidth = msg.length() * 10.0f;
        drawText((WINDOW_WIDTH - textWidth) / 2, WINDOW_HEIGHT / 2, msg, 0.2f, 1.0f, 0.2f);
    }

    // Restore previous OpenGL states
    glPopAttrib(); // Restore enable states and color

    glMatrixMode(GL_PROJECTION);
    glPopMatrix(); // Restore projection matrix
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix(); // Restore modelview matrix
}

// Helper function to draw text using GLUT bitmap fonts
// Note: This uses legacy OpenGL. For modern GL, use a text rendering library or texture atlases.
void drawText(float x, float y, const std::string& text, float r, float g, float b) {
    glColor3f(r, g, b);           // Set text color
    glRasterPos2f(x, y);        // Set position for text rendering (bottom-left)

    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c); // Use a standard bitmap font
    }
}


// --- Collision & Grid Interaction ---

// Simple Axis-Aligned Bounding Box collision check
bool isColliding(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
    return (x1 < x2 + w2 &&
        x1 + w1 > x2 &&
        y1 < y2 + h2 &&
        y1 + h1 > y2);
}

// Gets the tile type at a specific world coordinate (x, y)
// Takes dug holes into account.
TileType getTileAt(float x, float y) {
    int gridX = static_cast<int>(floor(x / TILE_SIZE));
    int gridY = static_cast<int>(floor(y / TILE_SIZE));

    // Bounds check
    if (gridX < 0 || gridX >= GRID_WIDTH || gridY < 0 || gridY >= GRID_HEIGHT) {
        return SOLID_BRICK; // Treat out-of-bounds as solid
    }

    // Check for dug holes first - they act as EMPTY space for collision
    auto it = dugHoles.find({ gridX, gridY });
    if (it != dugHoles.end()) {
        return EMPTY;
    }

    // Return the actual tile type from the level grid
    return level[gridY][gridX];
}

// Helper to get grid X index from world X coordinate
int getGridX(float x) {
    return static_cast<int>(floor(x / TILE_SIZE));
}
// Helper to get grid Y index from world Y coordinate
int getGridY(float y) {
    return static_cast<int>(floor(y / TILE_SIZE));
}


// Check if the entity can move to the target (x, y) position.
// Checks collision with solid tiles based on entity's bounding box.
// `onRope` and `isClimbing` flags influence how ladders/ropes are treated.
bool canMoveTo(float x, float y, float width, float height, bool onRope, bool isClimbing) {
    // Check corners and center points of the entity's bounding box in the potential new position
    float checkPointsX[] = { x, x + width / 2.0f, x + width };
    float checkPointsY[] = { y, y + height / 2.0f, y + height };

    for (float checkX : checkPointsX) {
        for (float checkY : checkPointsY) {
            // Don't check points exactly at the top/right edge if moving up/right?
            // Check slightly inside maybe? For now, check edges.

            TileType tile = getTileAt(checkX, checkY);

            switch (tile) {
            case BRICK:
            case SOLID_BRICK:
                // Collision with solid tile - cannot move here
                // Exception: Allow moving slightly into a solid tile if climbing off a ladder? Needs care.
                return false;
            case LADDER:
                // Allow passing through ladders if climbing, or moving horizontally
                // Block vertical movement through ladder if not climbing? Physics handles this mostly.
                break; // Generally passable, physics handles gravity/climbing speed
            case ROPE:
                // Allow passing horizontally if onRope is true.
                // Block falling through rope unless moving off the edge.
                // Block moving vertically onto a rope unless falling?
                break; // Generally passable horizontally, physics handles falling
            case EXIT_LADDER: // Treat like a regular ladder for collision
                break;
            case EMPTY:
            default:
                continue; // No collision with empty space
            }
        }
    }

    return true; // No solid collision detected
}


// Check if the entity is standing on solid ground (Brick, Solid Brick, or trapped enemy head)
bool isOnGround(const Entity& entity) {
    // Check slightly below the entity's feet at left, center, and right points
    float entityWidth = TILE_SIZE * 0.8f;
    float checkXLeft = entity.x + entityWidth * 0.1f;
    float checkXCenter = entity.x + entityWidth * 0.5f;
    float checkXRight = entity.x + entityWidth * 0.9f;
    float checkY = entity.y - 1.0f; // Check 1 pixel below feet

    TileType tileLeft = getTileAt(checkXLeft, checkY);
    TileType tileCenter = getTileAt(checkXCenter, checkY);
    TileType tileRight = getTileAt(checkXRight, checkY);

    // Considered on ground if standing on Brick or Solid Brick
    bool onSolidTile = (tileLeft == BRICK || tileLeft == SOLID_BRICK ||
        tileCenter == BRICK || tileCenter == SOLID_BRICK ||
        tileRight == BRICK || tileRight == SOLID_BRICK);

    if (onSolidTile) return true;

    // Check if standing on top of a trapped enemy's head
    for (int i = 0; i < numEnemies; ++i) {
        if (&entity != &enemies[i] && enemies[i].isTrapped) { // Check other entities that are trapped
            float enemyHeadY = enemies[i].y + TILE_SIZE * 0.9f; // Approx head height
            // Check if entity's feet are very close to the enemy's head Y
            // and horizontally overlapping
            if (fabs(entity.y - enemyHeadY) < 5.0f &&
                entity.x + entityWidth > enemies[i].x &&
                entity.x < enemies[i].x + TILE_SIZE * 0.8f)
            {
                return true; // Standing on trapped enemy head
            }
        }
    }

    return false; // Not on solid tile or trapped enemy
}

// Check if the entity is overlapping with a ladder tile at its center column
bool isOnLadder(const Entity& entity) {
    float entityWidth = TILE_SIZE * 0.8f;
    float entityHeight = TILE_SIZE * 0.95f;
    float checkX = entity.x + entityWidth / 2.0f; // Center X
    // Check multiple points vertically along the center line
    float checkYBottom = entity.y + entityHeight * 0.1f; // Near feet
    float checkYMiddle = entity.y + entityHeight * 0.5f; // Middle
    float checkYTop = entity.y + entityHeight * 0.9f; // Near head

    TileType tileBottom = getTileAt(checkX, checkYBottom);
    TileType tileMiddle = getTileAt(checkX, checkYMiddle);
    TileType tileTop = getTileAt(checkX, checkYTop);

    // True if any central part overlaps with a ladder or exit ladder
    return (tileBottom == LADDER || tileBottom == EXIT_LADDER ||
        tileMiddle == LADDER || tileMiddle == EXIT_LADDER ||
        tileTop == LADDER || tileTop == EXIT_LADDER);
}

// Check if the entity is overlapping with a rope tile near its vertical center
// and is roughly horizontally aligned with it.
bool checkOnRope(const Entity& entity) {
    float entityWidth = TILE_SIZE * 0.8f;
    float entityHeight = TILE_SIZE * 0.95f;
    // Check near the middle of the entity horizontally and vertically
    float checkX = entity.x + entityWidth / 2.0f;
    float checkY = entity.y + entityHeight * 0.5f; // Check vertical center

    TileType tileCenter = getTileAt(checkX, checkY);

    // Check if the tile at the vertical center is a rope
    if (tileCenter == ROPE) {
        // Check if the entity's feet are reasonably close to the rope's level
        int ropeGridY = getGridY(checkY);
        float ropeCenterY = ropeGridY * TILE_SIZE + TILE_SIZE / 2.0f;
        // Allow being slightly above/below the rope center while still considered "on" it
        if (fabs(entity.y - ropeGridY * TILE_SIZE) < TILE_SIZE * 0.3f) {
            return true;
        }
    }
    return false;
}

// Creates a dug hole at the specified grid coordinates if possible
void digHole(int gridX, int gridY) {
    // Check bounds
    if (gridX < 0 || gridX >= GRID_WIDTH || gridY < 0 || gridY >= GRID_HEIGHT) {
        std::cerr << "Dig attempt out of bounds (" << gridX << ", " << gridY << ")" << std::endl;
        return;
    }

    // Check if the tile is diggable (only BRICK)
    if (level[gridY][gridX] == BRICK) {
        // Check if there's already a hole being dug here
        if (dugHoles.find({ gridX, gridY }) == dugHoles.end()) {
            // Create a new dug hole entry
            DugHole hole;
            hole.gridX = gridX;
            hole.gridY = gridY;
            hole.timer = DIG_REFILL_TIME;
            hole.originalType = BRICK; // Store original type (always brick)

            dugHoles[{gridX, gridY}] = hole;
            // Don't change level[y][x] here; getTileAt handles checking dugHoles.
            // The visual representation is handled in drawGrid.

            std::cout << "Dug hole initiated at (" << gridX << ", " << gridY << ")" << std::endl;
            // Add digging sound effect here if possible
        }
        else {
            // Optional: Prevent re-digging an existing hole? Or maybe reset timer?
            // std::cout << "Already digging at (" << gridX << ", " << gridY << ")" << std::endl;
        }
    }
    else {
        std::cout << "Cannot dig non-brick tile type " << level[gridY][gridX] << " at (" << gridX << ", " << gridY << ")" << std::endl;
    }
}
