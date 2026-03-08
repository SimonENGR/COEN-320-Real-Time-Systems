#include <iostream>
#include <fstream> // For file operations
#include <unistd.h>
#include <ctime>
#include <sys/dispatch.h>
#include <chrono> // For timer functionality

// Structure to represent collision message
typedef struct {
    int aircraft1_id;
    int aircraft2_id;
    char decision;  // 'Y' for yes, 'N' for no
} collision_msg;

// Function to print the current timestamp
void printTimestamp(std::ostream &outStream = std::cout) {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    outStream << "["
              << (ltm->tm_mday < 10 ? "0" : "") << ltm->tm_mday << "/"
              << (ltm->tm_mon + 1 < 10 ? "0" : "") << (ltm->tm_mon + 1) << "/"
              << (1900 + ltm->tm_year) << " - "
              << (ltm->tm_hour < 10 ? "0" : "") << ltm->tm_hour << ":"
              << (ltm->tm_min < 10 ? "0" : "") << ltm->tm_min << ":"
              << (ltm->tm_sec < 10 ? "0" : "") << ltm->tm_sec
              << "] ";
}

int main() {
    // Create a connection name for the server
    name_attach_t *attach = name_attach(NULL, "OperatorConsole", 0);
    if (attach == NULL) {
        perror("name_attach");
        return 1;
    }

    std::cout << "Operator Console is running, waiting for collision alerts...\n";

    // Open a log file to write operator responses
    std::ofstream logFile("operatorlogs.txt", std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: operatorlogs.txt\n";
        return 1;
    }

    // Get the start time
    auto startTime = std::chrono::high_resolution_clock::now();

    while (true) {
        // Check if 90 seconds have passed
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = currentTime - startTime;

        if (elapsed.count() >= 75.0) {
            std::cout << "90-second timer expired. Exiting loop...\n";
            break; // Exit the loop
        }

        int rcvid;
        collision_msg msg;

        // Receive the collision message from the client
        rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid == -1) {
            perror("MsgReceive");
            continue;
        }

        // Inform user about the collision possibility
        std::cout << "Collision possible between Aircraft " << msg.aircraft1_id
                  << " and Aircraft " << msg.aircraft2_id << ".\n";

        // Commented-out user input logic (manual intervention section)
        /*
        std::cout << "Would you like to contact the Aircrafts to modify their Z position? (Y/N): ";

        // Start a timer for 2 seconds using chrono
        auto start = std::chrono::high_resolution_clock::now();
        std::string input;
        bool timedOut = false;

        // Use a separate thread to wait for user input
        std::thread inputThread([&]() {
            std::cin >> input;
        });

        // Loop while checking if the 2-second timer has expired
        while (true) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - start;

            if (elapsed.count() >= 2.0) {
                // If 2 seconds have passed, mark as timed out
                timedOut = true;
                break;
            }

            // If user input is received, break the loop
            if (!input.empty()) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // small delay to avoid busy-waiting
        }

        // Join the input thread to ensure proper cleanup
        inputThread.join();

        // If no input within 2 seconds, automatically choose 'Y'
        if (timedOut) {
            std::cout << "No response received, automatically choosing 'Y'.\n";
            msg.decision = 'Y';
        } else {
            // Otherwise, use the user's decision
            msg.decision = input.empty() ? 'Y' : input[0];
        }
        */
        
        //Comment this part below to turn off auto response
        // Automatically choose 'Y' for the decision
        msg.decision = 'Y';
        std::cout << "Automatically choosing 'Y'.\n";
        //Comment this part abovw to turn off auto response
        
        // Log the operator's response to the file with a timestamp
        printTimestamp(logFile);
        logFile << "Operator responded 'Y' to intervene for collision warning between Aircraft "
                << msg.aircraft1_id << " and Aircraft " << msg.aircraft2_id << ".\n";

        // Send the decision back to the client
        std::cout << "Sending message back to client...\n";
        MsgReply(rcvid, 0, &msg, sizeof(msg));
    }

    // Close the log file when the application stops
    logFile.close();
    std::cout << "Operatorlogs was successfully written...\n";
    name_detach(attach, 0);
    return 0;
}
