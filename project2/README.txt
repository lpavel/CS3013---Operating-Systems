Name: Laurentiu Pavel
wpi username: lpavel@wpi.edu
In order to create the modules and executables, simply run "make".

The modules created are openclose.ko and intercept.ko

If inserted, openclose.ko will intercept the system calls "open" and "close". Every time when one of this calls are made, the system catches them and prints to "syslog" accordingly to part1 of the assignment. You can see the output of "dmesg" on my machine after inserting the module in the text file "opencloseoutput.txt"

intercept.ko is the module that intercepts the 2nd system call added in the preproject. If inserted, whenever the call is being made, information about the process will be printed. It uses the processinfo structure (why is it not typedefed?) provided by the instructur in order to store the data got from the very low level task_struct structure. Basically it goes on the task_struct through the children using the list api and goes through all the nodes before and after the current one to find the processes that are needed.

The c files test.c and testnull.c are made to test the 2nd part of the assignment. The first one creates a process with a child with a child inside it. The 2nd one is almost identic except for the youngest child that is passing a NULL pointer that will be handled by the system call (it creates an empty structure that doesn't cause a segfault). Also, you can see the output of test and testnull on my machine in the files "testoutput.txt" and "testnulloutput.txt"

