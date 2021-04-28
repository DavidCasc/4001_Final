#include "shared.h"

void  SIGINT_handler(int);
int main() {

    /* Input Variables */
    char accountNum[BUFSIZ];
    char pinNum[BUFSIZ];
    char accountBal[BUFSIZ];
    char buffer[BUFSIZ];

    /* MISC */
    struct message_t pin_data;
    size_t size;

    //Variable for IPC
    int msgid, shmid;
    //Create semaphore for rates shared memory
    int ratessemid;
    struct sembuf p = { 0, -1, SEM_UNDO};
    struct sembuf v = { 0, +1, SEM_UNDO};

    //Create message queue
    msgid = msgget((key_t) toDB_key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        fprintf(stderr, "msgget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    //Create shared memory
    shmid = shmget((key_t) ratesKey, sizeof(struct rates_t),0666 | IPC_CREAT);
    if (shmid == -1){
        fprintf(stderr, "shmget failed with error: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    //Attach shared memory
    struct rates_t *rates = (struct rates_t*) shmat(shmid,(void*)0,0);
    if (rates ==(void *)-1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }

    //Create a semaphore for the shared memory for the variable rates
    ratessemid = semget((key_t)ratesSemKey, 1, 0600 | IPC_CREAT);
    if(ratessemid < 0){
        printf("Failed to create semaphore\n");
        exit(-1);
    }

    //set the initial rates
    if(semop(ratessemid, &p, 1) < 0)
    {
        perror("semop p failed\n");
        exit(-1);
    }
    rates->negInterest = 0.02;
    rates->posInterest = 0.01;
    if(semop(ratessemid, &v, 1) < 0)
    {
        perror("semop v failed\n");
        exit(-1);
    }

    //Install signal
    if (signal(SIGINT, SIGINT_handler) == SIG_ERR) {
        printf("SIGINT install error\n");
        exit(1);
    }
    //Run infinitely
    while (1) {
        /* Enter a main menu where you can choose to edit interest or add account*/
        printf("Press \"I\" to change Interest Rates and \"A\" to add account:");
        fflush(stdin);
        fgets(buffer, BUFSIZ, stdin);
        /* If editing the interest is chosen create a new menu */
        if(strncmp(buffer,"I",1) == 0){
            //Get user input for the positive rate
            printf("What is the new positive interest rate(1 = 1%%):");
            char posRate[BUFSIZ];
            fgets(posRate, BUFSIZ, stdin);

            //Convert to percent
            float posFloat = atof(posRate);
            posFloat = posFloat/100;

            //Get input for the negative rate
            printf("What is the new negative interest rate(1= 1%%):");
            char negRate[BUFSIZ];
            fgets(negRate, BUFSIZ, stdin);

            //Convert to percent
            float negFloat = atof(negRate);
            negFloat = negFloat/100;

            /* Enter the critical code and lock the share memory*/
            if(semop(ratessemid, &p, 1) < 0)
            {
                perror("semop p failed\n");
                exit(-1);
            }
            //Update the rates
            rates->negInterest = negFloat;
            rates->posInterest = posFloat;

            //Unlock the shared memory
            if(semop(ratessemid, &v, 1) < 0)
            {
                perror("semop v failed\n");
                exit(-1);
            }

        }
        /* Enter a form to create a new account */
        else if(strncmp(buffer,"A",1) ==0) {
            //Get input for the Account number
            printf("Please Enter Account Number:");
            fflush(stdin);
            fgets(accountNum, BUFSIZ, stdin);

            //Get input for the pin number
            printf("Please Enter PIN Number:");
            fflush(stdin);
            fgets(pinNum, BUFSIZ, stdin);

            //Get input for the funds available
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

	    printf("Message: %s\n", message);
	    //Copy string to message and send
            strcpy(pin_data.msg_text, message);
            pin_data.msg_type = 1;
            if (msgsnd(msgid, (void *) &pin_data, 500, 0) == -1) {
                fprintf(stderr, "msgsnd failed\n");
                exit(EXIT_FAILURE);
            }
            printf("Before free\n");
            free(message);
	    printf("After free\n");
        }
    }
}
void  SIGINT_handler(int sig)
{
    exit(1);
}

