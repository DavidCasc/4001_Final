#include "shared.h"

void  SIGINT_handler(int sig);

int main(){
    /* File references */
    FILE *DBFile;
    FILE *DBTemp;
    FILE *logFile;
    char* DBFileStr = "./DB.txt";
    char* DBTempStr = "./DB_temp.txt";
    char* logStr = "./deadlockslog.txt";
    size_t len;
    char *line;
    ssize_t read;

    /* Semaphore Variables*/
    int semid, ratesSemID, logsemid;
    struct sembuf p = { 0, -1, SEM_UNDO};
    struct sembuf v = { 0, +1, SEM_UNDO};

    /* Shared Memory Variables */
    int shmid;

    /* MISC */
    int running = 1;
    pid_t pid;

    //Get Semaphore for the shared memory in db.txt
    semid = semget((key_t)dbSemKey, 1, 0600 | IPC_CREAT);
    if ((semid < 0)) {
        printf("Failed to create semaphore \n");
        exit(-1);
    }

    //Get Semaphore for the share memory of the variable rates
    ratesSemID = semget((key_t)ratesSemKey, 1, 0600 | IPC_CREAT);
    if(ratesSemID < 0){
        printf("Failed to create semaphore\n");
        exit(-1);
    }

    //Create shared memory
    shmid = shmget((key_t) ratesKey, sizeof(struct rates_t),0666 | IPC_CREAT);
    if (shmid == -1){
        fprintf(stderr, "shmget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    //Attach shared memory
    struct rates_t *rates = (struct rates_t*) shmat(shmid,(void*)0,0);
    if (rates == (void *)-1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }
    //Install signal
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR) {
        printf("SIGINT install error\n");
        exit(1);
    }

    /* Create the semaphore for the logs text file */
    logsemid = semget((key_t)logSemKey, 1, 0600 | IPC_CREAT);
    if(logsemid < 0){
        printf("Failed to get logs semaphore\n");
        exit(-1);
    }

    while(running){
        //Fork to create a timer that triggers every minute
        pid = fork();
        switch(pid) {
            /* If there is an error exit*/
            case -1:
                printf("Fork Failed\n");
                exit(-1);
                /* The child waits a minute then exits*/
            case 0:
                sleep(60);
                exit(0);
                /* Parent waits for child to exit then calculates interest */
            default:
                break;
        }
        //wait for the signal from the child
        wait(0);

        //Decrement the counter which will wait and lock access to the db
        //Log semaphore state in shared logs file
        if(semop(logsemid, &p, 1) < 0)
        {
            perror("semop p failed\n");
            exit(-1);
        }

        logFile = fopen(logStr, "a");
        fprintf(logFile, "INTERESTCALCULATOR: Requesting access to DB Text File\n");
        fclose(logFile);

        if(semop(logsemid, &v, 1) < 0)
        {
            perror("semop v failed \n");
            exit(-1);
        }

        if(semop(semid, &p, 1) < 0) {
            fprintf(stderr, "semop failed with error: %d\n", errno);
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
        fprintf(logFile, "INTERESTCALCULATOR: Gained access to DB Text File\n");
        fclose(logFile);

        if(semop(logsemid, &v, 1) < 0)
        {
            perror("semop v failed \n");
            exit(-1);
        }

        sleep(30); //Sleep to increase the chances of deadlock

        //Log semaphore state in shared logs file
        if(semop(logsemid, &p, 1) < 0)
        {
            perror("semop p failed\n");
            exit(-1);
        }

        logFile = fopen(logStr, "a");
        fprintf(logFile, "INTERESTCALCULATOR: Requesting access to rates shared memory\n");
        fclose(logFile);

        if(semop(logsemid, &v, 1) < 0)
        {
            perror("semop v failed \n");
            exit(-1);
        }

        //Decrement the counter which will wait and lock the access to the variable rates
        if(semop(ratesSemID, &p, 1) < 0) {
            fprintf(stderr, "semop failed with error: %d\n", errno);
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
        fprintf(logFile, "INTERESTCALCULATOR: Gained Access to rates shared memory\n");
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

        //Increments if there is a new line
        while ((read = getline(&line, &len, DBFile)) != -1 ) {
            totalAccounts++;
        }

        //Close and reopen to refresh file
        fclose(DBFile);
        DBFile = fopen(DBFileStr, "r");

        for (int i = 0; i < totalAccounts; i++) {
            //read the line
            getline(&line, &len, DBFile);

            //Malloc and store account line read in
            char *tempstring = calloc(strlen(line), sizeof(char));
            strcpy(tempstring, line);
            struct account_t *account = createAccountFromLine(tempstring);

            //Edit the account balance by adding or subtracting the interest
            if (account->balance < 0) {
                account->balance = account->balance - (account->balance * rates->negInterest);
            } else {
                account->balance = account->balance + (account->balance * rates->posInterest);
            }

            if (i != totalAccounts) {
                fprintf(DBTemp, "%s\n", accountToString(account));
            } else {
                fprintf(DBTemp, "%s", accountToString(account));
            }

            deleteAccount(account);
        }

        //Close files
        fclose(DBFile);
        fclose(DBTemp);

        //Change the database temp to the db file
        remove(DBFileStr);
        rename(DBTempStr, DBFileStr);

        //Increment the counter to unlock the shared memory for the variable rates
        if(semop(ratesSemID, &v, 1) < 0)
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
        fprintf(logFile, "INTERESTCALCULATOR: Releasing access to rates shared memory\n");
        fclose(logFile);

        if(semop(logsemid, &v, 1) < 0)
        {
            perror("semop v failed \n");
            exit(-1);
        }
        //Increment the counter to unlock the shared memory db.txt
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
        fprintf(logFile, "INTERESTCALCULATOR: Releasing access to DB Text File\n");
        fclose(logFile);

        if(semop(logsemid, &v, 1) < 0)
        {
            perror("semop v failed \n");
            exit(-1);
        }
    }
}
void  SIGINT_handler(int sig)
{
    exit(1);
}
