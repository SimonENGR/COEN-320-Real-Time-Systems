#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <mutex>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <fstream>

std::mutex consoleMutex; // Global mutex for console output

struct Aircraft {
    int speed[3];       // Speed in ft/s (X, Y, Z)
    int initialPos[3];  // Initial position (X, Y, Z)
    int currentPos[3];  // Current position (X, Y, Z)
    bool ascend;        // Ascend flag (used in collision avoidance solution)
    bool descend;       // Descend flag (used in collision avoidance solution)
    int cycleCount;     // Countdown for cycles left for ascend/descend
};

struct Notification {
    bool ascend;
    bool descend;
    int cycleCount;
};
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

// Function to simulate an aircraft (same logic as before but adapted for dynamic aircraft count)
void simulateAircraft(int id, int totalAircrafts) {
    const std::string shm_radar_name = "/Aircraft_Radar_shm_" + std::to_string(id);
    const std::string shm_notification_name = "/CommSys_Notify_shm";
    const std::string sem_name = "/Aircraft_Radar_sem_" + std::to_string(id);
    const int RADAR_SIZE = 6 * sizeof(int); // Three position values (X, Y, Z) + Three speed values (X, Y, Z)
    const int NOTIFICATION_SIZE = totalAircrafts * sizeof(Notification); // Dynamic number of aircrafts

    // Radar Shared Memory
    int shm_fd_radar;
    int *sharedDataRadar;

    // Notification Shared Memory
    int shm_fd_notification;
    Notification *sharedNotifications;

    // Semaphore for Radar
    sem_t *sem;

    Aircraft aircraft;

    // Define aircraft based on id (switch case logic restored)
    switch (id) {
        case 0: //Heading straight south, Starting Top Left Low alt (Will purposefully collide with A4)
            aircraft = {{0, -900, 0}, {17000, 115000, 20000}, {17000, 115000, 20000}, false, false, 0};
            break;

        case 1: //Heading North west, Starting bottom right Low alt
            aircraft = {{-1200, 1200, 0}, {115000, 30000, 25000}, {115000, 30000, 25000}, false, false, 0};
            break;

        case 2: //Heading North west, Starting bottom right Low alt
            aircraft = {{-1200, 1200, 0}, {115000, 40000, 30000}, {115000, 40000, 30000}, false, false, 0};
            break;

        case 3: //Heading East, Starting middle left, medium alt
            aircraft = {{1200, 0, 0}, {15000, 50000, 35000}, {15000, 50000, 35000}, false, false, 0};
            break;

        case 4: //Heading North, Starting bottom left, medium alt
            aircraft = {{0, 900, 0}, {17000, 17000, 20000}, {17000, 17000, 20000}, false, false, 0};
            break;
        case 5: // Heading South-West, Starting Top Right Low Alt
            aircraft = {{-900, -900, 0}, {115000, 115000, 20000}, {115000, 115000, 20000}, false, false, 0};
            break;

        case 6: // Heading West, Starting Top Right Low Alt
            aircraft = {{-1200, 0, 0}, {115000, 115000, 30000}, {115000, 115000, 30000}, false, false, 0};
            break;

        case 7: // Heading South, Starting Top Right Medium Alt
            aircraft = {{0, -900, 0}, {115000, 115000, 35000}, {115000, 115000, 35000}, false, false, 0};
            break;

        case 8: // Heading West, Starting Top Center Low Alt
            aircraft = {{-900, 0, 0}, {115000, 60000, 20000}, {115000, 60000, 20000}, false, false, 0};
            break;

        case 9: // Heading South-West, Starting Top Center Medium Alt
            aircraft = {{-900, -900, 0}, {115000, 60000, 35000}, {115000, 60000, 35000}, false, false, 0};
            break;

        case 10: // Heading West, Starting Top Right High Alt
            aircraft = {{-1200, 0, 0}, {115000, 115000, 40000}, {115000, 115000, 40000}, false, false, 0};
            break;

        case 11: // Heading North, Starting Bottom Right Low Alt
            aircraft = {{0, 1200, 0}, {115000, 15000, 20000}, {115000, 15000, 20000}, false, false, 0};
            break;

        case 12: // Heading North-West, Starting Bottom Right Medium Alt
            aircraft = {{-900, 1200, 0}, {115000, 15000, 35000}, {115000, 15000, 35000}, false, false, 0};
            break;

        case 13: // Heading North, Starting Bottom Center Low Alt
            aircraft = {{0, 900, 0}, {60000, 15000, 25000}, {60000, 15000, 25000}, false, false, 0};
            break;

        case 14: // Heading North-West, Starting Bottom Center Medium Alt
            aircraft = {{-1200, 900, 0}, {60000, 15000, 35000}, {60000, 15000, 35000}, false, false, 0};
            break;

        case 15: // Heading North-East, Starting Bottom Left Low Alt
            aircraft = {{900, 1200, 0}, {17000, 15000, 20000}, {17000, 15000, 20000}, false, false, 0};
            break;

        case 16: // Heading West, Starting Bottom Left Medium Alt
            aircraft = {{-900, 0, 0}, {15000, 15000, 25000}, {15000, 15000, 25000}, false, false, 0};
            break;

        case 17: // Heading North-East, Starting Top Left Low Alt
            aircraft = {{900, 1200, 0}, {15000, 115000, 20000}, {15000, 115000, 20000}, false, false, 0};
            break;

        case 18: // Heading South, Starting Top Center Medium Alt
            aircraft = {{0, -900, 0}, {60000, 115000, 35000}, {60000, 115000, 35000}, false, false, 0};
            break;

        case 19: // Heading South-East, Starting Top Center High Alt
            aircraft = {{900, -900, 0}, {60000, 115000, 40000}, {60000, 115000, 40000}, false, false, 0};
            break;

        case 20: // Heading South-West, Starting Top Right Low Alt
            aircraft = {{-1200, -900, 0}, {115000, 115000, 25000}, {115000, 115000, 25000}, false, false, 0};
            break;

        case 21: // Heading West, Starting Middle Right Medium Alt
            aircraft = {{-900, 0, 0}, {115000, 60000, 35000}, {115000, 60000, 35000}, false, false, 0};
            break;

        case 22: // Heading South-East, Starting Middle Left Medium Alt
            aircraft = {{900, -900, 0}, {15000, 60000, 35000}, {15000, 60000, 35000}, false, false, 0};
            break;

        case 23: // Heading South-West, Starting Middle Right Medium Alt
            aircraft = {{-900, -900, 0}, {115000, 60000, 35000}, {115000, 60000, 35000}, false, false, 0};
            break;

        case 24: // Heading North, Starting Bottom Right High Alt
            aircraft = {{0, 1200, 0}, {115000, 15000, 40000}, {115000, 15000, 40000}, false, false, 0};
            break;

        case 25: // Heading West, Starting Bottom Right Medium Alt
            aircraft = {{-900, 0, 0}, {115000, 15000, 25000}, {115000, 15000, 25000}, false, false, 0};
            break;
        case 26: // Heading South, Starting Top Left High Alt
            aircraft = {{0, -900, 0}, {15000, 115000, 40000}, {15000, 115000, 40000}, false, false, 0};
            break;

        case 27: // Heading South-East, Starting Top Left Medium Alt
            aircraft = {{900, -900, 0}, {15000, 115000, 30000}, {15000, 115000, 30000}, false, false, 0};
            break;

        case 28: // Heading South-West, Starting Top Left Medium Alt
            aircraft = {{-900, -900, 0}, {15000, 115000, 30000}, {15000, 115000, 30000}, false, false, 0};
            break;

        case 29: // Heading West, Starting Top Right Medium Alt
            aircraft = {{-1200, 0, 0}, {115000, 115000, 30000}, {115000, 115000, 30000}, false, false, 0};
            break;

        case 30: // Heading South, Starting Top Right High Alt
            aircraft = {{0, -900, 0}, {115000, 115000, 40000}, {115000, 115000, 40000}, false, false, 0};
            break;

        case 31: // Heading North-East, Starting Bottom Right Low Alt
            aircraft = {{900, 1200, 0}, {115000, 15000, 20000}, {115000, 15000, 20000}, false, false, 0};
            break;

        case 32: // Heading South, Starting Top Center Medium Alt
            aircraft = {{0, -1200, 0}, {60000, 115000, 30000}, {60000, 115000, 30000}, false, false, 0};
            break;

        case 33: // Heading South-East, Starting Top Center Low Alt
            aircraft = {{900, -900, 0}, {60000, 115000, 20000}, {60000, 115000, 20000}, false, false, 0};
            break;

        case 34: // Heading West, Starting Middle Right Low Alt
            aircraft = {{-900, 0, 0}, {115000, 60000, 25000}, {115000, 60000, 25000}, false, false, 0};
            break;

        case 35: // Heading North, Starting Bottom Center High Alt
            aircraft = {{0, 900, 0}, {60000, 15000, 40000}, {60000, 15000, 40000}, false, false, 0};
            break;

        case 36: // Heading North-West, Starting Bottom Center Medium Alt
            aircraft = {{-900, 1200, 0}, {60000, 15000, 30000}, {60000, 15000, 30000}, false, false, 0};
            break;

        case 37: // Heading North, Starting Bottom Left High Alt
            aircraft = {{0, 1200, 0}, {15000, 15000, 40000}, {15000, 15000, 40000}, false, false, 0};
            break;

        case 38: // Heading South-West, Starting Middle Right Medium Alt
            aircraft = {{-900, -900, 0}, {115000, 60000, 30000}, {115000, 60000, 30000}, false, false, 0};
            break;

        case 39: // Heading North, Starting Bottom Left Medium Alt
            aircraft = {{0, 900, 0}, {15000, 15000, 30000}, {15000, 15000, 30000}, false, false, 0};
            break;

        case 40: // Heading South-West, Starting Middle Right High Alt
            aircraft = {{-900, -1200, 0}, {115000, 60000, 40000}, {115000, 60000, 40000}, false, false, 0};
            break;

        case 41: // Heading South, Starting Top Left Low Alt
            aircraft = {{0, -900, 0}, {15000, 115000, 20000}, {15000, 115000, 20000}, false, false, 0};
            break;

        case 42: // Heading West, Starting Middle Center Low Alt
            aircraft = {{-1200, 0, 0}, {60000, 60000, 20000}, {60000, 60000, 20000}, false, false, 0};
            break;

        case 43: // Heading South-East, Starting Middle Left High Alt
            aircraft = {{900, -900, 0}, {15000, 60000, 40000}, {15000, 60000, 40000}, false, false, 0};
            break;

        case 44: // Heading North, Starting Bottom Left Low Alt
            aircraft = {{0, 900, 0}, {15000, 15000, 20000}, {15000, 15000, 20000}, false, false, 0};
            break;

        case 45: // Heading South, Starting Top Center Low Alt
            aircraft = {{0, -1200, 0}, {60000, 115000, 20000}, {60000, 115000, 20000}, false, false, 0};
            break;

        case 46: // Heading South-West, Starting Top Right High Alt
            aircraft = {{-900, -900, 0}, {115000, 115000, 40000}, {115000, 115000, 40000}, false, false, 0};
            break;

        case 47: // Heading North-East, Starting Bottom Right High Alt
            aircraft = {{900, 1200, 0}, {115000, 15000, 40000}, {115000, 15000, 40000}, false, false, 0};
            break;

        case 48: // Heading North-West, Starting Bottom Left Medium Alt
            aircraft = {{-900, 900, 0}, {15000, 15000, 30000}, {15000, 15000, 30000}, false, false, 0};
            break;

        case 49: // Heading West, Starting Top Left Medium Alt
            aircraft = {{-1200, 0, 0}, {15000, 115000, 30000}, {15000, 115000, 30000}, false, false, 0};
            break;
    }

    // If the aircraft ID is greater than the selected load size, exit early
    if (id >= totalAircrafts) {
        return;
    }

    // Radar shared memory setup
    shm_fd_radar = shm_open(shm_radar_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd_radar == -1) {
        perror("shm_open Radar failed");
        return;
    }
    ftruncate(shm_fd_radar, RADAR_SIZE);
    sharedDataRadar = static_cast<int *>(mmap(0, RADAR_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_radar, 0));
    if (sharedDataRadar == MAP_FAILED) {
        perror("mmap Radar failed");
        return;
    }

    // Notification shared memory setup
    shm_fd_notification = shm_open(shm_notification_name.c_str(), O_RDONLY, 0666);
    if (shm_fd_notification == -1) {
        perror("shm_open Notification failed");
        return;
    }
    sharedNotifications = static_cast<Notification *>(mmap(0, NOTIFICATION_SIZE, PROT_READ, MAP_SHARED, shm_fd_notification, 0));
    if (sharedNotifications == MAP_FAILED) {
        perror("mmap Notification failed");
        return;
    }

    // Semaphore setup
    sem = sem_open(sem_name.c_str(), O_CREAT, 0666, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        return;
    }

    // Main simulation loop
    std::mutex fileMutex; // mutex for file writing

    for (int i = 0; i < 90; ++i) {
        Notification notification = sharedNotifications[id];
        if (notification.ascend && !aircraft.ascend) {
            aircraft.ascend = true;
            aircraft.descend = false;
            aircraft.cycleCount = notification.cycleCount;
        } else if (notification.descend && !aircraft.descend) {
            aircraft.ascend = false;
            aircraft.descend = true;
            aircraft.cycleCount = notification.cycleCount;
        } else if (!notification.ascend && !notification.descend) {
            aircraft.ascend = false;
            aircraft.descend = false;
        }

        if (aircraft.ascend && aircraft.cycleCount > 0) {
            aircraft.speed[2] = 100;
            aircraft.cycleCount--;
        } else if (aircraft.descend && aircraft.cycleCount > 0) {
            aircraft.speed[2] = -100;
            aircraft.cycleCount--;
        } else {
            aircraft.speed[2] = 0;
        }

        for (int j = 0; j < 3; ++j) {
            aircraft.currentPos[j] += aircraft.speed[j];
        }

        for (int j = 0; j < 3; ++j) {
            sharedDataRadar[j] = aircraft.currentPos[j];
        }
        for (int j = 3; j < 6; ++j) {
            sharedDataRadar[j] = aircraft.speed[j - 3];
        }

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            printTimestamp();
            std::cout << "Aircraft " << id << " Position: (" << sharedDataRadar[0] << ", " << sharedDataRadar[1] << ", " << sharedDataRadar[2] << ") | Speed: (" << sharedDataRadar[3] << ", " << sharedDataRadar[4] << ", " << sharedDataRadar[5] << ")";
            if (aircraft.cycleCount > 0) {
                std::cout << " - Currently " << (aircraft.ascend ? "ascending" : "descending") << ", " << aircraft.cycleCount << " cycles remaining.";
            }
            std::cout << std::endl;
        }

        // Save flight log every 20 seconds
        if (i % 20 == 0) {
            std::lock_guard<std::mutex> lock(fileMutex); // Lock for file writing
            std::ofstream logFile("flight_log.txt", std::ios::app); // Open file in append mode
            if (logFile.is_open()) {
                logFile << "Timestamp: ";
                printTimestamp();
                logFile << "\nAircraft " << id << " -> Position: ("
                        << sharedDataRadar[0] << ", " << sharedDataRadar[1] << ", " << sharedDataRadar[2]
                        << ") | Speed: (" << sharedDataRadar[3] << ", " << sharedDataRadar[4] << ", " << sharedDataRadar[5] << ")";
                if (aircraft.cycleCount > 0) {
                    logFile << " - Currently " << (aircraft.ascend ? "ascending" : "descending") << ", " << aircraft.cycleCount << " cycles remaining.\n";
                } else {
                    logFile << "\n";
                }
                logFile.close();
            } else {
                std::cerr << "Error: Unable to open flight log file.\n";
            }
        }

        sem_post(sem);
        sleep(1);
    }


    sem_close(sem);
    sem_unlink(sem_name.c_str());
    munmap(sharedDataRadar, RADAR_SIZE);
    munmap(sharedNotifications, NOTIFICATION_SIZE);
    close(shm_fd_radar);
    close(shm_fd_notification);
}

int main() {
    int loadChoice;
    const std::string shm_notification_name = "/CommSys_Notify_shm";

    std::cout << "Choose load level (1 - small, 2 - medium, 3 - high, 4 - very high): ";
    std::cin >> loadChoice;

    int numAircrafts;
    switch (loadChoice) {
        case 1:
            numAircrafts = 5;
            break;
        case 2:
            numAircrafts = 15;
            break;
        case 3:
            numAircrafts = 30;
            break;
        case 4:
            numAircrafts = 50;
            break;
        default:
            std::cout << "Invalid choice! Defaulting to small load.\n";
            numAircrafts = 5;
            break;
    }

    const int NOTIFICATION_SIZE = numAircrafts * sizeof(Notification);
    int shm_fd_notification = shm_open(shm_notification_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd_notification == -1) {
        perror("shm_open Notification failed");
        return 1;
    }

    ftruncate(shm_fd_notification, NOTIFICATION_SIZE);
    Notification *notifications = static_cast<Notification *>(mmap(0, NOTIFICATION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_notification, 0));
    if (notifications == MAP_FAILED) {
        perror("mmap Notification failed");
        return 1;
    }

    for (int i = 0; i < numAircrafts; ++i) {
        notifications[i].ascend = false;
        notifications[i].descend = false;
        notifications[i].cycleCount = 0;
    }

    std::vector<std::thread> aircraftThreads;
    for (int i = 0; i < numAircrafts; ++i) {
        aircraftThreads.push_back(std::thread(simulateAircraft, i, numAircrafts));
    }

    for (auto &t : aircraftThreads) {
        t.join();
    }

    std::cout << "All Aircraft simulations complete.\n";

    munmap(notifications, NOTIFICATION_SIZE);
    close(shm_fd_notification);
    shm_unlink(shm_notification_name.c_str());

    return 0;
}
