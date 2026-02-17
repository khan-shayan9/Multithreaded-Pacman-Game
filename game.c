// i21-0484 Shayan Khan SE-B

#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

// Constants
#define BOARD_WIDTH 19
#define BOARD_HEIGHT 21
#define CELL_SIZE 20
#define WINDOW_WIDTH (BOARD_WIDTH * CELL_SIZE)
#define WINDOW_HEIGHT (BOARD_HEIGHT * CELL_SIZE)

#define GHOST_COUNT 4
#define MAX_SPEED_BOOSTS 2

// Game elements
#define EMPTY 0
#define WALL 1
#define PELLET 2g
#define POWER_PELLET 3
#define PACMAN 4
#define GHOST 5
#define GHOST_HOUSE 6
#define GHOST_HOUSE_DOOR 7

// Directions
#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3

// Game state
typedef struct {
    int board[BOARD_HEIGHT][BOARD_WIDTH];
    int pacmanX, pacmanY;
    int pacmanDirection;
    int lives;
    int score;
    bool gameOver;
    bool powerPelletActive;
    int powerPelletTimer;
    int ghostsEaten;
} GameState;

// Ghost structure
typedef struct {
    int id;
    int x, y;
    int direction;
    bool isBlue;
    bool isEaten;
    bool isFast;
    bool inGhostHouse;
    bool hasSpeedBoost;
} Ghost;

// Global variables
GameState gameState;
Ghost ghosts[GHOST_COUNT];
pthread_t gameEngineThread, uiThread, ghostThreads[GHOST_COUNT];

// Synchronization primitives
pthread_mutex_t boardMutex;  // Protects the game board
pthread_mutex_t powerPelletMutex;  // Protects power pellet state
sem_t ghostHouseKeys;  // Limited keys for ghost house
sem_t ghostHousePermits;  // Limited permits for ghost house exit
sem_t speedBoosts;  // Limited speed boosts for ghosts
pthread_mutex_t speedBoostMutex;  // Protects speed boost allocation

// Game board layout (1 = wall, 0 = empty, 2 = pellet, 3 = power pellet, 6 = ghost house, 7 = ghost house door)
int initialBoard[BOARD_HEIGHT][BOARD_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,1},
    {1,3,1,1,2,1,1,1,2,1,2,1,1,1,2,1,1,3,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,2,1,1,1,1,1,2,1,2,1,1,2,1},
    {1,2,2,2,2,1,2,2,2,1,2,2,2,1,2,2,2,2,1},
    {1,1,1,1,2,1,1,1,0,1,0,1,1,1,2,1,1,1,1},
    {0,0,0,1,2,1,0,0,0,0,0,0,0,1,2,1,0,0,0},
    {1,1,1,1,2,1,0,1,1,7,1,1,0,1,2,1,1,1,1},
    {0,0,0,0,2,0,0,1,6,6,6,1,0,0,2,0,0,0,0},
    {1,1,1,1,2,1,0,1,1,1,1,1,0,1,2,1,1,1,1},
    {0,0,0,1,2,1,0,0,0,0,0,0,0,1,2,1,0,0,0},
    {1,1,1,1,2,1,0,1,1,1,1,1,0,1,2,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,1,2,1,2,1,1,1,2,1,1,2,1},
    {1,3,2,1,2,2,2,2,2,0,2,2,2,2,2,1,2,3,1},
    {1,1,2,1,2,1,2,1,1,1,1,1,2,1,2,1,2,1,1},
    {1,2,2,2,2,1,2,2,2,1,2,2,2,1,2,2,2,2,1},
    {1,2,1,1,1,1,1,1,2,1,2,1,1,1,1,1,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Function prototypes
void initGame();
void* gameEngineFunction(void* arg);
void* uiFunction(void* arg);
void* ghostFunction(void* arg);
void movePacman();
void moveGhost(Ghost* ghost);
void checkCollisions();
void drawGame();
void keyboard(int key, int x, int y);
void display();
void initOpenGL();
void timer(int value);
int canMove(int x, int y);
bool isPacmanCollision(Ghost* ghost);
void eatGhost(Ghost* ghost);
void respawnGhost(Ghost* ghost);
void activatePowerPellet();
void deactivatePowerPellet();
void drawText(float x, float y, const char* text, void* font);


// Initialize the game
void initGame() {
    // Initialize game state
    gameState.pacmanX = 9;
    gameState.pacmanY = 15;
    gameState.pacmanDirection = LEFT;
    gameState.lives = 3;
    gameState.score = 0;
    gameState.gameOver = false;
    gameState.powerPelletActive = false;
    gameState.powerPelletTimer = 0;
    gameState.ghostsEaten = 0;
    
    // Copy initial board layout
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            gameState.board[y][x] = initialBoard[y][x];
        }
    }
    
    // Initialize ghosts
    ghosts[0].id = 0;
    ghosts[0].x = 9;
    ghosts[0].y = 9;
    ghosts[0].direction = UP;
    ghosts[0].isBlue = false;
    ghosts[0].isEaten = false;
    ghosts[0].isFast = true;
    ghosts[0].inGhostHouse = true;
    ghosts[0].hasSpeedBoost = false;
    
    ghosts[1].id = 1;
    ghosts[1].x = 8;
    ghosts[1].y = 9;
    ghosts[1].direction = UP;
    ghosts[1].isBlue = false;
    ghosts[1].isEaten = false;
    ghosts[1].isFast = false;
    ghosts[1].inGhostHouse = true;
    ghosts[1].hasSpeedBoost = false;
    
    ghosts[2].id = 2;
    ghosts[2].x = 10;
    ghosts[2].y = 9;
    ghosts[2].direction = UP;
    ghosts[2].isBlue = false;
    ghosts[2].isEaten = false;
    ghosts[2].isFast = true;
    ghosts[2].inGhostHouse = true;
    ghosts[2].hasSpeedBoost = false;
    
    ghosts[3].id = 3;
    ghosts[3].x = 9;
    ghosts[3].y = 8;
    ghosts[3].direction = UP;
    ghosts[3].isBlue = false;
    ghosts[3].isEaten = false;
    ghosts[3].isFast = false;
    ghosts[3].inGhostHouse = true;
    ghosts[3].hasSpeedBoost = false;
    
    // Initialize synchronization primitives
    pthread_mutex_init(&boardMutex, NULL);
    pthread_mutex_init(&powerPelletMutex, NULL);
    pthread_mutex_init(&speedBoostMutex, NULL);
    
    // Initialize semaphores
    sem_init(&ghostHouseKeys, 0, 2);  // 2 keys available
    sem_init(&ghostHousePermits, 0, 1);  // 1 ghost can exit at a time
    sem_init(&speedBoosts, 0, MAX_SPEED_BOOSTS);  // 2 speed boosts available
    
    // Seed random number generator
    srand(time(NULL));
}

// Game engine thread function
void* gameEngineFunction(void* arg) {
    while (!gameState.gameOver) {
        // Move Pac-Man
        movePacman();
        
        // Check for collisions with ghosts
        checkCollisions();
        
        // Check if all pellets are eaten
        bool allPelletsEaten = true;
        pthread_mutex_lock(&boardMutex);
        for (int y = 0; y < BOARD_HEIGHT; y++) {
            for (int x = 0; x < BOARD_WIDTH; x++) {
                if (gameState.board[y][x] == PELLET || gameState.board[y][x] == POWER_PELLET) {
                    allPelletsEaten = false;
                    break;
                }
            }
            if (!allPelletsEaten) break;
        
        }
        pthread_mutex_unlock(&boardMutex);
        
        if (allPelletsEaten) {
            printf("*** ALL PELLETS EATEN, YOU WON CONGRATULATIONS :) ***\n");
            gameState.gameOver = true;
        }
        
        // Update power pellet timer
        if (gameState.powerPelletActive) {
            gameState.powerPelletTimer--;
            if (gameState.powerPelletTimer <= 0) {
                deactivatePowerPellet();
            }
        }
        
        // Sleep to control game speed
        usleep(100000);  // 100ms
    }
    
    return NULL;
}

// UI thread function
void* uiFunction(void* arg) {
    // Handled by GLUT main loop
    return NULL;
}
void drawText(float x, float y, const char* text, void* font) {
    glRasterPos2f(x, y);
    for (int i = 0; text[i] != '\0'; i++) {
        glutBitmapCharacter(font, text[i]);
    }
}

// Ghost thread function
void* ghostFunction(void* arg) {
    Ghost* ghost = (Ghost*)arg;
    
    while (!gameState.gameOver) {
        // If ghost is in ghost house, try to exit
        if (ghost->inGhostHouse) {
            // Try to get key and permit to exit ghost house
            if (sem_trywait(&ghostHouseKeys) == 0) {
                if (sem_trywait(&ghostHousePermits) == 0) {
                    // Successfully got key and permit, exit ghost house
                    ghost->inGhostHouse = false;
                    ghost->y = 8;  // Position just outside ghost house
                    
                    // Release key and permit
                    sem_post(&ghostHouseKeys);
                    sem_post(&ghostHousePermits);
                } else {
                    // Failed to get permit, release key
                    sem_post(&ghostHouseKeys);
                }
            }
            
            // If ghost is fast, try to get speed boost
            if (ghost->isFast && !ghost->hasSpeedBoost) {
                if (sem_trywait(&speedBoosts) == 0) {
                    pthread_mutex_lock(&speedBoostMutex);
                    ghost->hasSpeedBoost = true;
                    pthread_mutex_unlock(&speedBoostMutex);
                }
            }
        } else {
            // Move ghost
            moveGhost(ghost);
            
            // Check if collided with Pac-Man
            if (isPacmanCollision(ghost)) {
                if (gameState.powerPelletActive) {
                    eatGhost(ghost);
                } else {
                    // Pac-Man loses a life
                    pthread_mutex_lock(&boardMutex);
                    gameState.lives--;
                    if (gameState.lives <= 0) {
                        gameState.gameOver = true;
                    } else {
                        // Reset positions
                        gameState.pacmanX = 9;
                        gameState.pacmanY = 15;
                        for (int i = 0; i < GHOST_COUNT; i++) {
                            respawnGhost(&ghosts[i]);
                        }
                    }
                    pthread_mutex_unlock(&boardMutex);
                }
            }
        }
        
        // Sleep to control ghost speed
        if (ghost->hasSpeedBoost) {
            usleep(189999);  // Faster ghosts move every 80ms
        } else {
            usleep(299999);  // Normal ghosts move every 120ms
        }
    }
    
    // Release speed boost if holding one
    if (ghost->hasSpeedBoost) {
        sem_post(&speedBoosts);
        ghost->hasSpeedBoost = false;
    }
    
    return NULL;
}

// Move Pac-Man based on current direction
void movePacman() {
    int newX = gameState.pacmanX;
    int newY = gameState.pacmanY;
    
    // Calculate new position based on direction
    switch (gameState.pacmanDirection) {
        case UP:
            newY--;
            break;
        case RIGHT:
            newX++;
            break;
        case DOWN:
            newY++;
            break;
        case LEFT:
            newX--;
            break;
    }
    
    // Check if new position is valid
    pthread_mutex_lock(&boardMutex);
    if (canMove(newX, newY)) {
        // Update position
        gameState.pacmanX = newX;
        gameState.pacmanY = newY;
        
        // Check if Pac-Man ate a pellet
        if (gameState.board[newY][newX] == PELLET) {
            gameState.board[newY][newX] = EMPTY;
            gameState.score += 10;
        } 
        // Check if Pac-Man ate a power pellet
        else if (gameState.board[newY][newX] == POWER_PELLET) {
            pthread_mutex_lock(&powerPelletMutex);
            gameState.board[newY][newX] = EMPTY;
            activatePowerPellet();
            pthread_mutex_unlock(&powerPelletMutex);
            gameState.score += 50;
        }
    }
    pthread_mutex_unlock(&boardMutex);
}

// Move ghost based on AI
void moveGhost(Ghost* ghost) {
    int newX = ghost->x;
    int newY = ghost->y;
    
    // If the ghost is eaten, return to ghost house
    if (ghost->isEaten) {
        // Simple path finding to ghost house
        if (ghost->x < 9) newX++;
        else if (ghost->x > 9) newX--;
        else if (ghost->y < 9) newY++;
        else if (ghost->y > 9) newY--;
        else {
            // Reached ghost house
            ghost->isEaten = false;
            ghost->inGhostHouse = true;
            ghost->isBlue = false;
            
            // Release speed boost if it has one
            if (ghost->hasSpeedBoost) {
                sem_post(&speedBoosts);
                ghost->hasSpeedBoost = false;
            }
            
            pthread_mutex_lock(&boardMutex);
            pthread_mutex_unlock(&boardMutex);
            return;
        }
    } else {
        // Simple ghost AI (random movement with bias toward Pac-Man)
        int directions[4];
        int availableDirections = 0;
        
        pthread_mutex_lock(&boardMutex);
        
        // Check available directions
        if (canMove(ghost->x, ghost->y - 1) && ghost->direction != DOWN) {
            directions[availableDirections++] = UP;
        }
        if (canMove(ghost->x + 1, ghost->y) && ghost->direction != LEFT) {
            directions[availableDirections++] = RIGHT;
        }
        if (canMove(ghost->x, ghost->y + 1) && ghost->direction != UP) {
            directions[availableDirections++] = DOWN;
        }
        if (canMove(ghost->x - 1, ghost->y) && ghost->direction != RIGHT) {
            directions[availableDirections++] = LEFT;
        }
        
        pthread_mutex_unlock(&boardMutex);
        
        // If in blue mode, try to move away from Pac-Man
        if (ghost->isBlue) {
            // Choose random direction from available
            if (availableDirections > 0) {
                ghost->direction = directions[rand() % availableDirections];
            }
        } 
        // If not in blue mode, try to move toward Pac-Man
        else {
            // Simplified intelligence - bias movement toward Pac-Man
            int pacmanDirX = gameState.pacmanX - ghost->x;
            int pacmanDirY = gameState.pacmanY - ghost->y;
            
            // Choose direction with bias toward Pac-Man
            if (availableDirections > 0) {
                int bestDir = -1;
                int bestScore = -1000;
                
                for (int i = 0; i < availableDirections; i++) {
                    int score = 0;
                    
                    switch (directions[i]) {
                        case UP:
                            score = pacmanDirY < 0 ? 10 : -5;
                            break;
                        case RIGHT:
                            score = pacmanDirX > 0 ? 10 : -5;
                            break;
                        case DOWN:
                            score = pacmanDirY > 0 ? 10 : -5;
                            break;
                        case LEFT:
                            score = pacmanDirX < 0 ? 10 : -5;
                            break;
                    }
                    
                    // Add some randomness
                    score += rand() % 5;
                    
                    if (score > bestScore) {
                        bestScore = score;
                        bestDir = directions[i];
                    }
                }
                
                ghost->direction = bestDir;
            }
        }
        
        // Calculate new position based on direction
        switch (ghost->direction) {
            case UP:
                newY--;
                break;
            case RIGHT:
                newX++;
                break;
            case DOWN:
                newY++;
                break;
            case LEFT:
                newX--;
                break;
        }
    }
    
    // Update position if valid
    pthread_mutex_lock(&boardMutex);
    if (canMove(newX, newY)) {
        ghost->x = newX;
        ghost->y = newY;
    } else {
        // If can't move, choose a new random direction
        ghost->direction = rand() % 4;
    }
    pthread_mutex_unlock(&boardMutex);
}

// Check for collisions between Pac-Man and ghosts
void checkCollisions() {
    for (int i = 0; i < GHOST_COUNT; i++) {
        if (isPacmanCollision(&ghosts[i])) {
            if (gameState.powerPelletActive && !ghosts[i].isEaten) {
                eatGhost(&ghosts[i]);
            } else if (!ghosts[i].isEaten) {
                // Pac-Man loses a life
                pthread_mutex_lock(&boardMutex);
                gameState.lives--;
                if (gameState.lives <= 0) {
                    gameState.gameOver = true;
                } else {
                    // Reset positions
                    gameState.pacmanX = 9;
                    gameState.pacmanY = 15;
                    for (int j = 0; j < GHOST_COUNT; j++) {
                        respawnGhost(&ghosts[j]);
                    }
                }
                pthread_mutex_unlock(&boardMutex);
                break;
            }
        }
    }
}

// Check if a position is valid for movement
int canMove(int x, int y) {
    // Check bounds
    if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT) {
        return 0;
    }
    
    // Check if cell is a wall or ghost house (if not a ghost)
    return (gameState.board[y][x] != WALL && gameState.board[y][x] != GHOST_HOUSE);
}

// Check if ghost collided with Pac-Man
bool isPacmanCollision(Ghost* ghost) {
    return (ghost->x == gameState.pacmanX && ghost->y == gameState.pacmanY && !ghost->inGhostHouse);
}

// Eat a ghost
void eatGhost(Ghost* ghost) {
    ghost->isEaten = true;
    gameState.score += 200 * (1 << gameState.ghostsEaten);
    gameState.ghostsEaten++;
    
    // Release speed boost if ghost had one
    if (ghost->hasSpeedBoost) {
        sem_post(&speedBoosts);
        ghost->hasSpeedBoost = false;
    }
}

// Respawn a ghost in the ghost house
void respawnGhost(Ghost* ghost) {
    // Release speed boost if ghost had one
    if (ghost->hasSpeedBoost) {
        sem_post(&speedBoosts);
        ghost->hasSpeedBoost = false;
    }
    
    ghost->isEaten = false;
    ghost->isBlue = false;
    ghost->inGhostHouse = true;
    
    // Set position in ghost house
    switch (ghost->id) {
        case 0:
            ghost->x = 9;
            ghost->y = 9;
            break;
        case 1:
            ghost->x = 8;
            ghost->y = 9;
            break;
        case 2:
            ghost->x = 10;
            ghost->y = 9;
            break;
        case 3:
            ghost->x = 9;
            ghost->y = 8;
            break;
    }
}

// Activate power pellet mode
void activatePowerPellet() {
    gameState.powerPelletActive = true;
    gameState.powerPelletTimer = 50;  // Power pellet lasts for 50 game ticks
    gameState.ghostsEaten = 0;
    
    // Turn all ghosts blue
    for (int i = 0; i < GHOST_COUNT; i++) {
        if (!ghosts[i].isEaten && !ghosts[i].inGhostHouse) {
            ghosts[i].isBlue = true;
        }
    }
}

// Deactivate power pellet mode
void deactivatePowerPellet() {
    gameState.powerPelletActive = false;
    
    // Turn ghosts back to normal
    for (int i = 0; i < GHOST_COUNT; i++) {
        ghosts[i].isBlue = false;
    }
}

// Draw the game board
void drawGame() {
    // Draw board
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            float posX = x * CELL_SIZE;
            float posY = y * CELL_SIZE;
            
            switch (gameState.board[y][x]) {
                case WALL:
                    glColor3f(0.0f, 0.0f, 1.0f);  // Blue walls
                    glBegin(GL_QUADS);
                    glVertex2f(posX, posY);
                    glVertex2f(posX + CELL_SIZE, posY);
                    glVertex2f(posX + CELL_SIZE, posY + CELL_SIZE);
                    glVertex2f(posX, posY + CELL_SIZE);
                    glEnd();
                    break;
                    
                case PELLET:
                    glColor3f(1.0f, 1.0f, 1.0f);  // White pellets
                    glBegin(GL_QUADS);
                    glVertex2f(posX + CELL_SIZE/3, posY + CELL_SIZE/3);
                    glVertex2f(posX + 2*CELL_SIZE/3, posY + CELL_SIZE/3);
                    glVertex2f(posX + 2*CELL_SIZE/3, posY + 2*CELL_SIZE/3);
                    glVertex2f(posX + CELL_SIZE/3, posY + 2*CELL_SIZE/3);
                    glEnd();
                    break;
                    
                case POWER_PELLET:
                    glColor3f(1.0f, 1.0f, 0.0f);  // Yellow power pellets
                    glBegin(GL_QUADS);
                    glVertex2f(posX + CELL_SIZE/4, posY + CELL_SIZE/4);
                    glVertex2f(posX + 3*CELL_SIZE/4, posY + CELL_SIZE/4);
                    glVertex2f(posX + 3*CELL_SIZE/4, posY + 3*CELL_SIZE/4);
                    glVertex2f(posX + CELL_SIZE/4, posY + 3*CELL_SIZE/4);
                    glEnd();
                    break;
                    
                case GHOST_HOUSE:
                    glColor3f(0.5f, 0.5f, 0.5f);  // Gray ghost house
                    glBegin(GL_QUADS);
                    glVertex2f(posX, posY);
                    glVertex2f(posX + CELL_SIZE, posY);
                    glVertex2f(posX + CELL_SIZE, posY + CELL_SIZE);
                    glVertex2f(posX, posY + CELL_SIZE);
                    glEnd();
                    break;
                    
                case GHOST_HOUSE_DOOR:
                    glColor3f(1.0f, 0.0f, 1.0f);  // Purple ghost house door
                    glBegin(GL_QUADS);
                    glVertex2f(posX, posY);
                    glVertex2f(posX + CELL_SIZE, posY);
                    glVertex2f(posX + CELL_SIZE, posY + CELL_SIZE);
                    glVertex2f(posX, posY + CELL_SIZE);
                    glEnd();
                    break;
            }
        }
    }
    // Draw score and lives info (top-left)
char infoStr[50];
sprintf(infoStr, "Score: %d   Lives: %d", gameState.score, gameState.lives);
glColor3f(1.0f, 1.0f, 1.0f);
drawText(10.0f, 20.0f, infoStr, GLUT_BITMAP_HELVETICA_18);

    // Draw Pac-Man
    float pacmanX = gameState.pacmanX * CELL_SIZE;
    float pacmanY = gameState.pacmanY * CELL_SIZE;
    glColor3f(1.0f, 1.0f, 0.0f);  // Yellow Pac-Man
    glBegin(GL_QUADS);
    glVertex2f(pacmanX + CELL_SIZE/6, pacmanY + CELL_SIZE/6);
    glVertex2f(pacmanX + 5*CELL_SIZE/6, pacmanY + CELL_SIZE/6);
    glVertex2f(pacmanX + 5*CELL_SIZE/6, pacmanY + 5*CELL_SIZE/6);
    glVertex2f(pacmanX + CELL_SIZE/6, pacmanY + 5*CELL_SIZE/6);
    glEnd();
    
    // Draw ghosts
    for (int i = 0; i < GHOST_COUNT; i++) {
        float ghostX = ghosts[i].x * CELL_SIZE;
        float ghostY = ghosts[i].y * CELL_SIZE;
        
        if (ghosts[i].isBlue) {
            glColor3f(0.0f, 0.0f, 1.0f);  // Blue when vulnerable
        } else if (ghosts[i].isEaten) {
            glColor3f(0.5f, 0.5f, 0.5f);  // Gray when eaten
        } else {
            // Different colors for different ghosts
            switch (i) {
                case 0:
                    glColor3f(1.0f, 0.0f, 0.0f);  // Red
                    break;
                case 1:
                    glColor3f(1.0f, 0.5f, 0.0f);  // Orange
                    break;
                case 2:
                    glColor3f(0.0f, 1.0f, 1.0f);  // Cyan
                    break;
                case 3:
                    glColor3f(1.0f, 0.0f, 1.0f);  // Magenta
                    break;
            }
        }
        
        glBegin(GL_QUADS);
        glVertex2f(ghostX + CELL_SIZE/6, ghostY + CELL_SIZE/6);
        glVertex2f(ghostX + 5*CELL_SIZE/6, ghostY + CELL_SIZE/6);
        glVertex2f(ghostX + 5*CELL_SIZE/6, ghostY + 5*CELL_SIZE/6);
        glVertex2f(ghostX + CELL_SIZE/6, ghostY + 5*CELL_SIZE/6);
        glEnd();
    }
    
   
    
    // Display game over message when the game is over
    if (gameState.gameOver) {
        char gameOverStr[50];
        sprintf(gameOverStr, "Game Over! Final Score: %d", gameState.score);
        glColor3f(1.0f, 0.0f, 0.0f);
        glRasterPos2f(WINDOW_WIDTH/2 - 80, WINDOW_HEIGHT/2);
        for (int i = 0; gameOverStr[i] != '\0'; i++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, gameOverStr[i]);
        }
    }
}

// GLUT display function
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    
    pthread_mutex_lock(&boardMutex);
    drawGame();
    pthread_mutex_unlock(&boardMutex);
    
    glutSwapBuffers();
}

// GLUT keyboard function
void keyboard(int key, int x, int y) {
    switch (key) {
        case GLUT_KEY_UP:
            gameState.pacmanDirection = UP;
            break;
        case GLUT_KEY_RIGHT:
            gameState.pacmanDirection = RIGHT;
            break;
        case GLUT_KEY_DOWN:
            gameState.pacmanDirection = DOWN;
            break;
        case GLUT_KEY_LEFT:
            gameState.pacmanDirection = LEFT;
            break;
    }
    glutPostRedisplay();
}

// GLUT timer function for updating the display
void timer(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);  
}

// Initialize OpenGL
void initOpenGL() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    glMatrixMode(GL_MODELVIEW);
}

// Main function
int main(int argc, char** argv) 
{
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("--> Pac-Man, the Game <--");
    
    // Initialize OpenGL
    initOpenGL();
    
    // Register GLUT callbacks
    glutDisplayFunc(display);
    glutSpecialFunc(keyboard);
    glutTimerFunc(16, timer, 0);
    
    // Initialize game state and synchronization primitives
    initGame();
    
    // Create threads
    pthread_create(&gameEngineThread, NULL, gameEngineFunction, NULL);
    pthread_create(&uiThread, NULL, uiFunction, NULL);
    
    for (int i = 0; i < GHOST_COUNT; i++) 
    {
        pthread_create(&ghostThreads[i], NULL, ghostFunction, (void*)&ghosts[i]);
    }
    
    // Start GLUT main loop (this handles the UI thread)
    glutMainLoop();
    
    // Cleanup 
    pthread_mutex_destroy(&boardMutex);
    pthread_mutex_destroy(&powerPelletMutex);
    pthread_mutex_destroy(&speedBoostMutex);
    sem_destroy(&ghostHouseKeys);
    sem_destroy(&ghostHousePermits);
    sem_destroy(&speedBoosts);
    
    return 0;
}

