#include "shared.h"


int main(){
    int running = 1; //flag for main loop
    pid_t pid = fork();
    struct message_t msgData;

    /* Local Store Variables */
    struct account_t *localStore = NULL;
    int returnAddress;

    /* File references */
    FILE *DBFile;
    FILE *DBTemp;
    char* DBFileStr = "./DB.txt";
    char* DBTempStr = "./DB_temp.txt";


    /* Title */
    char *line = NULL;
    size_t len = 0;
    int accountFound = 0;
    ssize_t read;

    switch(pid){
        case -1:
            printf("fork failed\n");
            exit(0);
        /* Child Process */
        case 0:
            execv("./DB_editor", NULL);
            break;
        /* Parent Process */
        default:
            break;
    }
    /* Create message queue */
    int toATMqueue = msgget((key_t) toATM_key, 0666 | IPC_CREAT);
    if (toATMqueue == -1) {
        fprintf(stderr, "msgget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    int toDBqueue = msgget((key_t) toDB_key, 0666 | IPC_CREAT);
    if (toDBqueue == -1) {
        fprintf(stderr, "msgget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    while(running){
        /* Receive next message (regardless of type) from message queue */
        if (msgrcv(toDBqueue, (void *) &msgData, BUFSIZ, 0, 0) == 0) {
            fprintf(stderr, "msgrcv failed with error: %d\n", errno);
            exit(EXIT_FAILURE);
        }
        //Add block to get which ATM sent this (return address)
        //Get return address
        returnAddress = msgData.msg_type;
        printf("Message Type: %d\n", returnAddress);
        printf("Message Text: %s\n", msgData.msg_text);

        /* If this is a balance message */
        if (strcmp(msgData.msg_text, "BALANCE\0") == 0) {
            printAccounts(localStore);
            struct account_t * account = getAccount(localStore, msgData.msg_type);
            printf("found account\n");
            printf("Account: %f\n", account->balance);
            char *balance = balanceToString(account->balance);
            strcpy(msgData.msg_text, balance);

            if (msgsnd(toATMqueue, (void *) &msgData, 500, 0) == -1) {
                fprintf(stderr, "msgsnd failed\n");
                exit(EXIT_FAILURE);
            }
        }
        /* If there is a withdraw message */
        else if(strncmp(msgData.msg_text,"WITHDRAW",8) == 0){
            float withdrawlAmount = atof(msgData.msg_text + 9);
            struct account_t* account = getAccount(localStore, returnAddress);
            if(account->balance < withdrawlAmount){
                strcpy(msgData.msg_text, "NSF");
                if (msgsnd(toATMqueue, (void *) &msgData, 500, 0) == -1) {
                    fprintf(stderr, "msgsnd failed\n");
                    exit(EXIT_FAILURE);
                }
            }
            /* There are sufficient funds, continue with withdraw */
            else {
                /* Open database files */
                DBFile = fopen(DBFileStr, "r");
                DBTemp = fopen(DBTempStr, "w");

                //tracking variables
                int totalAccounts = 0;
                char currchar;

                //Find the total number of lines that corresponds to the number of accounts in the db
                currchar = getc(DBFile);
                //Increments if there is a new line
                while(currchar != EOF) {
                    //Count whenever new line is encountered
                    if (currchar == '\n') {
                        totalAccounts++;
                    }

                    //take the next char from file
                    currchar = getc(DBFile);
                }

                //Close and reopen to refresh file
                fclose(DBFile);
                DBFile = fopen(DBFileStr, "r");

                for(int i = 0; i< totalAccounts +1; i++) {
                    //read the line
                    getline(&line, &len, DBFile);

                    //Malloc and store account line read in
                    char *tempmessage = malloc(sizeof(char) * strlen(line));
                    strcpy(tempmessage, line);

                    //Edit balance if it is the correct account
                    if (strncmp(tempmessage, account->accountNumber, 5) == 0) {
                        account->balance = account->balance - withdrawlAmount;
                        fprintf(DBTemp, "%s", accountToString(account));
                        if (i != totalAccounts) {
                            fprintf(DBTemp, "\n");
                        }
                    } else {
                        fprintf(DBTemp, "%s", tempmessage);
                    }
                    //Free tempmessage in memory
                    free(tempmessage);
                }

                //Close files
                fclose(DBFile);
                fclose(DBTemp);

                //Change the database temp to the db file
                remove(DBFileStr);
                rename(DBTempStr, DBFileStr);

                //Send the FUNDS_OK message back to ATM
                strcpy(msgData.msg_text, "FUNDS_OK");
                if (msgsnd(toATMqueue, (void *) &msgData, 500, 0) == -1) {
                    fprintf(stderr, "msgsnd failed\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
        /* If there is a withdraw message */
        else if(strncmp(msgData.msg_text,"DEPOSIT",7) == 0){
            float depositAmount = atof(msgData.msg_text + 8);
            struct account_t* account = getAccount(localStore, returnAddress);
            /* There are sufficient funds, continue with withdraw */

            /* Open database files */
            DBFile = fopen(DBFileStr, "r");
            DBTemp = fopen(DBTempStr, "w");

            //tracking variables
            int totalAccounts = 0;
            char currchar;

            //Find the total number of lines that corresponds to the number of accounts in the db
            currchar = getc(DBFile);
            //Increments if there is a new line
            while(currchar != EOF) {
                //Count whenever new line is encountered
                if (currchar == '\n') {
                    totalAccounts++;
                }

                //take the next char from file
                currchar = getc(DBFile);
            }

            //Close and reopen to refresh file
            fclose(DBFile);
            DBFile = fopen(DBFileStr, "r");

            for(int i = 0; i< totalAccounts +1; i++) {
                //read the line
                getline(&line, &len, DBFile);

                //Malloc and store account line read in
                char *tempmessage = malloc(sizeof(char) * strlen(line));
                strcpy(tempmessage, line);

                //Edit balance if it is the correct account
                if (strncmp(tempmessage, account->accountNumber, 5) == 0) {
                    account->balance = account->balance + depositAmount;
                    fprintf(DBTemp, "%s", accountToString(account));
                    if (i != totalAccounts) {
                        fprintf(DBTemp, "\n");
                    }
                } else {
                    fprintf(DBTemp, "%s", tempmessage);
                }
                //Free tempmessage in memory
                free(tempmessage);
            }

            //Close files
            fclose(DBFile);
            fclose(DBTemp);

            //Change the database temp to the db file
            remove(DBFileStr);
            rename(DBTempStr, DBFileStr);

            //Send the FUNDS_OK message back to ATM
            strcpy(msgData.msg_text, "DEPOSIT_OK");
            if (msgsnd(toATMqueue, (void *) &msgData, 500, 0) == -1) {
                fprintf(stderr, "msgsnd failed\n");
                exit(EXIT_FAILURE);
            }

        }
        /* If there is an update from the DB Editor*/
        else if(strncmp(msgData.msg_text, "DB_UPDATE",9) == 0){
            struct account_t* newAccount = createAccount(msgData.msg_text + 10);
            char* newAccountStr = accountToString(newAccount);
            free(newAccount);
            /* Open database files */
            DBFile = fopen(DBFileStr, "a");
            fprintf(DBFile, "\n%s", newAccountStr);
            free(newAccountStr);
            fclose(DBFile);
        }
        /* If there is a login message*/
        else{
            struct account_t* acc = createLoginAccount(msgData.msg_text);
            acc->returnAddress = returnAddress;
            printf("Made Account: %s\n", acc->accountNumber);

            //Open the Database text file
            DBFile = fopen("./DB.txt", "r+");

            /*Check if account exists in database */
            while ((read = getline(&line, &len, DBFile)) != -1) {
                int size = strlen(line); //Get length of the line

                struct account_t* accLine = createAccountFromLine(line);
                printf("Made Account from Line\n");

                /* Check if this account number matches the one we're looking for */
                if(strcmp(accLine->accountNumber, acc->accountNumber) == 0){
                    printf("Found Account\n");
                    //If user provided pin matches database pin (with encryption)
                    accountFound = 1;

                    printf("%s\n",acc->pin);
                    printf("%s\n",accLine->pin);
                    
                    int atmPin = atoi(acc->pin);
                    int dbPin = atoi(accLine->pin);
                    dbPin++;

                    if(atmPin == dbPin){
                        printf("Pin matches\n");
                        /* Send OK message back to ATM */
                        struct message_t okMessage;
                        strcpy(okMessage.msg_text, "OK\0");
                        okMessage.msg_type = acc->returnAddress;
                        //Send message to ATM through IPC message queue
                        if (msgsnd(toATMqueue, (void *) &okMessage, 500, 0) == -1) {
                            fprintf(stderr, "msgsnd failed\n");
                            exit(EXIT_FAILURE);
                        }
                        printf("Sent message\n");
                        //Create node in local store
                        acc->balance = accLine->balance;
                        strncpy(acc->pin, accLine->pin, 3);
                        printf("Entering addNode\n");
                        localStore = addNode(localStore, acc);


                    } else{
                        acc->loginAttempts++; //Wrong attempt, increment login attempts!
                        printf("Wrong Pin\n");

                        /* Send fail message to ATM through IPC message queue */
                        struct message_t failMessage;
                        strcpy(failMessage.msg_text, "PIN_WRONG");
                        failMessage.msg_type = acc->returnAddress;
                        //Send message to ATM through IPC message queue
                        if (msgsnd(toATMqueue, (void *) &failMessage, 500, 0) == -1) {
                            fprintf(stderr, "msgsnd failed\n");
                            exit(EXIT_FAILURE);
                        }

                        /* If this is the 3rd wrong attempt, block the account */
                        if(acc->loginAttempts >= 3){
                            //Edit the db to put an x as the first character
                            //Open both original file and temp file
                            DBFile = fopen(DBFileStr, "r");
                            DBTemp = fopen(DBTempStr, "w");

                            /* Read file to determine total accounts (lines) in database */
                            int totalAccounts = 0;
                            char currChar;

                            currChar = getc(DBFile);
                            while(currChar != EOF){
                                /* Count when newline is encountered */
                                if(currChar == '\n'){
                                    totalAccounts++;
                                }
                                currChar = getc(DBFile); //Get next character from file

                            }

                            /* Now update database by using temp file to reflect edits */
                            fclose(DBFile);
                            DBFile = fopen("./DB.txt", "r");

                            for (int i = 0; i < totalAccounts + 1; i++) {
                                getline(&line, &len, DBFile);

                                char* tempMessage = malloc(sizeof(char) * strlen(line));
                                strcpy(tempMessage, line);

                                if(strncmp(tempMessage, acc->accountNumber, 5) == 0){
                                    strncpy(tempMessage, "X", 1);
                                }

                                fprintf(DBTemp, "%s", tempMessage);

                                free(tempMessage);

                            }
                            /* Close both database files after edit is done, move temp to DB */
                            fclose(DBFile);
                            fclose(DBTemp);

                            /* Change the database temp file to file */
                            remove("./DB.txt");
                            rename("./DB_temp.txt", "DB.txt");
                        }
                    }
                }

                deleteAccount(accLine);

            }

            /* If the user provided account was not found, send appropriate message back to ATM */
            if(!accountFound){
                /* Send account not found message to ATM through IPC message queue */
                struct message_t failMessage;
                strcpy(failMessage.msg_text, "ANF");
                failMessage.msg_type = acc->returnAddress;
                //Send message to ATM through IPC message queue
                if (msgsnd(toATMqueue, (void *) &failMessage, 500, 0) == -1) {
                    fprintf(stderr, "msgsnd failed\n");
                    exit(EXIT_FAILURE);
                }

            }

            accountFound = 0; //Reset flag

        }

    }


    return 0;
}