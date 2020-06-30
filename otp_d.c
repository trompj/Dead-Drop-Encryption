// Author: Justin Tromp
// Date: 05/25/2020
// Description: Server otp_d handles requests from otp to write either receive an encrypted message and
// write it to a file for the user provided, or retrieve an encrypted message (oldest modified file for
// that user) and return it to otp.
// References: Previous Assignments
// https://www.geeksforgeeks.org/c-program-delete-file/
// https://www.zentut.com/c-tutorial/c-file-exists/
// https://www.thinkage.ca/gcos/expl/c/lib/fopen.html
// https://www.tutorialspoint.com/c_standard_library/c_function_remove.htm
// https://beej.us/guide/bgnet/html/#setsockoptman

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

// Variables used for connections and process tracking (maximum of 5 processes running at a time).
struct sockaddr_in server_addr, client_addr;
int processes[5];
int connectionActive = 1;
int server_fd;
socklen_t cliLength;

// Takes a string (char*) parameter for the path of a file to be removed. If
// successful, the file is removed. If not, an error is sent to stderr.
// Pre-conditions: Must be passed a valid file path to remove that file.
// Post-conditions: File is removed or error is sent to stderr.
void removeFile(char* filePath) {
    int error = remove(filePath);
    if (error != 0) {
        char* message = "Unable to delete the file.\n";
        write(2, message, 27);
    }
}

// Initializes global processes array with all five positions set to -5.
// Pre-conditions: Declared global processes array.
// Post-conditions: All positions in processes are set to -5.
void initializeProcesses() {
    int i;
    // Set all processes to -5
    for (i = 0; i < 5; i++) {
        processes[i] = -5;
    }
}

// Sends file text from provided path to socket provided. If unsuccessful, message is sent to stderr.
// If successful, message is sent to client (otp).
// Pre-conditions: Must receive open socket connection and char* to path of file of which to send.
// Post-conditions: File text is sent if successful. If not, then error message is sent to stderr.
void sendFile(int communicationSocket, char* path) {
    FILE* filePtr = NULL;
    // Attempt to open file of provided path
    filePtr = fopen(path, "r");

    // If file is opened, send file over data connection
    if (filePtr != NULL) {
        //Declare variable
        int bytesRead = 0;

        //Determine file size
        fseek(filePtr, 0, SEEK_END);
        int fileSize = ftell(filePtr);
        char fileSizeString[20];
        memset(fileSizeString, '\0', 20);

        // Convert integer to string to send over socket
        sprintf(fileSizeString, "%d", fileSize);

        // Send number of bytes that will be transmitted
        send(communicationSocket, fileSizeString, 20, 0);

        //Rewind to start of file
        rewind(filePtr);

        char fileBuffer[1025];
        //As long as file is being received, loop
        while (1) {
            memset(fileBuffer, '\0', 1025);
            bytesRead = fread(fileBuffer, 1, 1024, filePtr);

            //If the bytesRead is more than 0, then send the bytes to client
            if (bytesRead > 0) {
                send(communicationSocket, fileBuffer, bytesRead, 0);
            }
            //Otherwise, break out of loop
            else {
                break;
            }
        }

        //Close the file
        fclose(filePtr);

        // Remove file that was sent
        removeFile(path);
    }

    //File doesn't exist and/or cannot be opened
    else {
        //Send file not found error message
        char* message = "File not found or could not be opened.\n";
        write(2, message, 39);

        // If the file is not found, send failure message to client so that it does not wait for message.
        send(communicationSocket, "0", 1, 0);
    }
}

// Path to oldest file of user is determined and returned.
// Pre-conditions: Must be passed char* of users name to find their corresponding files.
// Post-conditions: If found, path to file is returned. If no valid file is found, an empty string
// is returned.
char* findOldestFilePath(char* user) {
    // Declare variables to be used in directory manipulation
    int oldestModified = 2147483647;

    // Allocate memory fore filename in directory
    char* dirName = malloc(sizeof(char) * 128);
    memset(dirName, '\0', 128);

    // Starting directory
    DIR* dirToExamine = opendir(".");

    // To hold sub directory of starting directory
    struct dirent* file;
    // To hold information about sub directory
    struct stat fileAttributes;

    // Check that directory could be opened
    if (dirToExamine > 0) {
        // Loop through directory contents
        while ((file = readdir(dirToExamine)) != NULL) {

            // Check encountered file for username
            if (strstr(file->d_name, user) != NULL) {

                // Get attributes
                stat(file->d_name, &fileAttributes);

                // Check if found file is older than last oldest file
                // If so, update time and name of directory
                if ((int) fileAttributes.st_mtime < oldestModified) {
                    oldestModified = (int) fileAttributes.st_mtime;

                    // Empty dirName contents and set filename of found file to dirName to be appended
                    // to path.
                    memset(dirName, '\0', 128);
                    strcpy(dirName, file->d_name);
                }
            }
        }
    }
    else {
        perror("Could not open directory\n");
    }

    // Close directory being searched
    closedir(dirToExamine);

    // Set dirPath path as current working directory to have oldest user filename added to.
//    char* dirPath = malloc(sizeof(char) * 256);
    char dirPath[256];
    memset(dirPath, '\0', 256);
    getcwd(dirPath, 256);

    char* pathName = malloc(sizeof(char) * 512);
    memset(pathName, '\0', 512);
    // Create path name for file creation
    strcat(pathName, dirPath);
    strcat(pathName, "/");

    // Check if file was found, if not return empty string.
    if (strcmp(dirName, "") == 0) {
        // Free memory allocated for filename and path string
        free(dirName);

        return "";
    }
    else {
        strcat(pathName, dirName);
    }

    // Free memory allocated for filename and path string
    free(dirName);

    return pathName;
}

// If otp sends request for get command, operations are performed in this function to
// find the oldest file for the provided users name and send it over the socket connection.
// Pre-conditions: Parameters for a valid open socket connection and a string representation of
// a users name must be provided.
// Post-conditions: Oldest file text is sent over socket connection.
void performGetOperations(int communicationSocket, char* user) {
    // Find the oldest filename and its path of the provided user and return to filePath
    char* filePath = findOldestFilePath(user);

    // If filePath is empty, then set boolean type to indicate file could not be found. This skips the processes below.
    int fileFound = 1;
    if (strcmp(filePath, "") == 0) {
        fileFound = 0;
    }

    // Send the file to the client over the socket in sections
    sendFile(communicationSocket, filePath);

    // If the file was found, free the file path
    if (fileFound == 1) {
        // Free the file path
        free(filePath);
    }
}

// If otp sends request for post command, operations are performed in this function to
// receive the encrypted file over the socket connection and write that message to a file
// for that user. The file will be of the format: <pid>_<user>
// Pre-conditions: A valid/open socket connection and a string of the users name are passed as parameters.
// Post-conditions: If successful, an encrypted files text is received over the socket connection and saved to
// a file for that user with the users name listed. If unsuccessful, a corresponding error is printed to stderr.
void performPostOperations(int communicationSocket, char* user) {
    // Set read buffer to receive messages from otp_d server
    char* readBuffer = malloc(sizeof(char) * 2056);
    memset(readBuffer, '\0', 2056);

    // GET MESSAGE AND ADD TO FILE
    // Variables for path. Set path as current working directory.
    char dirPath[256];
    getcwd(dirPath, 256);

    // Get pid and convert to string
    char pid[10];
    sprintf(pid, "%d", getpid());

    char* pathName = malloc(sizeof(char) * 512);
    memset(pathName, '\0', 512);
    // Create path name for file creation
    strcat(pathName, dirPath);
    strcat(pathName, "/");
    strcat(pathName, pid);
    strcat(pathName, "_");
    strcat(pathName, user);

    FILE *fPointer = fopen(pathName, "w");

    // Check to see if file was opened
    if (fPointer == NULL) {
        char* message = "Error opening a file.\n";
        write(2, message, 21);
    }
    // Output message to file
    else {
        int valread = 0;
        // Loop until all of message is received from otp. Message will be received in chunks of 1024.
        while (valread != -1) {

            // Clear buffer
            memset(readBuffer, '\0', 2056);

            // Get encrypted message from the client
            valread = recv(communicationSocket, readBuffer, 1024, 0);

            // If message indicating send is completed is sent, exit read loop.
            if (valread == 0) {
                break;
            }

            // Allocate memory for string to accept the expected message size
            char* fileInput = malloc(sizeof(char) * 1028);
            memset(fileInput, '\0', 1028);

            // Add message to string from buffer and print to file
            strcpy(fileInput, readBuffer);
            fprintf(fPointer, "%s", fileInput);

            // Free allocated memory for fileInput
            free(fileInput);
        }
    }

    // Add final newline character at end of message
    fprintf(fPointer, "\n");
    fflush(stdout);

    // Close file
    fclose(fPointer);

    // Output message with path of new file
    write(1, pathName, strlen(pathName));
    write(1, "\n", 1);

    fflush(stdout);

    // Free allocated memory for file path and read buffer
    free(pathName);
    free(readBuffer);
}

// Check on processes running in background and if the process has finished. If process has finished, set position
// in processes array to free (-5). If all process positions are full and none are finished, continue looping until
// one completes.
// Pre-conditions: Declared processes array holding possible process ID's of background processes.
// Post-conditions: Checks background processes (if any) and if finished, sets process position in array to free (-5).
// If all positions are full, function will continue looping/checking until a process completes.
// Reference: http://web.engr.oregonstate.edu/~brewsteb/CS344Slides/3.1%20Processes.pdf
void checkProcesses() {
    int exitStatus = -10;

    int processesFull = 1;
    while (processesFull == 1) {
        int i;
        // Loop through processes, checking status
        for (i = 0; i < 5; i++) {
            // Check if process exists and if it is still running
            if (processes[i] != -5) {
                // Check if process completed and collect result in childPID, if not 0, then process has finished.
                pid_t childPID = waitpid(processes[i], &exitStatus, WNOHANG);

                // If the pid_t returned is not 0, then it has exited
                if (childPID != 0) {
                    // Set process index to -5 when finished to indicate free
                    processes[i] = -5;

                    // Set processes as not full as one ended
                    processesFull = 0;
                }
            }
            // If there is a free spot, then set processesFull bool value
            else {
                processesFull = 0;
            }
        }
    }
}

// Server driver function that sets up socket connection to listen on provided port and loops until ctrl-c is
// encountered. Each get or post operation is performed by a forked process once a connection is established by
// the main process. No more than 5 processes can be forked off at any given time. If another is attempted, the
// main process will wait until one finishes.
// Pre-conditions: A port number is passed as parameter to runServer to create a listening connection on that port.
// Post-conditions: If successful, any get or post request will be processed when received by otp and the server
// will run indefinitely until terminated with ctrl-c. If unsuccessful, runServer returns -1 and outputs a correlating
// error message to stderr.
int runServer(int port) {
    int error = 0;
    //Initiate control connection and get integer from error if exists
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    //If socket isn't opened properly, print error and return.
    if (server_fd < 0) {
        fprintf(stderr, "Error opening socket.\n");

        return -1;
    }

    // Server socket setup
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    //If socket cannot be bound properly, return -1 and print error.
    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error on binding.\n");
        return -1;
    }

    //Start listening
    listen (server_fd, 5);

    //Set client length based on client_addr
    cliLength = sizeof(client_addr);

    int communicationSocket = -5;
    //Loops until connection is ended (program currently setup to end with ctrl c and not user input.
    while (connectionActive) {
        // Wait for connection to be accepted and set connection to
        communicationSocket = accept(server_fd, (struct sockaddr *) &client_addr, &cliLength);

        // Variable to hold PID of fork process
        pid_t spawnPID = -5;

        // Check if there is a free process before fork and clear any if completed.
        // If not, continues looping until a process ends.
        checkProcesses();

        // Create fork and receive PID return value for processing
        spawnPID = fork();

        // Perform actions based on PID value returned
        switch (spawnPID) {

            // Error forking, output respective error and exit
            case -1: {
                perror("PID Fork Error");
                exit(1);
                break;
            }

            // Child process runs and performs operations
            case 0: {
                // Mandatory sleep for each child process
                sleep(2);

                //Buffer receives get or post command
                char* command = malloc(sizeof(char) * 8);
                memset(command, '\0', 8);

                // Get command (post or get)
                int valread = read(communicationSocket, command, 5);

                // Respond with acceptance message if received
                if (valread > 0) {
                    send(communicationSocket, "COMMAND_RECEIVED", 16, 0);
                }

                // Accept username from otp
                // Buffer receives get or post command
                char* readBuffer = malloc(sizeof(char) * 1029);
                memset(readBuffer, '\0', 1029);

                // Get user from the client
                valread = read(communicationSocket, readBuffer, 1024);

                // Set user to user string after setting memory and clearing it
                char* user = malloc(sizeof(char) * (strlen(readBuffer) + 5));
                memset(user, '\0', strlen(readBuffer));
                strcpy(user, readBuffer);

                // Respond with acceptance message if received
                if (valread > 0) {
                    send(communicationSocket, "USER_RECEIVED", 13, 0);
                }

                // Check if post or get and perform operations accordingly
                if (strcmp("get", command) == 0) {
                    performGetOperations(communicationSocket, user);
                }
                else if (strcmp("post", command) == 0) {
                    performPostOperations(communicationSocket, user);
                }

                // Free memory for user
                free(user);
                free(command);
                free(readBuffer);

                // Exit child process
                exit(0);
            }

            // Parent process enters
            default: {
                int i;
                // Loop through processes array and find a location with -5 to add PID of
                // background process. When found, add PID to that location. There should
                // always be a free process location at this point. Before a fork occurs,
                // the program will check for max capacity of 5 processes and will wait
                // until one finishes before adding more.
                for (i = 0; i < 5; i++) {
                    if (processes[i] == -5) {
                        processes[i] = spawnPID;
                        break;
                    }
                    fflush(stdout);
                }
            }
        }

        fflush(stdout);
    }

    //Close control connection. Never really reached as the only way to close currently is ctrl c however.
    close(communicationSocket);

    return 0;
}

// End all processes started when called on exit.
// Pre-conditions: Array of process ID's used in program is declared.
// Post-conditions: All processes found are terminated.
void endProcesses() {
    int i;
    // Loop through processes stored and end any that are currently working
    for (i = 0; i < 5; i++) {
        if (processes[i] != -5) {
            kill(processes[i], SIGTERM);
        }
    }
}

// Main takes in a command argument for the port number to listen for connections on while the server is running.
// The server driver function is called if the port consists of what appears to be a valid value and runs the server
// processes until exited. When finished, endProcesses is called to ensure all processes have ended prior to exiting.
int main(int argc, char* argv[]) {
    // Initialize processes array indices to -5
    initializeProcesses();

    //Check to see that correct number of arguments are included, if not throw error
    if (argc < 2) {
        fprintf(stderr, "A port number must be provided to listen on as a command line argument.");
        exit(1);
    }
    else if (argc > 2) {
        fprintf(stderr, "Only one command line argument should be provided to specify port to listen on.");
        exit(1);
    }

    int i;
    // Check if all values in argument are integers and output error/exit if not
    for (i = 0; i < strlen(argv[1]); i++) {
        if (!isdigit(argv[1][i])) {
            fprintf(stderr, "Port number argument can only be an integer value.\n");
            exit(1);
        }
    }

    //Set port
    int port = atoi(argv[1]);

    //If port number is invalid range, throw error
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Port number must be in a valid range of values (1 through 65535).\n");
        exit(1);
    }

    //Run server
    int error = runServer(port);

    // Kills any remaining processes
    endProcesses();

    // If an error occurs with opening socket/binding, then exit.
    if (error == -1) {
        exit(1);
    }

    return 0;
}