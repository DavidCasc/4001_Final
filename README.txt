This system was not tested on linux lab machine. Built and tested on RPi and Ubuntu
System Summary
DB_Server
DB_Editor
ATM
interestCalculator

C Files:
 -DB_Server.c
 -DB_Editor.c
 -ATM.c
 -interestCalculator.c

Executable Files:
 -DB_Server
 -DB_Editor
 -ATM
 -interestCalculator

Misc. Files:
 -DB.txt
 -deadlocklogs.txt

NOTE: Ensure Executable file names match list above

SETUP
1) Convert makefile.txt to makefile
2) run make

START UP
 1) Open a minimum of two terminal windows
 2) Type the following in commmand line:
	"./DB_Server"
    This will start up the DB_Server, DB_Editor, and interestCalculator.
 3) In a separate window, type the following to start the ATM:
	"./ATM"
 4) Repeat step 3 for the desired amount of ATM's

HOW TO LOGIN
 1) Type 5 digit account number and press enter (i.e. 00117)
 2) Type 3 digit account pin and press enter (i.e. 260)
 3) If pin is wrong, enter new pin. If returned to the account number field, account doesn't exist

HOW TO GET ACCOUNT BALANCE
 1) Once logged in type "B" and press enter (Menu entries are case sensitive)
 2) Account Balance should be displayed on screen

HOW TO WITHDRAW MONEY
 1) Once logged in type "W" and press enter (Menu entries are case sensitive)
 2) Enter amount to withdraw
 3) Money will either be withdrawn or NSF message will be displayed

HOW TO DEPOSIT MONEY
 1) Once logged in type "D" and press enter (Menu entries are case sensitive)
 2) Enter amount to deposit
 3) Deposit message will be displayed on screen

HOW TO APPLY FOR A LOAN
 1) Once logged in type "L" and press enter (Menu entries are case sensitive)
 2) Enter amount of loan
 3) Loan message will be displayed on screen

HOW TO ENTER NEW ACCOUNT INTO DATABASE
 1) Navigate to the window that is running the DB_Server
 2) Enter 5 digit account number (i.e. 12345)
 3) Enter 3 digit account pin (i.e. 678) 
 4) Enter desired balance
 5) The account should reflect in the database

HOW TO GENERATE DEADLOCK
 1) Wait 60 seconds for the interestCalculator to obtain lock on the database text file
 2) Within 30 seconds of step one, finish loan application at ATM
 3) If steps 1 and 2 are done correctly, the DB_Server should be process the request during the 30 seconds
 4) Deadlock creates, check logs.
