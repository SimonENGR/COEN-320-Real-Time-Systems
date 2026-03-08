#include <iostream>
#include <vector>
#include <tuple>
#include <mutex>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <ctime>
#include <cmath>
#include <string>
#include <sys/dispatch.h> // For message passing

#define SIZE (6 * sizeof(int)) // Three position (X, Y, Z) + Three speed (X, Y, Z)

typedef struct {
    int aircraft1_id;
    int aircraft2_id;
    char decision; // 'Y' for yes, 'N' for no
} collision_msg;

typedef struct {
    int aircraft1_id;
    int aircraft2_id;
    bool NotifyAircrafts; // Flag to indicate whether to notify aircrafts
} shared_data;

std::mutex operatorConsoleMutex;

void printTimestamp() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    std::cout << "["
              << (ltm->tm_mday < 10 ? "0" : "") << ltm->tm_mday << "/"
              << (ltm->tm_mon + 1 < 10 ? "0" : "") << (ltm->tm_mon + 1) << "/"
              << (1900 + ltm->tm_year) << " - "
              << (ltm->tm_hour < 10 ? "0" : "") << ltm->tm_hour << ":"
              << (ltm->tm_min < 10 ? "0" : "") << ltm->tm_min << ":"
              << (ltm->tm_sec < 10 ? "0" : "") << ltm->tm_sec
              << "] ";
}
// This will calculate the next 2 mins of flight
// It is using a nested for loop of size (number of aircrafts * 120) 
void calculateProjections(std::vector<std::vector<std::tuple<int, int, int>>> &projections, int *radarData, int numAircrafts) {
    printTimestamp();
    std::cout << "Calculating position projections...\n";
    for (int id = 0; id < numAircrafts; ++id) {
        for (int t = 0; t < 120; ++t) { // 120 seconds (2 minutes)
            int x = radarData[id * 6 + 0] + radarData[id * 6 + 3] * t;
            int y = radarData[id * 6 + 1] + radarData[id * 6 + 4] * t;
            int z = radarData[id * 6 + 2] + radarData[id * 6 + 5] * t;
            projections[id][t] = std::make_tuple(x, y, z);
        }
    }
    printTimestamp();
    std::cout << "Position projections completed.\n";
}

// This will detect the collision safety violation
// It is using a triple for loop this time, because each aircraft needs to be compared to every other aircraft, for every second
void detectCollisions(std::vector<std::vector<std::tuple<int, int, int>>> &projections, int numAircrafts, int coid, shared_data *commData, sem_t *sem_sync) {
    printTimestamp();
    std::cout << "Checking for collisions...\n";
    for (int t = 0; t < 120; ++t) { // Check each second
        for (int id1 = 0; id1 < numAircrafts; ++id1) {
            for (int id2 = id1 + 1; id2 < numAircrafts; ++id2) {
                int x1, y1, z1, x2, y2, z2;
                std::tie(x1, y1, z1) = projections[id1][t]; //assign the tuple (X Y Z positions) the value of a given aircraft at a given time
                std::tie(x2, y2, z2) = projections[id2][t]; //assign the tuple (X Y Z positions) the value of a given aircraft at a given time

                int xDistance = std::abs(x1 - x2); //calculate the distance between the aircrafts (X position)
                int yDistance = std::abs(y1 - y2); //calculate the distance between the aircrafts (Y position)
                int zDistance = std::abs(z1 - z2); //calculate the distance between the aircrafts (Z position)

                bool xClose = xDistance <= 3000; // check if the distance is safe or not
                bool yClose = yDistance <= 3000; // check if the distance is safe or not 
                bool zClose = zDistance <= 1000; // check if the distance is safe or not

                if (xClose && yClose && zClose) {  // if all three condition fail their check (thus safety violation occurs)
                    printTimestamp();
                    std::cout << "Collision detected at t=" << t << " seconds between Aircraft " << id1 << " and Aircraft " << id2 << "\n";

                    operatorConsoleMutex.lock(); // Prevent flooding Operator Console

                    collision_msg msg;
                    msg.aircraft1_id = id1;
                    msg.aircraft2_id = id2;

                    collision_msg reply;
                    int status = MsgSend(coid, &msg, sizeof(msg), &reply, sizeof(reply)); // send message to operator
                    if (status == -1) {
                        perror("MsgSend to OperatorConsole failed");
                    } else {
                        if (reply.decision == 'Y') { // receive response from operator
                            commData->aircraft1_id = id1;
                            commData->aircraft2_id = id2;
                            commData->NotifyAircrafts = true;
                        }
                        sem_post(sem_sync); // used with Communication System
                    }

                    operatorConsoleMutex.unlock(); 
                }
            }
        }
    }
    printTimestamp();
    std::cout << "Collision check completed.\n";
}

void writeToDataDisplay(int *radarData, int *displayData, int numAircrafts) {
    printTimestamp();
    std::cout << "Writing radar data to DataDisplay shared memory...\n";
    for (int id = 0; id < numAircrafts; ++id) {
        for (int j = 0; j < 6; ++j) {
            displayData[id * 6 + j] = radarData[id * 6 + j];
        }
    }
    printTimestamp();
    std::cout << "Data written to DataDisplay shared memory.\n";
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

    // Shared memory and semaphore for Radar -> CompSys
    const char *shm_radar = "/Radar_CompSys_shm";
    const char *sem_radar_name = "/Radar_CompSys_sem";
    int shm_fd_radar;
    int *radarData;
    sem_t *sem_radar;

    // Shared memory and semaphore for CompSys -> DataDisplay
    const char *shm_display = "/CompSys_DataDisplay_shm";
    const char *sem_display_name = "/CompSys_DataDisplay_sem";
    int shm_fd_display;
    int *displayData;
    sem_t *sem_display;

    // Shared memory for CompSys -> CommunicationSys
    const char *shm_comm = "/CompSys_CommSys_shm";
    int shm_fd_comm;
    shared_data *commData;

    // Semaphore for synchronization between ComputerSystem and CommunicationSystem
    const char *sem_sync_name = "/CompSys_CommSys_sync_sem";
    sem_t *sem_sync;


    // Create shared memory object for CompSys -> CommunicationSys
    shm_fd_comm = shm_open(shm_comm, O_CREAT | O_RDWR, 0666);
    if (shm_fd_comm == -1) {
        perror("shm_open for CommunicationSys failed");
        return 1;
    }

    ftruncate(shm_fd_comm, sizeof(shared_data)); // Allocate memory for shared_data
    commData = static_cast<shared_data *>(mmap(0, sizeof(shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_comm, 0));
    if (commData == MAP_FAILED) {
        perror("mmap for CommunicationSys failed");
        return 1;
    }

    // Initialize shared data
    commData->NotifyAircrafts = false;

    // Open shared memory object for Radar -> CompSys
    shm_fd_radar = shm_open(shm_radar, O_RDONLY, 0666);
    if (shm_fd_radar == -1) {
        perror("shm_open for Radar failed");
        return 1;
    }

    ftruncate(shm_fd_radar, numAircrafts * SIZE); // Adjust shared memory size dynamically
    radarData = static_cast<int *>(mmap(0, numAircrafts * SIZE, PROT_READ, MAP_SHARED, shm_fd_radar, 0));
    if (radarData == MAP_FAILED) {
        perror("mmap for Radar failed");
        return 1;
    }

    // Open semaphore for Radar -> CompSys
    sem_radar = sem_open(sem_radar_name, O_RDONLY);
    if (sem_radar == SEM_FAILED) {
        perror("sem_open for Radar failed");
        return 1;
    }

    // Create shared memory object for CompSys -> DataDisplay
    shm_fd_display = shm_open(shm_display, O_CREAT | O_RDWR, 0666);
    if (shm_fd_display == -1) {
        perror("shm_open for DataDisplay failed");
        return 1;
    }

    ftruncate(shm_fd_display, numAircrafts * SIZE); // Adjust shared memory size dynamically
    displayData = static_cast<int *>(mmap(0, numAircrafts * SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_display, 0));
    if (displayData == MAP_FAILED) {
        perror("mmap for DataDisplay failed");
        return 1;
    }

    // Open or create semaphore for CompSys -> DataDisplay
    sem_display = sem_open(sem_display_name, O_CREAT, 0666, 0);
    if (sem_display == SEM_FAILED) {
        perror("sem_open for DataDisplay failed");
        return 1;
    }

    // Open or create synchronization semaphore
    sem_sync = sem_open(sem_sync_name, O_CREAT, 0666, 0);
    if (sem_sync == SEM_FAILED) {
        perror("sem_open for sync failed");
        return 1;
    }

    // Connect to the Operator Console
    const char *server_name = "OperatorConsole";
    int coid = name_open(server_name, 0);
    if (coid == -1) {
        perror("name_open for OperatorConsole failed");
        return 1;
    }
    // This format was the only way i found to easily compare the flight predictions
    std::vector<std::vector<std::tuple<int, int, int>>> projections(numAircrafts, std::vector<std::tuple<int, int, int>>(120));
    // This is the main execution loop
    // Logically it goes in this order 
    // -> calculate the next 2 mins of flight for every aircraft
    // -> check for potential collision (if so contact operator)
    // -> Regardless of collision danger, send data to DataDisplay
    // -> sem_post the display
    
    for (int i = 0; i < 30; ++i) {
        sem_wait(sem_radar);
        printTimestamp();
        std::cout << "Processing Radar data...\n";
        calculateProjections(projections, radarData, numAircrafts);
        detectCollisions(projections, numAircrafts, coid, commData, sem_sync);
        writeToDataDisplay(radarData, displayData, numAircrafts);
        sem_post(sem_display);
        printTimestamp();
        std::cout << "Data written to DataDisplay shared memory\n";
    }

    sem_close(sem_radar);
    sem_close(sem_display);
    sem_close(sem_sync);
    sem_unlink(sem_display_name);
    sem_unlink(sem_sync_name);
    munmap(radarData, numAircrafts * SIZE);
    munmap(displayData, numAircrafts * SIZE);
    name_close(coid);

    return 0;
}
