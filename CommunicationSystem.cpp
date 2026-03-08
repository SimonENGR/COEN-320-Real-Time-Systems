#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <ctime>
#include <string>

// Structure for reading from shared memory
typedef struct {
    int aircraft1_id;
    int aircraft2_id;
    bool NotifyAircrafts;
} shared_data;

// New notification structure to write into shared memory
struct Notification {
    bool ascend;
    bool descend;
    int cycleCount;
};

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

    // Shared memory for CompSys -> CommunicationSys
    const char *shm_comm = "/CompSys_CommSys_shm";
    int shm_fd_comm;
    shared_data *commData;

    // Shared memory for notifications (CommunicationSys -> Aircraft)
    const char *shm_notification = "/CommSys_Notify_shm";
    int shm_fd_notification;
    Notification *notifications;

    const int NOTIFICATION_SIZE = numAircrafts * sizeof(Notification); // Dynamic size for the number of aircrafts

    // Semaphore for synchronization between ComputerSystem and CommunicationSystem
    const char *sem_sync_name = "/CompSys_CommSys_sync_sem";
    sem_t *sem_sync;

    // Open shared memory object for CompSys -> CommunicationSys
    shm_fd_comm = shm_open(shm_comm, O_RDWR, 0666);
    if (shm_fd_comm == -1) {
        perror("shm_open for CommunicationSys failed");
        return 1;
    }

    commData = static_cast<shared_data *>(mmap(0, sizeof(shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_comm, 0));
    if (commData == MAP_FAILED) {
        perror("mmap for CommunicationSys failed");
        return 1;
    }

    // Initialize the NotifyAircrafts flag to false at startup
    commData->NotifyAircrafts = false;

    // Open or create shared memory for notifications
    shm_fd_notification = shm_open(shm_notification, O_CREAT | O_RDWR, 0666);
    if (shm_fd_notification == -1) {
        perror("shm_open for Notification failed");
        return 1;
    }

    ftruncate(shm_fd_notification, NOTIFICATION_SIZE); // Set the size of shared memory dynamically
    notifications = static_cast<Notification *>(mmap(0, NOTIFICATION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_notification, 0));
    if (notifications == MAP_FAILED) {
        perror("mmap for Notification failed");
        return 1;
    }

    // Initialize all Notification structures to default values
    for (int i = 0; i < numAircrafts; ++i) {
        notifications[i].ascend = false;
        notifications[i].descend = false;
        notifications[i].cycleCount = 0;
    }

    // Open synchronization semaphore
    sem_sync = sem_open(sem_sync_name, 0);
    if (sem_sync == SEM_FAILED) {
        perror("sem_open for sync failed");
        return 1;
    }

    // Wait for the semaphore before checking shared memory
    while (true) {
        sem_wait(sem_sync); // Wait until ComputerSystem posts the semaphore

        // Check if we need to notify aircrafts
        if (commData->NotifyAircrafts) {
            std::cout << "Notifying Aircrafts "
                      << commData->aircraft1_id << " and "
                      << commData->aircraft2_id << std::endl;

            // Write notifications into shared memory
            notifications[commData->aircraft1_id].ascend = true;
            notifications[commData->aircraft1_id].descend = false;
            notifications[commData->aircraft1_id].cycleCount = 10; // Example value for cycles

            notifications[commData->aircraft2_id].ascend = false;
            notifications[commData->aircraft2_id].descend = true;
            notifications[commData->aircraft2_id].cycleCount = 10; // Example value for cycles

            // Reset NotifyAircrafts flag after processing
            commData->NotifyAircrafts = false;

            std::cout << "Notification written to shared memory.\n";
        } else {
            std::cout << "No notification required.\n";
        }
    }

    // Clean up
    sem_close(sem_sync);
    munmap(commData, sizeof(shared_data));
    munmap(notifications, NOTIFICATION_SIZE);
    close(shm_fd_comm);
    close(shm_fd_notification);

    return 0;
}
