#include "shared.h"

/* File references */
FILE *DBFile;
FILE *DBTemp;
char* DBFileStr = "./DB.txt";
char* DBTempStr = "./DB_temp.txt";
size_t len;
char *line;


int main(){
    int running = 1;
    pid_t pid;

    while(running){
        pid = fork();
        switch(pid) {
            /* If there is an error exit*/
            case -1:
                printf("Fork Failed\n");
                exit(-1);
                /* The child waits a minute then exits*/
            case 0:
                sleep(15);
                exit(0);
                /* Parent waits for child to exit then calculates interest */
            default:
                break;
        }
        wait(0);
        /* Open database files */
        DBFile = fopen(DBFileStr, "r");
        DBTemp = fopen(DBTempStr, "w");

        //tracking variables
        int totalAccounts = 0;
        char currchar;

        //Find the total number of lines that corresponds to the number of accounts in the db
        currchar = getc(DBFile);
        //Increments if there is a new line
        while (currchar != EOF) {
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

        for (int i = 0; i < totalAccounts + 1; i++) {
            //read the line
            getline(&line, &len, DBFile);

            //Malloc and store account line read in
            char *tempstring = malloc(sizeof(char) * strlen(line));
            strcpy(tempstring, line);
            struct account_t *account = createAccountFromLine(tempstring);

            if (account->balance < 0) {
                account->balance = account->balance - (account->balance * 0.02);
            } else {
                account->balance = account->balance + (account->balance * 0.01);
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
    }
}