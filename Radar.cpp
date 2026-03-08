#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <ctime>
#include <string>
#include <vector>
#include <iomanip>

// Constants for Aircrafts and Zone
#define SIZE (6 * sizeof(int)) // Three position (X, Y, Z) + Three speed (X, Y, Z)

// Define the boundaries of the 3D zone
const int xMin = 15000, xMax = 115000;
const int yMin = 15000, yMax = 115000;
const int zMin = 15000, zMax = 40000;

// Function to print the current timestamp
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

    // Dynamically allocate the aircraftInZone array
    std::vector<bool> aircraftInZone(numAircrafts, false);

    // Shared memory and semaphore for Radar -> CompSys
    const char *shm_radar = "/Radar_CompSys_shm";
    const char *sem_radar_name = "/Radar_CompSys_sem";
    int shm_fd_radar;
    int *radarData;
    sem_t *sem_radar;

    // Open/create shared memory for Radar -> CompSys
    shm_fd_radar = shm_open(shm_radar, O_CREAT | O_RDWR, 0666);
    if (shm_fd_radar == -1) {
        perror("shm_open for Radar failed");
        return 1;
    }

    // Adjust shared memory size based on the number of aircrafts
    ftruncate(shm_fd_radar, numAircrafts * SIZE); // Memory for all aircrafts
    radarData = static_cast<int *>(mmap(0, numAircrafts * SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_radar, 0));
    if (radarData == MAP_FAILED) {
        perror("mmap for Radar failed");
        return 1;
    }

    // Open or create semaphore
    sem_radar = sem_open(sem_radar_name, O_CREAT, 0666, 0);
    if (sem_radar == SEM_FAILED) {
        perror("sem_open for Radar failed");
        return 1;
    }

    sleep(3); // Let aircraft simulations begin
    std::cout << "Radar starting data transfer...\n";

    // Loop to read from all aircrafts and forward to CompSys
    for (int i = 0; i < 30; ++i) { // Perform 30 iterations of reading
        for (int id = 0; id < numAircrafts; ++id) {
            // Shared memory and semaphore names for each aircraft
            std::string shm_aircraft_name = "/Aircraft_Radar_shm_" + std::to_string(id);
            std::string sem_aircraft_name = "/Aircraft_Radar_sem_" + std::to_string(id);

            int shm_fd_aircraft;
            int *aircraftData;
            sem_t *sem_aircraft;

            // Open shared memory for the aircraft
            shm_fd_aircraft = shm_open(shm_aircraft_name.c_str(), O_RDONLY, 0666);
            if (shm_fd_aircraft == -1) {
                perror(("shm_open for Aircraft " + std::to_string(id) + " failed").c_str());
                continue;
            }

            aircraftData = static_cast<int *>(mmap(0, SIZE, PROT_READ, MAP_SHARED, shm_fd_aircraft, 0));
            if (aircraftData == MAP_FAILED) {
                perror(("mmap for Aircraft " + std::to_string(id) + " failed").c_str());
                continue;
            }

            // Open semaphore for the aircraft
            sem_aircraft = sem_open(sem_aircraft_name.c_str(), O_RDONLY);
            if (sem_aircraft == SEM_FAILED) {
                perror(("sem_open for Aircraft " + std::to_string(id) + " failed").c_str());
                continue;
            }

            sem_wait(sem_aircraft); // Wait for the Aircraft to provide new data

            // Write data into Radar -> CompSys shared memory at the correct offset
            for (int j = 0; j < 6; ++j) {
                radarData[id * 6 + j] = aircraftData[j];
            }

            // Check if aircraft is inside the zone
            bool inZone = (radarData[id * 6 + 0] >= xMin && radarData[id * 6 + 0] <= xMax) &&
                          (radarData[id * 6 + 1] >= yMin && radarData[id * 6 + 1] <= yMax) &&
                          (radarData[id * 6 + 2] >= zMin && radarData[id * 6 + 2] <= zMax);

            if (inZone && !aircraftInZone[id]) {
                // Aircraft entering zone for the first time
                aircraftInZone[id] = true;
                printTimestamp();
                std::cout << "Aircraft " << id << " entered the zone, tracking coordinates until zone is left\n";
            } else if (!inZone && aircraftInZone[id]) {
                // Aircraft exiting the zone
                aircraftInZone[id] = false;
            }

            // Write text data about each aircraft's position and speed
            printTimestamp();
            std::cout << "Received Data From Aircraft " << id << " -> Position: ("
                      << radarData[id * 6 + 0] << ", " << radarData[id * 6 + 1] << ", " << radarData[id * 6 + 2]
                      << ") | Speed: ("
                      << radarData[id * 6 + 3] << ", " << radarData[id * 6 + 4] << ", " << radarData[id * 6 + 5]
                      << ")" << std::endl;

            // Clean up for this Aircraft
            munmap(aircraftData, SIZE);
            close(shm_fd_aircraft);
            sem_close(sem_aircraft);
        }

        // After checking all aircraft, print if none are in the zone
        bool anyInZone = false;
        for (bool inZoneFlag : aircraftInZone) {
            if (inZoneFlag) {
                anyInZone = true;
                break;
            }
        }

        if (!anyInZone) {
            printTimestamp();
            std::cout << "No Aircrafts in the zone yet\n";
        }

        // Signal CompSys to indicate new data is available
        sem_post(sem_radar);
        sleep(3); // Simulate delay between updates
    }

    // Clean up after the radar loop ends
    sem_close(sem_radar);
    sem_unlink(sem_radar_name);
    munmap(radarData, numAircrafts * SIZE);
    close(shm_fd_radar);

    return 0;
}
