#include "shared.h"

/*
 * The atm component will run in an infinite loop until the user presses
 * X.
 */
int main(int argc, char *argv[]) {
    //Declare Variables
    char accountNum[BUFSIZ];
    char pin[BUFSIZ];
    char buffer[BUFSIZ];
    char *concat;
    int loggedin = 0;
    int loginAttempts = 0;
    struct message_t pin_data;
    int wrongCred = 0;
    size_t size;
    char amountWithdrawn[BUFSIZ];
    int messageType = getpid();

    //Start malloc

    //Create toATM message queue
    int toATMqueue = msgget((key_t) toATM_key, 0666 | IPC_CREAT);
    if (toATMqueue == -1) {
        fprintf(stderr, "msgget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    //Create toDB message queue
    int toDBqueue = msgget((key_t) toDB_key, 0666 | IPC_CREAT);
    if (toDBqueue == -1) {
        fprintf(stderr, "msgget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    //Start an infinte loop
    while (1) {
        while (!loggedin) {
            //Poll for user input for account number
            if(loginAttempts == 0) {
                printf("Please Enter Account Number:");
                fflush(stdin);
                fgets(accountNum, BUFSIZ, stdin);
            }

            //Poll for user input for the pin number
            printf("Please Enter Pin Number:");
            fflush(stdin);
            fgets(pin, BUFSIZ, stdin);

            //Concatenate the two strings together to send message
            if(loginAttempts == 0) {
                accountNum[strcspn(accountNum, "\n")] = ',';
            }
            pin[strcspn(pin, "\n")] = '\0';
            size = strlen(accountNum) + strlen(pin) + 2;
            concat = (char *) malloc(sizeof(char) * size);

            if (concat == NULL) {
                exit(0);
            }

            strncpy(concat, accountNum, strlen(accountNum));
            strncpy(concat + strlen(accountNum), pin, strlen(pin));

            //Generate message for queue
            pin_data.msg_type = messageType;
            strcpy(pin_data.msg_text, concat);
            if (msgsnd(toDBqueue, (void *) &pin_data, 500, 0) == -1) {
                fprintf(stderr, "msgsnd failed\n");
                exit(EXIT_FAILURE);
            }

            //Wait for a message back from the db server
            if (msgrcv(toATMqueue, (void *) &pin_data, BUFSIZ, messageType, 0) == 0) {
                fprintf(stderr, "msgrcv failed with error: %d\n", errno);
                exit(EXIT_FAILURE);
            }
            if (strncmp(pin_data.msg_text, "OK\0", 3) == 0) {
                loggedin = 1;
            } else {
                loginAttempts++;
                if(loginAttempts >= 3){
                    printf("Account is blocked\n");
                    loginAttempts = 0;
                }
            }
        }
        //Create a while loop for when the user is logged in
        while (loggedin) {
            //Create a menu option for user to select
            printf("Press \"B\" for Balance or \"W\" for Withdrawals or \"D\" for Deposit or \"X\" to Quit:");
            fflush(stdin);
            fgets(buffer, BUFSIZ, stdin);
            //Handle if the user chooses to view balance
            if (strncmp(buffer, "B", 1) == 0) {
                //Create a message for balance
                pin_data.msg_type = messageType;
                strcpy(pin_data.msg_text, "BALANCE\0");
                if (msgsnd(toDBqueue, (void *) &pin_data, 500, 0) == -1) {
                    fprintf(stderr, "msgsnd failed\n");
                    exit(EXIT_FAILURE);
                }
                //listen for the message back with the balance
                if (msgrcv(toATMqueue, (void *) &pin_data, BUFSIZ, messageType, 0) == 0) {
                    fprintf(stderr, "msgrcv failed with error: %d\n", errno);
                    exit(EXIT_FAILURE);
                }

                printf("Current Balance is: %s\n", pin_data.msg_text);
            }
            //Handle if the user wants to withdraw
            else if(strncmp(buffer, "W", 1) == 0){
                //Create console ouotput for the menu option
                printf("How much are you going to withdraw:");
                fflush(stdin);
                fgets(amountWithdrawn, BUFSIZ, stdin);

                //Realloc so that the message can be concatenated to be sent  off
                size = strlen(amountWithdrawn) + strlen("WITHDRAW,") + 2;
                concat = realloc(concat, size);
                amountWithdrawn[strcspn(amountWithdrawn, "\n")] = '\0';
                strcpy(concat, "WITHDRAW,");
                strcat(concat, amountWithdrawn);

                //Transfer the concatenated message into the message body
                strcpy(pin_data.msg_text, concat);
                pin_data.msg_type = messageType;
                if (msgsnd(toDBqueue, (void *) &pin_data, 500, 0) == -1) {
                    fprintf(stderr, "msgsnd failed\n");
                    exit(EXIT_FAILURE);
                }

                //Wait for a response back for the Server
                if (msgrcv(toATMqueue, (void *) &pin_data, BUFSIZ, messageType, 0) == 0) {
                    fprintf(stderr, "msgrcv failed with error: %d\n", errno);
                    exit(EXIT_FAILURE);
                }
                if (strncmp(pin_data.msg_text, "NSF\0", 4) == 0) {
                    printf("There are not enough funds.\n");
                } else {
                    printf("Money is being dispensed.\n");
                }

            }
            else if(strncmp(buffer, "D", 1) == 0){
                //Create console ouotput for the menu option
                printf("How much are you going to deposit:");
                fflush(stdin);
                fgets(amountWithdrawn, BUFSIZ, stdin);

                //Realloc so that the message can be concatenated to be sent  off
                size = strlen(amountWithdrawn) + strlen("DEPOSIT,") + 2;
                concat = realloc(concat, size);
                amountWithdrawn[strcspn(amountWithdrawn, "\n")] = '\0';
                strcpy(concat, "DEPOSIT,");
                strcat(concat, amountWithdrawn);

                //Transfer the concatenated message into the message body
                strcpy(pin_data.msg_text, concat);
                pin_data.msg_type = messageType;
                if (msgsnd(toDBqueue, (void *) &pin_data, 500, 0) == -1) {
                    fprintf(stderr, "msgsnd failed\n");
                    exit(EXIT_FAILURE);
                }

                //Wait for a response back for the Server
                if (msgrcv(toATMqueue, (void *) &pin_data, BUFSIZ, messageType, 0) == 0) {
                    fprintf(stderr, "msgrcv failed with error: %d\n", errno);
                    exit(EXIT_FAILURE);
                }
                if (strncmp(pin_data.msg_text, "DEPOSIT_OK", 10) == 0) {
                    printf("Funds have been deposited into your account!\n");
                }

            }
            //Exit if the user chooses X
            else if(strncmp(buffer, "X", 1) == 0){
                
                break;
            }

        }

        free(concat);
        return 0;
    }
}