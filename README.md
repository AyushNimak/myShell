# myShell

Project by: Ayush Nimak
NetID: an831

---FILES---
mysh.c
    contains implementation for shell functionality
arraylist.c
    contains implementation for arraylist that is used by mysh.c
arraylist.h
    header file for arraylist.c containing all function prototypes
Makefile
    to efficiently generate object files and executables
Miscellaneous .txt files and directories
    used for testing and debugging of mysh.c

---RUNNING FILES---
Instruction "make" should automatically create and update executables for mysh

mysh works for the 4 different types of entries:

1. ./mysh                   -> enters interactive mode
2. ./mysh sample.sh         -> enters batch mode and receives input from sample.sh
3. [some output] | ./mysh   -> runs a "psuedo-batch" mode that runs mysh with that single line of input
4. ./mysh /tty              -> enters interactive mode as usual

** For the grader's reference, I've included all the files and directories I used for test cases 

    - directory "sampled" was used to check cd and pwd functionality
    - t1.txt to t5.txt were utilized for miscellaneaous tests (enumerated later in this document)

---TESTING PLAN---
My testing strategy began with figuring out how I wanted to implement the mysh in the first place. 
After determining so, I was able to test each individual part independently, and then combine the programs together.

These individual components include, but are not limited to: 
1. Ensuring that the shell enters with the correct mode given an input
2. Parsing each individual line correctly
3. Extracting each token of a line and loading it into an array list
4. Sending the command to the correct execution program
5. Forking and using dup2 appropriately

---TESTING STRATEGY AND CASES HANDLED---

Each component of the program was tested for edge cases and improbable scenarios.

My project was tested and was successful for all of the below test cases:

cd .. 

pwd

grep hello < t1.txt

grep hello < t1.txt > t6.txt

echo hello | grep goodbye

grep hello < t1.txt | grep hello.c

cd hello > t6.txt | grep hello

grep hello < t1.txt > t6.txt | grep hell < t2.txt > t3.txt

./mysh sample.sh

echo echo hello | ./mysh

ls *.txt
