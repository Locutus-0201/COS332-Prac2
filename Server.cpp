#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <unistd.h>     
#include <cstring>
#include <thread> // The standard C++ library for handling multiple users
#include <mutex>  // The standard C++ library for database protection

using namespace std;

struct Appointment {
    string date;
    string time;
    string person;
};

// Global variables so all threads can access them
vector<Appointment> database; 
mutex db_mutex; // The "talking stick" to prevent database corruption

void sendText(int client_socket, string text) {
    send(client_socket, text.c_str(), text.length(), 0);
}

void clearScreen(int client_socket) {
    sendText(client_socket, "\033[2J\033[1;1H");
}

string readLineAndEcho(int client_socket) {
    string input = "";
    char c;
    while (true) {
        int bytes_read = read(client_socket, &c, 1);
        
        // Safety feature: If read is 0 or less, the client forcefully disconnected
        if (bytes_read <= 0) {
            return "DISCONNECT";
        }

        if ((unsigned char)c == 255) {
            char discard;
            read(client_socket, &discard, 1); 
            read(client_socket, &discard, 1); 
            continue; 
        }
        if (c == '\n') continue; 
        if (c == '\r') {
            sendText(client_socket, "\r\n"); 
            break; 
        }
        if (c == 8 || c == 127) {
            if (input.length() > 0) {
                input.pop_back(); 
                sendText(client_socket, "\b \b"); 
            }
            continue;
        }
        if (c >= 32 && c <= 126) {
            input += c;
            send(client_socket, &c, 1, 0); 
        }
    }
    return input;
}

// ---------------------------------------------------------
// THE WORKER THREAD: Runs independently for EVERY user
// ---------------------------------------------------------
void handleClient(int client_socket) {
    
    sendText(client_socket, "Welcome to the Server! Please enter your name: ");
    string username = readLineAndEcho(client_socket);
    
    if (username == "DISCONNECT") {
        close(client_socket);
        return;
    }

    cout << "User '" << username << "' has joined the server." << endl;

    while (true) {
        clearScreen(client_socket); 

        sendText(client_socket, "============================\r\n");
        sendText(client_socket, "      APPOINTMENT MENU      \r\n");
        sendText(client_socket, "============================\r\n");
        sendText(client_socket, "Logged in as: " + username + "\r\n\r\n");
        sendText(client_socket, "1. Add Appointment\r\n");
        sendText(client_socket, "2. Search Appointments\r\n");
        sendText(client_socket, "3. Delete Appointment\r\n");
        sendText(client_socket, "4. Display All Appointments\r\n");
        sendText(client_socket, "5. Exit\r\n");
        sendText(client_socket, "Choose an option: ");

        string choice = readLineAndEcho(client_socket);

        // Break the loop if the client drops connection
        if (choice == "DISCONNECT") {
            cout << "User '" << username << "' unexpectedly dropped connection." << endl;
            break; 
        }

        if (choice.length() > 0) {
            char option = choice[0]; 

            if (option == '1') {
                Appointment newAppt;
                sendText(client_socket, "Enter Date (e.g. 2026-03-01): ");
                newAppt.date = readLineAndEcho(client_socket);
                
                sendText(client_socket, "Enter Time (e.g. 14:00): ");
                newAppt.time = readLineAndEcho(client_socket);
                
                sendText(client_socket, "Enter Person: ");
                newAppt.person = readLineAndEcho(client_socket);
                
                if (newAppt.date == "DISCONNECT" || newAppt.time == "DISCONNECT" || newAppt.person == "DISCONNECT") break;

                // LOCK THE DATABASE before writing to it!
                db_mutex.lock();
                database.push_back(newAppt); 
                db_mutex.unlock();
                
                sendText(client_socket, "\r\n>> Appointment added successfully! Press Enter to continue... <<\r\n");
                readLineAndEcho(client_socket); 

            } else if (option == '2') {
                sendText(client_socket, "Enter Person to search for: "); 
                string searchName = readLineAndEcho(client_socket);
                if (searchName == "DISCONNECT") break;

                sendText(client_socket, "\r\n--- Search Results ---\r\n");
                bool found = false;
                
                db_mutex.lock(); // Lock before reading
                for (size_t i = 0; i < database.size(); i++) {
                    if (database[i].person.find(searchName) != string::npos) {
                        sendText(client_socket, "[ID: " + to_string(i) + "] " + database[i].date + " at " + database[i].time + " with " + database[i].person + "\r\n");
                        found = true;
                    }
                }
                db_mutex.unlock();
                
                if (!found) sendText(client_socket, "No appointments found.\r\n");
                sendText(client_socket, "\r\n>> Press Enter to continue... <<\r\n");
                readLineAndEcho(client_socket);

            } else if (option == '3') {
                sendText(client_socket, "Enter the ID number to delete: "); 
                string idStr = readLineAndEcho(client_socket);
                if (idStr == "DISCONNECT") break;

                try {
                    int id = stoi(idStr); 
                    
                    db_mutex.lock(); // Lock before deleting
                    if (id >= 0 && id < database.size()) {
                        database.erase(database.begin() + id);
                        sendText(client_socket, "\r\n>> Appointment deleted. <<\r\n");
                    } else {
                        sendText(client_socket, "\r\n>> Invalid ID. <<\r\n");
                    }
                    db_mutex.unlock();
                    
                } catch (...) {
                    sendText(client_socket, "\r\n>> Invalid input. ID must be a number. <<\r\n");
                }
                sendText(client_socket, ">> Press Enter to continue... <<\r\n");
                readLineAndEcho(client_socket);

            } else if (option == '4') {
                sendText(client_socket, "\r\n--- All Appointments ---\r\n"); 
                
                db_mutex.lock(); // Lock before reading
                if (database.empty()) {
                    sendText(client_socket, "The database is empty.\r\n");
                } else {
                    for (size_t i = 0; i < database.size(); i++) {
                        sendText(client_socket, "[ID: " + to_string(i) + "] " + database[i].date + " at " + database[i].time + " with " + database[i].person + "\r\n");
                    }
                }
                db_mutex.unlock();
                
                sendText(client_socket, "\r\n>> Press Enter to continue... <<\r\n");
                readLineAndEcho(client_socket);

            } else if (option == '5') {
                clearScreen(client_socket);
                sendText(client_socket, "Goodbye, " + username + "!\r\n");
                cout << "User '" << username << "' has disconnected." << endl;
                break; 
                
            } else {
                sendText(client_socket, "Invalid choice.\r\n>> Press Enter to continue... <<\r\n");
                readLineAndEcho(client_socket);
            }
        }
    }
    // Clean up their specific socket when they leave
    close(client_socket); 
}

// ---------------------------------------------------------
// THE MANAGER: Only listens for connections and spawns workers
// ---------------------------------------------------------
int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int port = 23; 
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) return 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        cerr << "Bind failed! Remember to use sudo." << endl;
        return 1;
    }
    if (listen(server_fd, 3) < 0) return 1;

    cout << "Appointment Server listening on port " << port << "..." << endl;

    // The Manager Loop: Wait for connections forever
    while (true) {
        int addrlen = sizeof(address);
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
            
            // Spawn a new background thread for this client, and detach it so it runs independently
            thread client_thread(handleClient, client_socket);
            client_thread.detach(); 
        }
    }

    close(server_fd);
    return 0;
}