// Author: Justin Tromp
// Date: 05/25/2020
// Description: Keygen: Allows for creation of a randomly generated key used to encrypt and decrypt
// text/files. The key files generated from keygen.c can be used with otp_d and otp. Must pass
// a command line argument for the size of the key, which will consist of randomly generated characters
// of that length (capital letters or a space are all possible).
// References: Previous Assignments
// https://www.geeksforgeeks.org/generating-random-number-range-c/

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

// Takes an unsigned int value as parameter and generates a string of characters
// (capital letters and spaces) randomly of the provided length. This string is returned.
// Pre-conditions: Must be passed a valid keyLength value as parameter.
// Post-conditions: Returns a randomly generated key of length provided.
char* generateRandomString (unsigned int keyLength) {
    int lowerLimit = 65;
    // 90 is end of capital characters in ASCII table, 91 will signify a space.
    int upperLimit = 91;

    char* createdKey = malloc(sizeof(char) * (keyLength + 5));
    memset(createdKey, '\0', (keyLength + 5));

    int i;
    // Loop number of times as keyLength to generate a random value within the range of
    // possible values.
    for (i = 0; i < keyLength; i++) {
        // Determine random value in range of limits
        int value = (rand() % (upperLimit - lowerLimit + 1)) + lowerLimit;

        char c = ' ';
        // Get character
        if (value != 91) {
            c = value;
        }
        else {
            c = 32;
        }

        // Set character in string
        createdKey[i] = c;
    }

    // Add newline character to end of generated string
    createdKey[keyLength] = '\n';

    return createdKey;
}

// Main function gathers the arguments from command line and performs error checking.
// Using the provided command line argument of length of key, a randomly generated key
// of that size is generated and output to stdout.
int main(int argc, char const *argv[]) {
    // Use current system time as seed for random generation
    srand(time(0));

    // If argc is less than or more than 2, output error message for incorrect number of arguments and exit.
    if (argc < 2) {
        fprintf(stderr, "Must provide a command line argument for key length.\n");
        exit(1);
    }
    else if (argc > 2) {
        fprintf(stderr, "Command line argument should only include one argument for key length.\n");
        exit(1);
    }

    int i;
    // Check if all values in argument are integers and output error/exit if not
    for (i = 0; i < strlen(argv[1]); i++) {
        if (!isdigit(argv[1][i])) {
            fprintf(stderr, "Key length argument can only be an integer value.\n");
            exit(1);
        }
    }

    unsigned int keyLength = 0;
    // Get the keylength from the arguments provided
    keyLength = atoi(argv[1]);

    char* key = NULL;
    key = generateRandomString(keyLength);

    // If there is a key generated, then output the key to stdout
    if (key != NULL) {
        // Output key to stdout
        fprintf(stdout, "%s", key);
        fflush(stdout);

        free(key);
    }

    return 0;
}
