# Advanced_robot_programming_project
## Description 
The goal is to simulate a network of multi-process systems, all identical, each one running on a machine in the same LAN, connected through sockets, exchanging a data token at a specified speed. 

It is implemented on a single machine. An artificial delay is used to mimic the network communication delays. 

The structure of software is composed by: 
1) the main.c file: it contains the main code; 
2) the G.c file: it contains the code of the G process; 
3) the script.sh file: a script for compiling and executing the software; 
4) the ConfigurationFile.cfg: the configuration file containing all arguments needed to run the main code.

### MAIN.C file: 
It is the main code of the architecture. The needed arguments to run it are defined in the configuration file. Here, the P,S,G,L processes have been created. The PID values of P and G processes are saved into an external txt file in order to be read by S process when they are needed.  The user decides to start, stop or output the contents of the log file, typing on the console "1","2" or "3". The process S receives console messages and acts using Posix signals in order to satisfy the request. In addition it sends the signal to P process through non-deterministic pipes. The process P receives tokens from G, computes and sends the updated token to G via Socket connection; therefore it is connected to S (as already stated), for reading the signals, and to G (for reading the old token) through non-deterministic pipes. Here the Posix select is used. In addition, the token values and the signals are stored in a buffer, which is sent to L process through non-deterministic pipe as well. The process L reads the token values and the console signals received from P and stores them in a txt file. 

### G.c file: 
It is the code of G process. It is the client of the socket: it receives the updated token from P. Once the token has been received, the process sends it to P process through a non-deterministic pipe. 

### script.sh file: 
It is a script which should be executed to run the software. First of all, it sources the configuration file and get it. After that, it compiles the codes and executes them. 

### ConfigurationFile.cfg file: 
It contains all arguments needed for the main code. They are: 
1) the path for the log file; 
2) the IP address of the previous machine; 
3) the IP address of the current internet connection (IPv4); 
4) the IP address of the next machine; 
5) the Reference frequency; 
6) the path of the executable G process; 
7) the port number (>80); 
8) the first token (by default it is -1); 
9) the path for the txt file where the P pid is written; 
10)the path for the txt file where the G pid is written;
11)the waiting time. 


## GUIDE TO RUN THE SOTWARE
Before run the software, adjust the configuration file, in order to define your own IP address and the paths. 
After that, open the terminal, change directory and move into the software folder. 
Run "./script.sh". 
