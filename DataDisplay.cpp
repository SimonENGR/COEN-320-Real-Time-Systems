#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <string>
#include <vector>
#include <iomanip>

// Constants for Zone boundaries
const int xMin = 15000, xMax = 115000;
const int yMin = 15000, yMax = 115000;

// Grid dimensions for top-down perspective
const int GRID_WIDTH = 50;  // Number of columns
const int GRID_HEIGHT = 25; // Number of rows

// Calculate cell size based on zone dimensions
const int CELL_SIZE_X = (xMax - xMin) / GRID_WIDTH;
const int CELL_SIZE_Y = (yMax - yMin) / GRID_HEIGHT;

void printGrid(const int *data, int numAircrafts) {
    std::vector<std::string> grid(GRID_HEIGHT, std::string(GRID_WIDTH, '.')); // Initialize grid with dots

    for (int id = 0; id < numAircrafts; ++id) {
        // Get X, Y positions
        int xPos = data[id * 6 + 0];
        int yPos = data[id * 6 + 1];
        int zPos = data[id * 6 + 2];

        // Calculate grid cell based on positions
        int col = (xPos - xMin) / CELL_SIZE_X;
        int row = GRID_HEIGHT - 1 - (yPos - yMin) / CELL_SIZE_Y; // Flip vertically for top-down view

        // Ensure positions are within bounds
        if (col >= 0 && col < GRID_WIDTH && row >= 0 && row < GRID_HEIGHT) {
            grid[row][col] = '*'; // Aircraft symbol
        }

        // Print Aircraft info near the grid
        std::cout << "* (A" << id << " - Z: " << zPos << ")" << std::endl;
    }

    // Print the top labels
    std::cout << "Zone View (Top-down X/Y perspective):\n";
    std::cout << "X : " << xMin << ", Y : " << yMax << std::setw(GRID_WIDTH * 2 - 10) << "X : " << xMax << ", Y : " << yMax << "\n";

    // Print the grid
    for (const auto &line : grid) {
        std::cout << line << std::endl;
    }

    // Print the bottom labels
    std::cout << "X : " << xMin << ", Y : " << yMin << std::setw(GRID_WIDTH * 2 - 10) << "X : " << xMax << ", Y : " << yMin << "\n";
    std::cout << std::endl;
}

int main() {
    // Ask the user to choose the load level
    int loadChoice;
    std::cout << "Choose load level (1 - small, 2 - medium, 3 - high, 4 - very high): ";
    std::cin >> loadChoice;

    // Determine the number of aircrafts based on the load level
    int numAircrafts;
    switch (loadChoice) {
        case 1:
            numAircrafts = 5;  // Small load
            break;
        case 2:
            numAircrafts = 15; // Medium load
            break;
        case 3:
            numAircrafts = 30; // High load
            break;
        case 4:
            numAircrafts = 50; // Very high load
            break;
        default:
            std::cout << "Invalid choice! Defaulting to small load.\n";
            numAircrafts = 5;
            break;
    }

    const int SIZE = 6 * sizeof(int); // Three position (X, Y, Z) + Three speed (X, Y, Z)
    const int DISPLAY_MEMORY_SIZE = numAircrafts * SIZE; // Dynamically allocate memory for aircraft data

    // Shared memory and semaphore for DataDisplay
    const char *shm_display = "/CompSys_DataDisplay_shm";
    const char *sem_display_name = "/CompSys_DataDisplay_sem";
    int shm_fd_display;
    int *displayData;
    sem_t *sem_display;

    // Open shared memory object
    shm_fd_display = shm_open(shm_display, O_RDONLY, 0666);
    if (shm_fd_display == -1) {
        perror("shm_open for DataDisplay failed");
        return 1;
    }

    // Map shared memory to process address space
    displayData = static_cast<int *>(mmap(0, DISPLAY_MEMORY_SIZE, PROT_READ, MAP_SHARED, shm_fd_display, 0));
    if (displayData == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    // Open semaphore
    sem_display = sem_open(sem_display_name, O_RDONLY);
    if (sem_display == SEM_FAILED) {
        perror("sem_open failed");
        return 1;
    }

    // Read and display grid data
    for (int i = 0; i < 30; ++i) {
        sem_wait(sem_display); // Wait for CompSys to indicate new data

        std::cout << "Displaying data from shared memory...\n";
        printGrid(displayData, numAircrafts); // Visualize aircraft data on the grid dynamically

        sleep(3); // Simulate delay between updates
    }

    // Clean up
    sem_close(sem_display);
    munmap(displayData, DISPLAY_MEMORY_SIZE);
    close(shm_fd_display);

    return 0;
}
