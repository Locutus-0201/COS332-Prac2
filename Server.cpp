#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <unistd.h>     
#include <cstring>

using namespace std;

// 1. Define what an Appointment looks like
struct Appointment {
    string date;
    string time;
    string person;
};

// Helper function to easily send text to the client
void sendText(int client_socket, string text) {
    send(client_socket, text.c_str(), text.length(), 0);
}

// The core requirement: Read text AND manually echo it back to the client
string readLineAndEcho(int client_socket) {
    string input = "";
    char c;
    while (read(client_socket, &c, 1) > 0) {
        // If user presses Enter (Carriage Return)
        if (c == '\r' || c == '\n') {
            sendText(client_socket, "\r\n"); // Echo a clean new line visually
            if (input.length() > 0) {
                break; // We have a complete line!
            }
            continue; // Ignore empty newlines
        }
        
        // Handle Backspace so they can fix typos! (ASCII 8 or 127)
        if (c == 8 || c == 127) {
            if (input.length() > 0) {
                input.pop_back(); // Remove from our string
                sendText(client_socket, "\b \b"); // Visually erase on their screen
            }
            continue;
        }

        // Add the character to our string and echo it back!
        input += c;
        send(client_socket, &c, 1, 0); 
    }
    return input;
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int port = 23; 
    
    // 2. Our "Database" (A dynamic array)
    vector<Appointment> database;

    // --- Networking Setup ---
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) return 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) return 1;
    if (listen(server_fd, 3) < 0) return 1;

    cout << "Appointment Server listening on port " << port << "..." << endl;

    int addrlen = sizeof(address);
    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) return 1;
    cout << "A client has connected!" << endl;

    // --- The Main Application Loop ---
    while (true) {
        // Print the Menu
        sendText(client_socket, "\r\n============================\r\n");
        sendText(client_socket, "      APPOINTMENT MENU      \r\n");
        sendText(client_socket, "============================\r\n");
        sendText(client_socket, "1. Add Appointment\r\n");
        sendText(client_socket, "2. Search Appointments\r\n");
        sendText(client_socket, "3. Delete Appointment\r\n");
        sendText(client_socket, "4. Exit\r\n");
        sendText(client_socket, "Choose an option: ");

        // Read the user's choice using our special echo function
        string choice = readLineAndEcho(client_socket);

        if (choice == "1") {
            Appointment newAppt;
            sendText(client_socket, "Enter Date (e.g. 2026-03-01): ");
            newAppt.date = readLineAndEcho(client_socket);
            
            sendText(client_socket, "Enter Time (e.g. 14:00): ");
            newAppt.time = readLineAndEcho(client_socket);
            
            sendText(client_socket, "Enter Person: ");
            newAppt.person = readLineAndEcho(client_socket);
            
            database.push_back(newAppt); // Save to array
            sendText(client_socket, "\r\n>> Appointment added successfully! <<\r\n");

        } else if (choice == "2") {
            sendText(client_socket, "Enter Person to search for: ");
            string searchName = readLineAndEcho(client_socket);
            
            sendText(client_socket, "\r\n--- Search Results ---\r\n");
            bool found = false;
            for (size_t i = 0; i < database.size(); i++) {
                // If the search name matches part of the saved person's name
                if (database[i].person.find(searchName) != string::npos) {
                    string result = "[ID: " + to_string(i) + "] " + database[i].date + " at " + database[i].time + " with " + database[i].person + "\r\n";
                    sendText(client_socket, result);
                    found = true;
                }
            }
            if (!found) sendText(client_socket, "No appointments found.\r\n");

        } else if (choice == "3") {
            sendText(client_socket, "Enter the ID number to delete (search first to find ID): ");
            string idStr = readLineAndEcho(client_socket);
            
            try {
                int id = stoi(idStr); // Convert string to integer
                if (id >= 0 && id < database.size()) {
                    database.erase(database.begin() + id); // Remove from array
                    sendText(client_socket, "\r\n>> Appointment deleted. <<\r\n");
                } else {
                    sendText(client_socket, "\r\n>> Invalid ID. <<\r\n");
                }
            } catch (...) {
                sendText(client_socket, "\r\n>> Invalid input. Must be a number. <<\r\n");
            }

        } else if (choice == "4") {
            sendText(client_socket, "Goodbye!\r\n");
            break; 
        } else {
            sendText(client_socket, "Invalid choice. Please enter 1, 2, 3, or 4.\r\n");
        }
    }

    close(client_socket);
    close(server_fd);
    cout << "Server shutdown." << endl;
    return 0;
}