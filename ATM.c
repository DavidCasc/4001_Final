#include "shared.h"
void  SIGINT_handler(int sig);

/*
 * The atm component will run in an infinite loop until the user presses
 * X.
 */
int main() {
    /* Input variables*/
    char accountNum[BUFSIZ];
    char pin[BUFSIZ];
    char buffer[BUFSIZ];
    char amountWithdrawn[BUFSIZ];
    char *concat;
    size_t size;

    /* Flags and Counters */
    int loggedin = 0;
    int loginAttempts = 0;

    /* Message data */
    struct message_t pin_data;
    int messageType = getpid();

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
    //Install signal
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR) {
        printf("SIGINT install error\n");
        exit(1);
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
            //Exit if the user chooses X
            if(strncmp(accountNum, "X", 1) == 0){
                strcpy(pin_data.msg_text, "X");
                pin_data.msg_type = messageType;
                if (msgsnd(toDBqueue, (void *) &pin_data, 500, 0) == -1) {
                    fprintf(stderr, "msgsnd failed\n");
                    exit(EXIT_FAILURE);
                }
                break;
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
            //move to next while loop if correct
            if (strncmp(pin_data.msg_text, "OK\0", 3) == 0) {
                loggedin = 1;

            //do not increment if account was not found
            } else if(strncmp(pin_data.msg_text, "ANF", 3) == 0) {
                printf("Account was not found, try again.\n");
                continue;

            //increment the counter if the account was found but pin was wrong
            }else {
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
            printf("Press \"B\" for Balance or \"W\" for Withdrawals or \"D\" for Deposit \"L\" to apply for a loan \" or \"X\" to Quit:");
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
            /* Handle Loan */
            else if(strncmp(buffer, "L", 1) == 0){
                //Create console ouotput for the menu option
                printf("How much are you going to take out as a loan:");
                fflush(stdin);
                fgets(amountWithdrawn, BUFSIZ, stdin);

                //Realloc so that the message can be concatenated to be sent  off
                size = strlen(amountWithdrawn) + strlen("LOAN,") + 2;
                concat = realloc(concat, size);
                amountWithdrawn[strcspn(amountWithdrawn, "\n")] = '\0';
                strcpy(concat, "LOAN,");
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
                if (strncmp(pin_data.msg_text, "LOAN_OK", 10) == 0) {
                    printf("Loan Granted!\n");
                }
            }
            //Exit if the user chooses X
            else if(strncmp(buffer, "X", 1) == 0){
                strcpy(pin_data.msg_text, "X");
                pin_data.msg_type = messageType;
                if (msgsnd(toDBqueue, (void *) &pin_data, 500, 0) == -1) {
                    fprintf(stderr, "msgsnd failed\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }

        }


        return 0;
    }
}
void  SIGINT_handler(int sig)
{
    exit(1);
}