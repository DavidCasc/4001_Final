#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/msg.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include <math.h>
#include <unistd.h>

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

int toDB_key = 1234;
int toATM_key = 4321;

/**
 * Assumes message is in the format:
 *  [AccountNum][PIN][Balance]
 *  [ADR][AccountNum][PIN][Balance]
 * @param message
 * @return
 */
struct account_t* createAccount(char* message){

    struct account_t* acc = malloc(sizeof(struct account_t));
    acc->accountNumber = malloc(sizeof(char)*5);
    acc->pin = malloc(sizeof(char)*3);
    strncpy(acc->accountNumber, message, 5);
    strncpy(acc->pin, message + 6, 3);
    acc->balance = atof(message+10);
    acc->returnAddress = 2; //This will change later
    acc->loginAttempts = 0;
    acc->next = NULL;
    return acc;

}
struct account_t* createLoginAccount(char*message){
    struct account_t* acc = malloc(sizeof(struct account_t));
    acc->accountNumber = malloc(sizeof(char)*5);
    acc->pin = malloc(sizeof(char)*3);
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

    struct account_t* acc = malloc(sizeof(struct account_t));
    acc->accountNumber = malloc(sizeof(char)*5);
    acc->pin = malloc(sizeof(char)*3);
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
    char* str = malloc(sizeof(char) * (digits + 3));

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
    char * accStr = malloc(sizeof(char) * (strlen(acc->pin) + strlen(acc->accountNumber) + strlen(balance) + 2));
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
    for(curr = head; curr->next != NULL; curr=curr->next){
        if(curr->returnAddress == returnAddress){
            /* If the node is the head */
            if(curr == head){
                head = curr->next;
            }
            /* If the node in the middle */
            else if(curr->next != NULL) {
                prev->next = curr->next;
                curr->next = NULL;
            }
            /* If the node is at the end */
            else {
                prev->next = NULL;
                curr->next = NULL;
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



