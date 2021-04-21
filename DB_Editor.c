#include "shared.h"

int main() {
    char accountNum[BUFSIZ];
    char pinNum[BUFSIZ];
    char accountBal[BUFSIZ];
    struct message_t pin_data;
    size_t size;
    int msgid;
    //Create message queue
    msgid = msgget((key_t) toDB_key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        fprintf(stderr, "msgget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("Please Enter Account Number:");
        fflush(stdin);
        fgets(accountNum, BUFSIZ, stdin);
        printf("Please Enter PIN Number:");
        fflush(stdin);
        fgets(pinNum, BUFSIZ, stdin);
        printf("Please Enter Available Funds:");
        fflush(stdin);
        fgets(accountBal, BUFSIZ, stdin);

        //convert the \n to commas
        accountNum[strcspn(accountNum, "\n")] = ',';
        pinNum[strcspn(pinNum, "\n")] = ',';
        accountBal[strcspn(accountBal, "\n")] = '\0';

        //create the message to send to the DB Server
        size = strlen(accountNum) + strlen(pinNum) + strlen(accountBal) + strlen("DB_UPDATE,") + 2;
        char *message = malloc(size);
        strncpy(message, "DB_UPDATE,", strlen("DB_UPDATE,"));
        size = strlen("DB_UPDATE,");
        strncpy(message + size, accountNum, strlen(accountNum));
        size = strlen("DB_UPDATE,") + strlen(accountNum);
        strncpy(message + size, pinNum, strlen(pinNum));
        size = strlen("DB_UPDATE,") + strlen(accountNum) + strlen(pinNum);
        strncpy(message + size, accountBal, strlen(accountBal));

        strcpy(pin_data.msg_text, message);
        pin_data.msg_type = 1;
        if (msgsnd(msgid, (void *) &pin_data, 500, 0) == -1) {
            fprintf(stderr, "msgsnd failed\n");
            exit(EXIT_FAILURE);
        }

        free(message);
    }
}

