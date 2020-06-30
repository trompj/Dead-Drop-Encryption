// Author: Justin Tromp
// Date: 05/25/2020
// Description: otp acts as the client to otp_d and can perform a get or post request. With a get request,
// otp requests an encrypted file for a user and decrypts it with a key. With a post request, otp encrypts
// a file with a key and sends the encrypted text to be written to a file by otp_d.
// Valid post arguments: post <username> <file_to_encrypt> <key> <port>
// Valid get arguments: get <username> <key> <port>
// References: Previous Assignments
// https://www.zentut.com/c-tutorial/c-file-exists/
// https://www.thinkage.ca/gcos/expl/c/lib/fopen.html
// https://beej.us/guide/bgnet/html/#setsockoptman


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>

// Sends message over a valid/open socket connection.
// Pre-conditions: Must have a valid/open socket and a string message to be sent passed as parameters.
// Post-conditions: Message in parameter is sent over socket connection to otp_d.
void sendMessage(int socket, char* message) {
    int charsSent = 0;
    int charsLeft = strlen(message);
    int charsSentTotal = 0;

    // Loop until the entire message is sent
    while(charsLeft > 0) {
        // Send all or part of message if necessary over socket connection
        charsSent = send(socket, message, charsLeft, 0);

        // Determine number of characters left to be sent and how many characters have been sent total
        charsLeft = charsLeft - charsSent;
        charsSentTotal = charsSentTotal + charsSent;

        // If number of characters sent is -1, then break out of loop. Nothing was sent.
        if (charsSent == -1) {
            break;
        }
    }
}

// Receives encrypted message from otp_d over valid/open socket connection.
// Pre-conditions: Must be passed a valid/open socket connection as parameter.
// Post-conditions: Receives the encrypted message from otp_d over socket connection and returns it at end
// of function.
char* receiveEncryptedMessage(int socket) {
    char fileSize[20];
    memset(fileSize, '\0', 20);

    // Receives the size of the file that is expected to be sent by otp_d.
    recv(socket, fileSize, 20, 0);

    // Convert string size of file to integer value.
    int fileSizeInt = 0;
    fileSizeInt = atoi(fileSize);

    // Set aside memory for the encrypted message to be received based on the size of file expected.
    char* encryptedMessage = malloc(sizeof(char) * (fileSizeInt + 5));
    memset(encryptedMessage, '\0', (fileSizeInt + 5));

    int valread = 0;
    int bytesRead = 0;
    // Set read buffer to receive messages from otp_d server
    char* readBuffer = malloc(sizeof(char) * 1028);
    memset(readBuffer, '\0', 1028);

    // Loop until all of message is received from otp_d. Message will be received in chunks of 1024.
    while (bytesRead < fileSizeInt) {
        // Clear buffer
        memset(readBuffer, '\0', 1028);

        // Get encrypted message from the server
        valread = recv(socket, readBuffer, 1024, 0);

        strcat(encryptedMessage, readBuffer);
        bytesRead = bytesRead + valread;
    }

    // Free buffer
    free(readBuffer);

    return encryptedMessage;
}

// Check the characters of the message provided to ensure that only capital letters or spaces are encountered.
// Pre-conditions: Must be passed a string message as parameter.
// Post-conditions: If message has an invalid character, output error message to stderr and exit otp.
void checkCharacters(char* message) {
    int i;
    // Loop through message
    for (i = 0; i < strlen(message) - 1; i++) {
        // If end of message is encountered, exit loop.
        if (message[i] == '\n') {
            break;
        }
        // If the message is not a capital letter or space, send error message and exit 1
        else if (!(((int)message[i] >= 65 && (int)message[i] <= 90) || (int)message[i] == 32)) {
            fprintf(stderr, "Input provided has an invalid character.\n");
            exit(1);
        }
    }
}

// Encrypts message in a file based on provided key.
// Pre-conditions: Must be passed a valid name of a file and the name of a key as parameters.
// Post-conditions: The message in the provided file is retrieved and encrypted by the provided key. The
// encrypted message is then returned by the function.
char* encryptMessage(char* key, char* fileName) {
    char dirPath[256];
    // Get the current working directory
    getcwd(dirPath, 256);

    // Set file pointers
    FILE* keyFilePointer;
    FILE* textFilePointer;

    // Create path to file for key and use the path created to open that file to be read from.
    char pathName[256];
    memset(pathName, '\0', 256);
    strcat(pathName, dirPath);
    strcat(pathName, "/");
    strcat(pathName, key);
    keyFilePointer = fopen(pathName, "r");

    // Create a path to file for fileName and use the path created to open that file to be read from.
    memset(pathName, '\0', 256);
    strcat(pathName, dirPath);
    strcat(pathName, "/");
    strcat(pathName, fileName);
    textFilePointer = fopen(fileName, "r");

    char keyLineRead[100000];
    memset(keyLineRead, '\0', 100000);
    // Read line from key file to encrypt with
    fgets(keyLineRead, 100000, keyFilePointer);

    char messageLineRead[100000];
    memset(messageLineRead, '\0', 100000);
    // Read line from message/text file to be encrypted
    fgets(messageLineRead, 100000, textFilePointer);

    // Close files after read
    fclose(keyFilePointer);
    fclose(textFilePointer);

    // Get message and key lengths and compare sizes
    int keyLen = strlen(keyLineRead);
    int messageLen = strlen(messageLineRead);

    // Throw error if key file is not equal to or larger than the message to be encrypted
    if (keyLen < messageLen) {
        fprintf(stderr, "Key must be the same size or larger than the file being encrypted.\n");
        exit(1);
    }

    // Check that key file and message do not have any out of place characters that are not allowed.
    // Outputs error to stderr if found and exits with 1.
    checkCharacters(keyLineRead);
    checkCharacters(messageLineRead);

    // Allocate memory for encrypted message. This variable will be returned by the function
    // if successful.
    int sizeMsg = strlen(messageLineRead);
    char* encryptedMessage = malloc(sizeof(char) * sizeMsg);
    memset(encryptedMessage, '\0', sizeMsg);

    int i;
    // Encrypt message and store in new variable
    for (i = 0; i < messageLen - 1; i++) {
        int asciiMsgChar = 0;
        int asciiKeyChar = 0;
        // Get ascii values of characters for key and message to encrypt
        // If the current position is a space, set to value of 91
        if (keyLineRead[i] == ' ') {
            asciiKeyChar = 91 - 65;
        }
        // Otherwise, set value of the capital letter
        else {
            asciiKeyChar = (int) keyLineRead[i] - 65;
        }

        // If the current position is a space, set to value of 91
        if (messageLineRead[i] == ' ') {
            asciiMsgChar = 91 - 65;
        }
        // Otherwise, set the value of the capital letter
        else {
            asciiMsgChar = (int) messageLineRead[i] - 65;
        }

        // Determine new character value based on encryption technique (otp)
        int newChar = (asciiKeyChar + asciiMsgChar) % 27;

        // Determine new ASCII character
        newChar = 65 + newChar;

        // If the new ASCII value is 91, set to space ASCII of 32 as 91 will represent a space
        if (newChar == 91) {
            newChar = 32;
        }
        // If the character found is larger than 91, then wrap around to the beginning of the capital letter ASCII
        // values.
        else if (newChar > 91) {
            newChar = newChar - 27;
        }

        // Set character
        encryptedMessage[i] = (char) newChar;
    }

    return encryptedMessage;
}

// Decrypt the provided message passed as a parameter and return it. Message is decrypted based on provided key.
// Pre-conditions: The parameter of the name of a key file and a string of the encrypted message.
// Post-conditions: If successful, the encrypted message is decrypted and returned by function. Errors
// are returned to stderr where applicable.
char* decryptMessage(char* key, char* encryptedMessage) {
    char dirPath[256];
    memset(dirPath, '\0', 256);
    // Get the current working directory
    getcwd(dirPath, 256);

    // Declare pointer for key file to be opened
    FILE* keyFilePointer;

    // Determine path to key file and open key file to be read from
    char pathName[256];
    memset(pathName, '\0', 256);
    strcat(pathName, dirPath);
    strcat(pathName, "/");
    strcat(pathName, key);
    keyFilePointer = fopen(pathName, "r");

    char keyLineRead[100000];
    memset(keyLineRead, '\0', 100000);
    // Read line from key file to encrypt with
    fgets(keyLineRead, 100000, keyFilePointer);

    // Close file after read
    fclose(keyFilePointer);

    // Get message and key lengths and compare sizes
    int keyLen = strlen(keyLineRead);
    int messageLen = strlen(encryptedMessage);

    // Throw error if key file is not equal to or larger than the message to be decrypted
    if (keyLen < messageLen) {
        fprintf(stderr, "Key must be the same size or larger than the file being decrypted.\n");
        exit(1);
    }

    // Check that key file and message do not have any out of place characters that are not allowed.
    // Outputs error to stderr if found and exits with 1. This should not occur, but is an extra check.
    checkCharacters(keyLineRead);
    checkCharacters(encryptedMessage);

    // Set aside location in memory for decrypted message. This will be returned after decryption.
    char* decryptedMessage = malloc(sizeof(char) * (messageLen + 5));
    memset(decryptedMessage, '\0', (messageLen + 5));

    int i;
    // Decrypt message and store in new variable
    for (i = 0; i < messageLen - 1; i++) {
        int asciiMsgChar = 0;
        int asciiKeyChar = 0;
        // Get ascii values of characters for key and message to encrypt
        // If the current position is a space, set to value of 91
        if (keyLineRead[i] == ' ') {
            asciiKeyChar = 91 - 65;
        }
        // Otherwise perform calculation to determine what character based on an encountered capital letter.
        else {
            asciiKeyChar = (int) keyLineRead[i] - 65;
        }

        // If the current position is a space, set to value of 91
        if (encryptedMessage[i] == ' ') {
            asciiMsgChar = 91 - 65;
        }
        // Otherwise perform calculation to determine what character based on an encountered capital letter.
        else {
            asciiMsgChar = (int) encryptedMessage[i] - 65;
        }

        // Determine new character value based on encryption technique (otp)
        int newChar = (asciiMsgChar - asciiKeyChar) % 27;

        // Determine new ASCII character
        newChar = 65 + newChar;

        // If the new ASCII value is 64, set to space ASCII of 32 as 64 represents space
        if (newChar == 64 || newChar == 91) {
            newChar = 32;
        }
        // Otherwise treat as a capital letter and add 27 to determine the decrypted character.
        else if (newChar < 65) {
            newChar = newChar + 27;
        }

        // Set decrypted character to decryptedMessage
        decryptedMessage[i] = (char) newChar;
    }

    return decryptedMessage;
}

// Initiate a socket connection based on port value passed as parameter.
// Pre-conditions: Must be passed a valid port integer value as parameter.
// Post-conditions: Connection is established with otp_d if successful and socket connection value is returned.
// If unsuccessful a corresponding message is output to stderr.
int initiateConnection(int port) {
    struct sockaddr_in serv_addr;
    int clientSocket;

    // Attempt to create the socket, if failed output error to stderr and exit.
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket creation error\n");
        exit(1);
    }

    // Set sin_family and sin_port (based on provided port value) of the socket
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Attempt to convert address string to AF_INET network address
    // structure. If unsuccessful, outputs error to stderr and exits.
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address\n");
        exit(1);
    }

    // Attempt connection with given attributes. If unsuccessful return error to stderr and exit.
    // If successful, clientSocket is ready for use and is returned.
    if (connect(clientSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Connection Failed\n");
        exit(1);
    }

    return clientSocket;
}

// Takes 5 arguments for a post request and 4 arguments for a get request. The main function primarily acts in
// validating arguments received by command line and then acts as a driver function to call the relevant functions
// required for a post process and a get process, depending on which is requested.
int main(int argc, char* argv[]) {
    //Check to see that correct number of arguments are included, if not throw error
    if (argc < 5 || argc > 6) {
        fprintf(stderr, "Invalid number of arguments provided. Must provide 4 with get or 5 with post.\n");
        exit(1);
    }

    char* key = NULL;
    char* fileName = NULL;
    char* user = argv[2];
    int portPos = 0;

    // Check if post set position of port and key/filename arguments.
    if (strcmp(argv[1], "post") == 0) {
        portPos = 5;

        key = argv[4];
        fileName = argv[3];
    }
    //Check if get and set position of port and get key argument.
    else if (strcmp(argv[1], "get") == 0) {
        portPos = 4;

        key = argv[3];
    }

    int i;
    // Check if all values in port argument are integers and output error/exit if not
    for (i = 0; i < strlen(argv[portPos]); i++) {
        if (!isdigit(argv[portPos][i])) {
            fprintf(stderr, "Port number argument can only be an integer value.\n");
            exit(1);
        }
    }

    //Set port from command argument
    int port = atoi(argv[portPos]);

    //If port number is invalid range, throw error
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Port number must be in a valid range of values (1 through 65535).\n");
        exit(1);
    }

    char* encryptedMsg = NULL;
    // If post command, encrypt message based on key
    if (strcmp(argv[1], "post") == 0) {
        encryptedMsg = encryptMessage(key, fileName);
    }

    // Initiate connection with server through port provided
    int socket = initiateConnection(port);

    // Send command message
    send(socket, argv[1], strlen(argv[1]), 0);

    // Buffer receives message
    char* readBuffer = malloc(sizeof(char) * 1024);
    memset(readBuffer, '\0', sizeof(char) * 1024);

    // Wait for acceptance message (command)
    recv(socket, readBuffer, 16, 0);

    // Send user
    send(socket, user, strlen(user), 0);

    // Buffer reset to receive message again
    memset(readBuffer, '\0', sizeof(char) * 1024);

    // Wait for acceptance message (user)
    recv(socket, readBuffer, 13, 0);

    // If operating in post mode, send encrypted message
    if (strcmp(argv[1], "post") == 0) {
        // Send encrypted message to server over provided socket
        sendMessage(socket, encryptedMsg);
    }
    // If operating in get mode, receive encrypted message and decrypt it
    else if (strcmp(argv[1], "get") == 0) {
        encryptedMsg = receiveEncryptedMessage(socket);

        // If the encrypted message is not empty, then decrypt the message and output to stdout.
        if (strcmp(encryptedMsg, "") != 0) {
            char *decryptedMsg = NULL;
            // Decrypt message based on key
            decryptedMsg = decryptMessage(key, encryptedMsg);

            printf("%s\n", decryptedMsg);

            free(decryptedMsg);
        }
    }

    // Free read buffer memory before exit
    free(readBuffer);
    free(encryptedMsg);

    return 0;
}