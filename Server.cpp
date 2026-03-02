#include <iostream> //cout, endl
#include <string>
#include <vector>

#include <sys/socket.h> //socket, bind, listen, accept
#include <netinet/in.h> 

#include <unistd.h> //read and close

#include <thread> //to make multiple users possible
#include <mutex>

using namespace std;

//basic appointment examples we were given
struct Appointment
{
    string date, time, person;
};

//storage for all our data
vector<Appointment> database;
mutex db_mutex;

//display "ui" to the user and read their input
void sendText(int client_socket, string text)
{
    send(client_socket, text.c_str(), text.length(), 0);
}
string readLine(int client_socket)
{
    string input = "";
    char c;
    
    while (read(client_socket, &c, 1) > 0)
    {
        //"negotiation handshake fix"
        if ((unsigned char)c == 255)
        {
            char discard;
            read(client_socket, &discard, 1); 
            read(client_socket, &discard, 1); 
            continue; 
        }

        if (c == '\r' || c == '\n')
        {
            if (input.length() > 0)
            {
                return input;
            }
            continue;
        }
        
        if (c >= 32 && c <= 126)
        {
            input += c;
        }
    }
    
    return "DISCONNECT";
}

//one user instance.
void handleClient(int client_socket)
{
    sendText(client_socket, "Name: ");
    string username = readLine(client_socket);
    
    if (username == "DISCONNECT")
    { 
        close(client_socket); 
        return; 
    }

    //serverside
    cout << username << " joined the server." << endl;

    //while connected thread stays alive
    while (true)
    {
        //ui
        sendText(client_socket, "\n1.Add 2.Search 3.Delete 4.Display 5.Exit\nChoice: ");
        string choice = readLine(client_socket);
        
        //exit
        if (choice == "DISCONNECT" || choice == "5")
        {
            break;
        }

        //usual appointment path
        if (choice == "1")
        {
            Appointment a;
            sendText(client_socket, "Date: "); 
            a.date = readLine(client_socket);
            
            sendText(client_socket, "Time: "); 
            a.time = readLine(client_socket);
            
            sendText(client_socket, "Person: "); 
            a.person = readLine(client_socket);
            
            lock_guard<mutex> lock(db_mutex);
            database.push_back(a);
            sendText(client_socket, "Added.\n");
        }

        //search by name
        else if (choice == "2")
        {
            sendText(client_socket, "Search a Name: ");
            string name = readLine(client_socket);
            
            lock_guard<mutex> lock(db_mutex);
            for (size_t i = 0; i < database.size(); i++)
            {
                if (database[i].person.find(name) != string::npos)
                {
                    sendText(client_socket, to_string(i) + " | " + database[i].date + " | " + database[i].time + " | " + database[i].person + "\n");
                }
            }
        }

        //remove via Id
        else if (choice == "3")
        {
            sendText(client_socket, "ID to remove: ");
            string idStr = readLine(client_socket);
            
            try
            {
                //stoi = string to int 
                int id = stoi(idStr);
                lock_guard<mutex> lock(db_mutex);
                
                if (id >= 0 && id < database.size())
                {
                    database.erase(database.begin() + id);
                    sendText(client_socket, "Deleted.\n");
                }
            }
            catch (...)
            { 
                sendText(client_socket, "Invalid.\n"); 
            }
        }

        //display all in database
        else if (choice == "4")
        {
            lock_guard<mutex> lock(db_mutex);
            for (size_t i = 0; i < database.size(); i++)
            {
                sendText(client_socket, to_string(i) + " | " + database[i].date + " | " + database[i].time + " | " + database[i].person + "\n");
            }
        }
    }
    
    cout << username << " left the server." << endl;
    close(client_socket);
}

int main()
{
    //https://www.geeksforgeeks.org/cpp/socket-programming-in-cpp/
    // AF_INET means IPv4, SOCK_STREAM means TCP connection
    int main_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    // catch "Address already in use" error when restarting server quickly
    int enable_reuse = 1;
    setsockopt(main_server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable_reuse, sizeof(enable_reuse));
    
    //server setting things
    sockaddr_in server_settings;
    server_settings.sin_family = AF_INET;           
    server_settings.sin_addr.s_addr = INADDR_ANY;  
    server_settings.sin_port = htons(23);  
    
    //attaching setting to socket, like setting accesibility settings first
    bind(main_server_socket, (struct sockaddr *)&server_settings, sizeof(server_settings));
    
    //allow 3 connections for now
    listen(main_server_socket, 3);

    cout << "Server started on port 23..." << endl;

    //endless loop
    while (true)
    {
        //wait for users
        int new_user_socket = accept(main_server_socket, nullptr, nullptr);
        // if connected go to the user connected instance
        if (new_user_socket >= 0)
        {
            thread(handleClient, new_user_socket).detach();
        }
    }
    
    return 0;
}