all: DB_Server DB_Editor ATM interestCalculator

DB_Server: DB_Server.c shared.h
	gcc DB_Server.c -o DB_Server -lm
DB_Editor: DB_Editor.c
	gcc DB_Editor.c -o DB_Editor -lm
ATM: ATM.c
	gcc ATM.c -o ATM -lm
interestCalculator: interestCalculator.c
	gcc interestCalculator.c -o interestCalculator -lm
