#include "shared.h"

void  SIGINT_handler(int);
int main(){

    //Create variables for tracking the new pids
    pid_t pid_interest, pid_editor;
    pid_editor = fork();

    /* Local Store Variables */
    struct account_t *localStore = NULL;
    int returnAddress;

    /* File references */
    FILE *DBFile;
    FILE *DBTemp;
    FILE *logFile;
    char* DBFileStr = "./DB.txt";
    char* DBTempStr = "./DB_temp.txt";
    char* logStr = "./deadlockslog.txt";
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    /* Semaphore Variables */
    int semid, ratessemid, logsemid;
    union semun u, ratessem, logssem;
    struct sembuf p = { 0, -1, SEM_UNDO};
    struct sembuf v = { 0, +1, SEM_UNDO};

    /* Variables for shared memory */
    int shmid;
    struct rates_t *rates;

    /* Variables for Message queues */
    int toATMqueue;
    int toDBqueue;


    /* MISC */
    int accountFound = 0; //Flag for if the account is found
    int running = 1; //flag for main loop
    struct message_t msgData; //Create a message for the message queue

    //Fork to start the editor process
    switch(pid_editor){
        case -1:
            printf("fork failed\n");
            exit(0);
        /* Child Process */
        case 0:
            execv("DB_Editor", NULL);
            break;
        /* Parent Process */
        default:
            break;
    }

    /* Create message queue for outward (to ATM) messages */
    toATMqueue = msgget((key_t) toATM_key, 0666 | IPC_CREAT);
    if (toATMqueue == -1) {
        fprintf(stderr, "msgget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Create message queue for inward (to Server) messages*/
    toDBqueue = msgget((key_t) toDB_key, 0666 | IPC_CREAT);
    if (toDBqueue == -1) {
        fprintf(stderr, "msgget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    /* Set up semaphore for the database text file */
    /* Create semaphore for accesing the database text file*/
    semid = semget((key_t)dbSemKey, 1, 0600 | IPC_CREAT);
    if(semid < 0){
        printf("Failed to create semaphore\n");
        exit(-1);
    }
    //Initialize semaphore with a value of one
    u.val = 1;

    //Set the Value
    if (semctl(semid, 0, SETVAL, u) < 0) {
        fprintf(stderr, "semctl failed with error: %d\n", errno);
    }

    //Fork to start the interestCalculator
    pid_interest = fork();
    switch(pid_interest){
        case -1:
            printf("Fork Failed\n");
            exit(-1);
        case 0:
            execv("interestCalculator", NULL);
            break;
        default:
            break;
    }

    /* Set up a semaphore for the shared memory for the variable rates */
    //get IPC for sem
    ratessemid = semget((key_t)ratesSemKey, 1, 0600 | IPC_CREAT);
    if(ratessemid < 0){
        printf("Failed to create semaphore\n");
        exit(-1);
    }

    //Initialize a the variable rates semaphore to a value of 1
    ratessem.val = 1;

    //Set the value
    if (semctl(ratessemid, 0, SETVAL, ratessem) < 0) {
        fprintf(stderr, "semctl failed with error: %d\n", errno);
    }

    /* Create shared memory for the variable interest rate */
    shmid = shmget((key_t) ratesKey, sizeof(struct rates_t),0666 | IPC_CREAT);
    if (shmid == -1){
        fprintf(stderr, "shmget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    //Attach shared memory
    rates = (struct rates_t*) shmat(shmid,(void*)0,0);
    if (rates ==(void *)-1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }

    /* Create the semaphore for the logs text file */
    logsemid = semget((key_t)logSemKey, 1, 0600 | IPC_CREAT);
    if(logsemid < 0){
        printf("Failed to create semaphore\n");
        exit(-1);
    }

    //Initialize a semaphore for the logs
    logssem.val = 1;

    //set the value of the semaphore
    if (semctl(logsemid, 0, SETVAL, logssem) < 0) {
        fprintf(stderr, "semctl logs failed with error: %d\n", errno);
    }

    //Install signal
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR) {
        printf("SIGINT install error\n");
        exit(1);
    }

    //Running infinitely taking from inwards queue
    while(running){
        /* Receive next message (regardless of type) from message queue */
        if (msgrcv(toDBqueue, (void *) &msgData, BUFSIZ, 0, 0) == 0) {
            fprintf(stderr, "msgrcv failed with error: %d\n", errno);
            exit(EXIT_FAILURE);
        }
        //Add block to get which ATM sent this (return address)
        //Get return address
        returnAddress = msgData.msg_type;

        /* If this is a balance message */
        if (strcmp(msgData.msg_text, "BALANCE") == 0) {
            struct account_t * account = getAccount(localStore, msgData.msg_type);
            account = updateAccount(account);
            char *balance = balanceToString(account->balance);
            strcpy(msgData.msg_text, balance);

            //Send Balance response on the outward queue
            if (msgsnd(toATMqueue, (void *) &msgData, 500, 0) == -1) {
                fprintf(stderr, "msgsnd failed\n");
                exit(EXIT_FAILURE);
            }
        }
        /* If there is a withdraw message */
        else if(strncmp(msgData.msg_text,"WITHDRAW",8) == 0){

            //Get float value from the inward message
            float withdrawlAmount = atof(msgData.msg_text + 9);

            //Find existing account
            struct account_t* account = getAccount(localStore, returnAddress);
            account = updateAccount(account);
            //Check if the balance is insufficient
            if(account->balance < withdrawlAmount){

                //Send NSF message on the outward queue
                strcpy(msgData.msg_text, "NSF");
                if (msgsnd(toATMqueue, (void *) &msgData, 500, 0) == -1) {
                    fprintf(stderr, "msgsnd failed\n");
                    exit(EXIT_FAILURE);
                }
            }
            /* There are sufficient funds, continue with withdraw */
            else {

                //Log semaphore state in shared logs file
                if(semop(logsemid, &p, 1) < 0)
                {
                    perror("semop p failed\n");
                    exit(-1);
                }

                logFile = fopen(logStr, "a");
                fprintf(logFile, "DB_SERVER: Requesting access to DB Text File\n");
                fclose(logFile);

                if(semop(logsemid, &v, 1) < 0)
                {
                    perror("semop v failed \n");
                    exit(-1);
                }

                /* Decrement the counter to block access to the db*/
                if(semop(semid, &p, 1) < 0)
                {
                    perror("semop p failed\n");
                    exit(-1);
                }

                //Log semaphore state in shared logs file
                if(semop(logsemid, &p, 1) < 0)
                {
                    perror("semop p failed\n");
                    exit(-1);
                }

                logFile = fopen(logStr, "a");
                fprintf(logFile, "DB_SERVER: Gained access to DB Text File\n");
                fclose(logFile);

                if(semop(logsemid, &v, 1) < 0)
                {
                    perror("semop v failed \n");
                    exit(-1);
                }

                /* Open database files */
                DBFile = fopen(DBFileStr, "r");
                DBTemp = fopen(DBTempStr, "w");

                //tracking variables
                int totalAccounts = 0;

                //Find the total number of lines that corresponds to the number of accounts in the db
                //Increments if there is a new line
                while((read = getline(&line, &len, DBFile)) != -1) {
                    totalAccounts++;
                }

                //Close and reopen to refresh file
                fclose(DBFile);
                DBFile = fopen(DBFileStr, "r");

                //Iterate through the accounts in the text file
                for(int i = 0; i< totalAccounts; i++) {
                    //read the line
                    getline(&line, &len, DBFile);

                    //Malloc and store account line read in
                    char *tempmessage = calloc(strlen(line), sizeof(char));
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

                /* Increment the counter unblock access to the db */
                if(semop(semid, &v, 1) < 0)
                {
                    perror("semop v failed \n");
                    exit(-1);
                }

                //Log semaphore state in shared logs file
                if(semop(logsemid, &p, 1) < 0)
                {
                    perror("semop p failed\n");
                    exit(-1);
                }

                logFile = fopen(logStr, "a");
                fprintf(logFile, "DB_SERVER: Released access to DB Text File\n");
                fclose(logFile);

                if(semop(logsemid, &v, 1) < 0)
                {
                    perror("semop v failed \n");
                    exit(-1);
                }

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
            //Turn the inward message to float
            float depositAmount = atof(msgData.msg_text + 8);

            //Find the account in local store
            struct account_t* account = getAccount(localStore, returnAddress);
            account = updateAccount(account);

            /* Edit DB to reflect deposit */
            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Requested access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            //Decrement the counter to block the access to the db
            if(semop(semid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Gained access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            /* Open database files */
            DBFile = fopen(DBFileStr, "r");
            DBTemp = fopen(DBTempStr, "w");

            //tracking variables
            int totalAccounts = 0;

            //Decrements if there is a new line
            while((read = getline(&line, &len, DBFile)) != -1) {
                totalAccounts++;

            }

            //Close and reopen to refresh file
            fclose(DBFile);
            DBFile = fopen(DBFileStr, "r");

            //Increment through the accounts in the text file
            for(int i = 0; i< totalAccounts; i++) {
                //read the line
                getline(&line, &len, DBFile);

                //Malloc and store account line read in
                char *tempmessage = calloc(strlen(line), sizeof(char));
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

            //Increment the counter to unblock access to the database
            if(semop(semid, &v, 1) < 0)
            {
                perror("semop v failed\n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Released access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            //Send the FUNDS_OK message back to ATM
            strcpy(msgData.msg_text, "DEPOSIT_OK");
            if (msgsnd(toATMqueue, (void *) &msgData, 500, 0) == -1) {
                fprintf(stderr, "msgsnd failed\n");
                exit(EXIT_FAILURE);
            }

        }
        /* If there is an update from the DB Editor*/
        else if(strncmp(msgData.msg_text, "DB_UPDATE",9) == 0){
            printf("Message: %s\n", msgData.msg_text);
            //create an accounts from the message
            struct account_t* newAccount = createAccount(msgData.msg_text + 10);
            //create a string representation of the account
            char* newAccountStr = accountToString(newAccount);
            free(newAccount);
	    printf("New line: %s\n", newAccountStr);

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Requested access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            //Decrements counter and blocks the access to the db
            if(semop(semid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Gained access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            /* Open database files */
            DBFile = fopen(DBFileStr, "a");

            //Append to the text file
            fprintf(DBFile, "%s\n", newAccountStr);
	    printf("before\n");
            free(newAccountStr);
	    printf("after\n");

            //Close file
            fclose(DBFile);

            //Increments counter to unblock access to the db
            if(semop(semid, &v, 1) < 0)
            {
                perror("semop v failed\n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Released access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }
        }
        /* Delete Node if the ATM Closes*/
        else if(strncmp(msgData.msg_text, "X", 1) == 0){
            //localStore = deleteNode(localStore, msgData.msg_type);
            kill(pid_interest, SIGINT);
            kill(pid_editor, SIGINT);
            killATM(localStore);
	    deleteMsgQueue(toATMqueue);
	    deleteMsgQueue(toDBqueue);
	    if(shmdt(rates) == -1){
		fprintf(stderr, "shdt fail\n");
		exit(EXIT_FAILURE);
	    }
	    if(shmctl(shmid, IPC_RMID, 0) == -1) {
		fprintf(stderr, "shmctl failed\n");
		exit(EXIT_FAILURE);
	    }
            exit(1);
        }
        /* Handle Loan Message */
        else if(strncmp(msgData.msg_text, "LOAN",4) == 0){
            //Get loan amount from message;
            float loanAmount = atof(msgData.msg_text+5);

            //Get account from local storage
            struct account_t *account = getAccount(localStore, msgData.msg_type);
            account = updateAccount(account);

            //Calculate debt with interest
            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Requested access to Rates Shared Memory\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }
            //Gain lock of interest rates shm
            if(semop(ratessemid, &p, 1) < 0)
            {
                perror("semop p failed \n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Gained access to Rates Shared Memory\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            //Edit the loan amount to add interest
            loanAmount = loanAmount + (loanAmount * rates->negInterest);
            account->balance = account->balance - loanAmount;


            /* Edit DB to reflect loan */
            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Requested access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            //Decrement the counter to block the access to the db
            if(semop(semid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Gained access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            //Release Lock on the shared memory for the rates
            if(semop(ratessemid, &v, 1) < 0)
            {
                perror("semop v failed\n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Released access to Rates Shared Memory\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            /* Open database files */
            DBFile = fopen(DBFileStr, "r");
            DBTemp = fopen(DBTempStr, "w");

            //tracking variables
            int totalAccounts = 0;

            //Decrements if there is a new line
            while((read = getline(&line,&len,DBFile)) != -1) {
                totalAccounts++;
            }

            //Close and reopen to refresh file
            fclose(DBFile);
            DBFile = fopen(DBFileStr, "r");

            //Increment through the accounts in the text file
            for(int i = 0; i< totalAccounts; i++) {
                //read the line
                getline(&line, &len, DBFile);

                //Malloc and store account line read in
                char *tempmessage = calloc(strlen(line), sizeof(char));
                strcpy(tempmessage, line);

                //Edit balance if it is the correct account
                if (strncmp(tempmessage, account->accountNumber, 5) == 0) {
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

            //Increment the counter to unblock access to the database
            if(semop(semid, &v, 1) < 0)
            {
                perror("semop v failed\n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Released access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            //Send the FUNDS_OK message back to ATM
            strcpy(msgData.msg_text, "LOAN_OK");
            if (msgsnd(toATMqueue, (void *) &msgData, 500, 0) == -1) {
                fprintf(stderr, "msgsnd failed\n");
                exit(EXIT_FAILURE);
            }

        }
        /* If there is a login message*/
        else{

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Requested access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            //Decrement the counter to block access to the db
            if(semop(semid, &p, 1) < 0)
            {
                perror("semop p failed \n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Gained access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
            }

            //create an account
            struct account_t* acc = createLoginAccount(msgData.msg_text);
            acc->returnAddress = returnAddress;

            //Open the Database text file
            DBFile = fopen("./DB.txt", "r+");

            /*Check if account exists in database */
            while ((read = getline(&line, &len, DBFile)) != -1) {
                int size = strlen(line); //Get length of the line

                //Create an account from the db
                struct account_t* accLine = createAccountFromLine(line);

                /* Check if this account number matches the one we're looking for */
                if(strcmp(accLine->accountNumber, acc->accountNumber) == 0){
                    //If user provided pin matches database pin (with encryption)
                    accountFound = 1;

                    //decrypt the pins
                    int atmPin = atoi(acc->pin);
                    int dbPin = atoi(accLine->pin);
                    dbPin++;

                    if(atmPin == dbPin){
                        /* Send OK message back to ATM */
                        struct message_t okMessage;
                        strcpy(okMessage.msg_text, "OK\0");
                        okMessage.msg_type = acc->returnAddress;
                        //Send message to ATM through IPC message queue
                        if (msgsnd(toATMqueue, (void *) &okMessage, 500, 0) == -1) {
                            fprintf(stderr, "msgsnd failed\n");
                            exit(EXIT_FAILURE);
                        }
                        //Create node in local store
                        acc->balance = accLine->balance;
                        strncpy(acc->pin, accLine->pin, 3);
                        localStore = addNode(localStore, acc);


                    } else{
                        acc->loginAttempts++; //Wrong attempt, increment login attempts!

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

                            while((read = getline(&line, &len, DBFile)) != -1) {
                                totalAccounts++;
                            }

                            /* Now update database by using temp file to reflect edits */
                            fclose(DBFile);
                            DBFile = fopen("./DB.txt", "r");

                            for (int i = 0; i < totalAccounts; i++) {
                                getline(&line, &len, DBFile);

                                char* tempMessage = calloc(strlen(line), sizeof(char));
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

            //Increment counter / call signal
            if(semop(semid, &v, 1) < 0)
            {
                perror("semop v failed\n");
                exit(-1);
            }

            //Log semaphore state in shared logs file
            if(semop(logsemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }

            logFile = fopen(logStr, "a");
            fprintf(logFile, "DB_SERVER: Released access to DB Text File\n");
            fclose(logFile);

            if(semop(logsemid, &v, 1) < 0)
            {
                perror("semop v failed \n");
                exit(-1);
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
void  SIGINT_handler(int sig)
{
    exit(1);
}
