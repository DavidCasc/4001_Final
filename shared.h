#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/sem.h>
#include <signal.h>
#include <math.h>

#if defined(__linux__)
#include <wait.h>
//Defines the union semun as required
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short *array;
};
#endif
struct message_t {
    long int msg_type;
    char msg_text[BUFSIZ];
};

struct account_t{
    char* accountNumber;
    char* pin;
    float balance;
    int returnAddress;
    int loginAttempts;
    struct account_t* next;
};

struct rates_t{
    float posInterest;
    float negInterest;
};


int toDB_key = 1234;
int toATM_key = 4321;
int dbSemKey = 1235;
int ratesKey = 1236;
int ratesSemKey = 1237;
int logSemKey = 1238;

/**
 * Assumes message is in the format:
 *  [AccountNum][PIN][Balance]
 * @param message
 * @return
 */
struct account_t* createAccount(char* message){
    struct account_t* acc = calloc(1,sizeof(struct account_t));
    acc->accountNumber = (char *) calloc(5, sizeof(char));
    acc->pin = (char *) calloc(3, sizeof(char));
    strncpy(acc->accountNumber, message, 5);
    strncpy(acc->pin, message+6, 3);
    acc->balance = atof(message+10);
    acc->returnAddress = -1;
    acc->loginAttempts = 0;
    acc->next = NULL;
    return acc;

}
struct account_t* createLoginAccount(char*message){
    struct account_t* acc = calloc(1,sizeof(struct account_t));
    acc->accountNumber = calloc(5,sizeof(char));
    acc->pin = calloc(3, sizeof(char));
    strncpy(acc->accountNumber, message, 5);
    strncpy(acc->pin, message + 6, 3);
    return acc;
}

/**
 * Assumes message is in the format:
 *  [AccountNum][PIN][Balance]
 * @param message
 * @return
 */
struct account_t* createAccountFromLine(char* message){

    struct account_t* acc = calloc(1,sizeof(struct account_t));
    acc->accountNumber = calloc(5,sizeof(char));
    acc->pin = calloc(3,sizeof(char));
    strncpy(acc->accountNumber, message, 5);
    strncpy(acc->pin, message + 6, 3);
    acc->balance = atof(message+10);
    acc->returnAddress = -1;
    acc->loginAttempts = 0;
    return acc;

}

/**
 * This function will deallocate memory associated with
 * account
 * @param acc
 */
void deleteAccount(struct account_t* acc){

    free(acc->accountNumber);
    free(acc->pin);
    free(acc);

}

char * balanceToString(float balance){
    int digits = floor (log10 (abs ((int)balance))) + 1;
    char* str = calloc((digits + 3), sizeof(char));

    sprintf (str, "%.2f", balance);
    return str;
}

/**
 * This function is used to convert the account struct to a string to be stored
 * in the database and also for printing
 * @param acc
 * @return
 */
char * accountToString(struct account_t* acc){
    char* balance = balanceToString(acc->balance);
    char * accStr = calloc((strlen(acc->pin) + strlen(acc->accountNumber) + strlen(balance) + 2),sizeof(char));
    strcat(accStr, acc->accountNumber);
    strcat(accStr, ",");
    strcat(accStr, acc->pin);
    strcat(accStr, ",");
    strcat(accStr, balance);
    free(balance);
    return accStr;
}
/**
 * This function will free all the memory allocated by malloc
 * for the node
 * @param node The node to be deleted
 */
struct account_t * deleteNode(struct account_t* head, int returnAddress){
    struct account_t *curr;
    struct account_t *prev;
    for(curr = head; curr != NULL; curr=curr->next){
        if(curr->returnAddress == returnAddress){
            /* If the node is the head */
            if(curr == head){
                head = curr->next;
                break;
            }
            /* If the node in the middle */
            else if(curr->next != NULL) {
                prev->next = curr->next;
                curr->next = NULL;
                break;
            }
            /* If the node is at the end */
            else {
                prev->next = NULL;
                curr->next = NULL;
                break;
            }
        }
        prev = curr;
    }
    deleteAccount(curr);
    return head;
}

struct account_t * addNode(struct account_t *head, struct account_t* newNode){
    newNode->next = head;
    head = newNode;
    return head;
}

struct account_t * getAccount(struct account_t *head, int returnAddress){
    struct account_t *curr;
    for(curr = head; curr != NULL; curr=curr->next){
        if(curr->returnAddress == returnAddress){
            return curr;
        }
    }
    return NULL;
}

struct account_t* updateAccount(struct account_t *src){
    struct account_t *temp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    FILE *DBFile;
    DBFile = fopen("./DB.txt", "r");
    /* Read file to determine total accounts (lines) in database */
    int totalAccounts = 0;
    char currChar;

    currChar = getc(DBFile);
    while((read = getline(&line, &len, DBFile)) != -1){
        totalAccounts++;

    }

    /* Now update database by using temp file to reflect edits */
    fclose(DBFile);
    DBFile = fopen("./DB.txt", "r");

    for (int i = 0; i < totalAccounts; i++) {
        getline(&line, &len, DBFile);

        char* tempMessage = calloc(strlen(line), sizeof(char));
        strcpy(tempMessage, line);
        temp = createAccountFromLine(tempMessage);

        if(strncmp(temp->accountNumber, src->accountNumber, 5) == 0){
            src->balance = temp->balance;
	    fclose(DBFile);
            return src;
        }

    }
    /* Close both database files after edit is done, move temp to DB */
    fclose(DBFile);
    return NULL;
}
void printAccounts(struct account_t *head){
    if(head == NULL){
        printf("List is empty\n");
    }

    for(struct account_t* curr = head; curr != NULL; curr = curr->next){
        printf("Account Number: %s\n", head->accountNumber);
        printf("Pin: %s\n", head->pin);
        printf("Balance: %f\n", head->balance);
        printf("Return Address: %d\n", head->returnAddress);
        printf("Login Attempts: %d\n", head->loginAttempts);
    }
}

void killATM(struct account_t* head){
    for(struct account_t* curr = head; curr != NULL; curr = curr->next){
        kill(curr->returnAddress, SIGINT);
    }
}

void deleteMsgQueue(int msgid){
	if(msgctl(msgid, IPC_RMID, 0) == -1) {
		fprintf(stderr, "msgctl failed\n");
		exit(EXIT_FAILURE);
	}
}


