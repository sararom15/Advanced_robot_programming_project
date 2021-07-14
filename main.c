#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

double ComputeToken(double receivedToken, float DT, double RF)
{
    if(receivedToken >= 1)
        receivedToken = -1;
    double newToken;
    newToken = receivedToken + DT * (1 - (receivedToken * receivedToken) / 2 )* (2 * M_PI * RF);
    printf("New Token:%f\n", newToken);
    return newToken;
}


// Returns the current time in microseconds.
long getTime(){
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

int main(int argc, char *argv[])
{
    // Load data from configuration file
    char *logfile;
    char *ip_address_prev;
    char *ip_address;
    char *ip_address_next; 
    const char *process_G_path;
    const char *FIRST_TOKEN;
    double RF;
    int portno;
    const char *savedP_PIDfile; 
    const char *savedG_PIDfile; 
    const char *waiting_time;

    logfile = argv[1];
    ip_address_prev = argv[2];
    ip_address = argv[3];
    ip_address_next = argv[4];
    RF = atof(argv[5]);
    process_G_path = argv[6];
    portno = atoi(argv[7]);
    FIRST_TOKEN = argv[8];
    savedP_PIDfile = argv[9]; 
    savedG_PIDfile = argv[10]; 
    waiting_time = argv[11];

    if (argc < 12)
    {
        fprintf(stderr, "Missing arguments\n");
        exit(-1);
    }

    // Variables declaration
    pid_t pid[3];
    pid_t pid_P;
    pid_t pid_L;
    long startTime, endTime;
    float DT;
    double newToken;
    const int BufferSize = 50;
    FILE *logname;
    char buffer_L[BufferSize], buffer_S[BufferSize];
    ssize_t TokenFromG, action, dumplog, comm_value;
    int first = 0; 

    //Select Variables declaration
    fd_set rfds_P, rfds_S, wfds_S;
    struct timeval tvP, tvL, tvS;
    int retval;
    int nfds;     

    // Socket variables declaration 
    int sockfd, newsockfd, clilen;
    char server_buf[256];
    struct sockaddr_in serv_addr, cli_addr;


    const int start = 1; // "start"
    const int stop = 2;  // "stop"
    const int log = 3;   // "dump log"

    // Pipes initialization
    int pipe_SP[2];
    int pipe_GP[2];
    int pipe_LP[2];

    if (pipe(pipe_SP) == -1)
    {
        perror("pipe_SP");
        exit(-1);
    }
    if (pipe(pipe_GP) == -1)
    {
        perror("pipe_GP");
        exit(-1);
    }
    if (pipe(pipe_LP) == -1)
    {
        perror("pipe_LP");
        exit(-1);
    }

    printf("Select:\n'1' -> to start the machine\n'2' -> to stop the machine\n'3' -> to dump the log file\n\n");

    // First fork
    pid[0] = fork();
    if (pid[0] < 0)
    {
        perror("First fork");
        return -1;
    }
    //*********Process P***********//
    if (pid[0] > 0)
    {
        int n;
        const int W = 1;
        printf("Process P starts wit pid: %d\n", getpid());
        pid_P = getpid();

        // Save pid_P on an external file
        FILE *pid_P_file = fopen(savedP_PIDfile, "w");
        if (pid_P_file == NULL)
        {
            perror("File opening");
            exit(-1);
        }
        if (fprintf(pid_P_file, "%i", pid_P) == -1)
        {
            perror("File writing");
            exit(-1);
        }
        fclose(pid_P_file);

        // loop
        while (1)
        {
            //Define buffer for P and for string to write in log file 
            char bufferP[BufferSize];
            char string_log[BufferSize * 2];

            // Initialization for select() 
            FD_ZERO(&rfds_P);
            FD_SET(pipe_GP[0], &rfds_P);
            FD_SET(pipe_SP[0], &rfds_P);

            close(pipe_SP[1]);
            close(pipe_GP[1]);

            // Check which file descriptor belonging to the set is bigger
            if (pipe_GP[0] > pipe_SP[0])
                nfds = pipe_GP[0] + 1;
            else
                nfds = pipe_SP[0] + 1;

            // Set the amount of time
            tvP.tv_sec = 0;
            tvP.tv_usec = 1000;

            //Execute only the first time: send the default token to G
            if (first == 0)
            {
                time_t taketime = time(NULL);
                char bufferUpdToken[BufferSize];

                // Create string for log file 
                int n = sprintf(string_log, "time: %s| G starts with default Token: %s\n", ctime(&taketime), FIRST_TOKEN);
                if (n == -1)
                {
                    perror("String creation");
                    exit(-1);
                }

                //----------SOCKET ----------//

                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0)
                {
                    perror("ERROR opening socket");
                    exit(-1);
                }
                setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &W, sizeof(W));
                fflush(stdout);
                bzero((char *)&serv_addr, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_addr.s_addr = INADDR_ANY;
                serv_addr.sin_port = htons(portno);

                if (bind(sockfd, (struct sockaddr *)&serv_addr,
                         sizeof(serv_addr)) < 0)
                {
                    perror("ERROR on binding");
                    exit(-1);
                }
                listen(sockfd, 5);
                clilen = sizeof(cli_addr);

                newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
                if (newsockfd < 0)
                {
                    perror("ERROR on accept");
                    exit(-1);
                }
                bzero(server_buf, 256);

                // WRITE THE DEFAULT TOKEN TO G 
                n = write(newsockfd, FIRST_TOKEN, sizeof(FIRST_TOKEN));
                if (n < 0)
                {
                    perror("ERROR writing to socket");
                    exit(-1);
                }
                taketime = time(NULL);
                // Complete the string (string_log) to be sent to L with the newToken line
                int m = sprintf(bufferUpdToken, "%s First Token: %s\n\n", ctime(&taketime), FIRST_TOKEN);
                if (m == -1)
                {
                    perror("String creation");
                    exit(-1);
                }
                int o = sprintf(string_log, "%s%s", string_log, bufferUpdToken);
                if (o == -1)
                {
                    perror("String creation");
                    exit(-1);
                }
                first = 1;
            }
            else
            {
                //-----------SELECT() of P: read from G and from S -------------//
                retval = select(nfds, &rfds_P, NULL, NULL, &tvP);
                if (retval == -1)
                {
                    perror("select()");
                    exit(-1);
                }
                else if (retval)
                {
                    // P reads pipe_GP 
                    if (FD_ISSET(pipe_GP[0], &rfds_P))
                    {
                        // Receive token via pipe_GP from process G 
                        TokenFromG = read(pipe_GP[0], bufferP, BufferSize);

                        if (TokenFromG == -1)
                        {
                            perror("No token from G\n");
                            exit(-1);
                        }
                        startTime = getTime();
                        bufferP[TokenFromG] = '\0';
                        char bufferUpdToken[BufferSize];


                        // Calculate timestamp
                        time_t taketime = time(NULL);

                        // Create string for log file 
                        int n = sprintf(string_log, "\ntime: %s G sent the Token: %s\n", ctime(&taketime), bufferP);
                        if (n == -1)
                        {
                            perror("String creation");
                            exit(-1);
                        }
                        // Send updated token via socket
                        sockfd = socket(AF_INET, SOCK_STREAM, 0);
                        if (sockfd < 0)
                        {
                            perror("ERROR opening socket");
                            exit(-1);
                        }
                        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &W, sizeof(W));
                        fflush(stdout);
                        bzero((char *)&serv_addr, sizeof(serv_addr));
                        serv_addr.sin_family = AF_INET;
                        serv_addr.sin_addr.s_addr = INADDR_ANY;
                        serv_addr.sin_port = htons(portno);
                        if (bind(sockfd, (struct sockaddr *)&serv_addr,
                                 sizeof(serv_addr)) < 0)
                        {
                            perror("ERROR on binding");
                            exit(-1);
                        }
                        listen(sockfd, 5);
                        clilen = sizeof(cli_addr);

                        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
                        if (newsockfd < 0)
                        {
                            perror("ERROR on accept");
                            exit(-1);
                        }
                        bzero(server_buf, 256);

                        // Calculate time 
                        endTime = getTime();

                        //printf("start:%li\nend:%li\n",startTime,endTime);
                        DT = (double)(endTime-startTime)/1000000;

                        // Calculate next Token
                        char tokenBuf[BufferSize];
                        newToken = ComputeToken(atof(bufferP), DT, RF);
                        sprintf(tokenBuf, "%f", newToken);

                        // Write to the socket the updated token
                        n = write(newsockfd, tokenBuf, BufferSize);
                        if (n < 0)
                        {
                            perror("ERROR writing to socket");
                            exit(-1);
                        }
                        //taketime = time(NULL);
                        // Complete the string (string_log) to be sent to L with the newToken line
                        int m = sprintf(bufferUpdToken, " Updated Token is %f", newToken);
                        if (m == -1)
                        {
                            perror("String creation");
                            exit(-1);
                        }

                        int p = sprintf(string_log, "%s%s\n\n", string_log, bufferUpdToken);
                        if (p == -1)
                        {
                            perror("String creation");
                            exit(-1);
                        }
                    }
                    // P reads pipe from S 
                    else if (FD_ISSET(pipe_SP[0], &rfds_P))
                    {
                        // Receive the action from process S
                        action = read(pipe_SP[0], bufferP, BufferSize);
                        bufferP[action] = '\0';
                        time_t taketime = time(NULL);

                        // Create string to be sent to L (logfile)
                        if (sprintf(string_log, "\ntime: %s New action from S: %s\n", ctime(&taketime), bufferP) == -1)
                        {
                            perror("String creation");
                            exit(-1);
                        }
                    }
                }
            }
            // send string for log file to L 
            write(pipe_LP[1], string_log, BufferSize*2);
        }
    }
    else if (pid[0] == 0)
    {
        // Second fork
        pid[1] = fork();
        if (pid[1] < 0)
        {
            perror("Second fork");
            exit(-1);
        }

        //*********** Process L************//
        if (pid[1] > 0)
        {

            printf("Process L starts wit pid: %d\n", getpid());
            pid_L = getpid();

            //loop
            while (1)
            {
                // Open logfile
                logname = fopen(logfile, "a");
                if (logname == NULL)
                {
                    perror("Cannot open log file");
                }
                // Read from the pipe the action
                dumplog = read(pipe_LP[0], buffer_L, BufferSize*2);
                
                if (dumplog == -1)
                {
                    perror("Read from pipe_LP");
                    exit(-1);
                }
                buffer_L[dumplog] = '\0';
                //printf("%s",buffer_L);

                // Write on log file
                if (fputs(buffer_L, logname) == EOF)
                {
                    perror("Logfile writing");
                    exit(-1);
                }
                fclose(logname);

                
            }
        }
        else if (pid[1] == 0)
        {
            // Third fork
            pid[2] = fork();
            if (pid[2] < 0)
            {
                perror("Third fork");
                exit(-1);
            }
            // *********Process G***********//
            if (pid[2] > 0)
            {
                printf("Process G starts wit pid: %d\n", getpid());
                // Save pid_G on an external file
                FILE *pid_G_file = fopen(savedG_PIDfile, "w");
                if (pid_G_file == NULL)
                {
                    perror("File opening");
                    exit(-1);
                }
                if (fprintf(pid_G_file, "%i", getpid()) == -1)
                {
                    perror("File writing");
                    exit(-1);
                }
                fclose(pid_G_file);

                char *num_port;
                char pipeGP_0[5];
                char pipeGP_1[5];

                // Prepare the variables to be given to the G process
                if (sprintf(pipeGP_0, "%i", pipe_GP[0]) < 0)
                {
                    perror("String creation");
                    exit(-1);
                }
                if (sprintf(pipeGP_1, "%i", pipe_GP[1]) < 0)
                {
                    perror("String creation");
                    exit(-1);
                }

                num_port = argv[7];

                // exec G process passing the necessary arguments
                if (execl(process_G_path, ip_address, num_port, pipeGP_0, pipeGP_1,waiting_time, NULL) == -1)
                {
                    perror("Exec failed");
                    exit(-1);
                }
            }
            //********* Process S**********//
            else
            {
                printf("Process S starts wit pid: %d\n", getpid());

                // Get process P pid
                char strP[5];
                int pid_P_;
                FILE *pid_P_f = fopen(savedP_PIDfile, "r");
                if (pid_P_f == NULL)
                {
                    perror("file opening");
                    exit(-1);
                }
                while (fgets(strP, 10, pid_P_f) == NULL)
                {
                }
                pid_P_ = atoi(strP);
                // Get process G pid
                char strG[5];
                int pid_G_;
                FILE *pid_G_f = fopen(savedG_PIDfile, "r");
                if (pid_G_f == NULL)
                {
                    perror("file opening");
                    exit(-1);
                }
                while (fgets(strG, 10, pid_G_f) == NULL)
                {
                }
                pid_G_ = atoi(strG);

                // Start the program with the stop signal 
                kill(pid_P_, SIGSTOP);
                kill(pid_G_, SIGSTOP);
                printf("The program is stopped.\n");

                //loop
                while (1)
                {
                    // Initialization select() 
                    FD_ZERO(&rfds_S);
                    FD_ZERO(&wfds_S);
                    FD_SET(STDIN_FILENO, &rfds_S);
                    FD_SET(pipe_SP[1], &wfds_S);

                    close(pipe_SP[0]);

                    tvS.tv_sec = 0;
                    tvS.tv_usec = 1000;
                    int ret = select(pipe_SP[1] + 1, &rfds_S, &wfds_S, NULL, NULL);
                    if (ret == -1)
                    {
                        perror("select()");
                        exit(-1);
                    }
                    else if (ret)
                    {
                        // S reads the command from console 
                        if (FD_ISSET(STDIN_FILENO, &rfds_S))
                        {
                            comm_value = read(STDIN_FILENO, buffer_S, BufferSize);
                            if (comm_value == -1)
                            {
                                perror("Read()");
                                exit(-1);
                            }
                            buffer_S[comm_value] = '\0';


                            // Command is START
                            if (atoi(buffer_S) == start)
                            {
                                // start process P
                                kill(pid_P_, SIGCONT);
                                // start process G
                                kill(pid_G_, SIGCONT);

                                printf("Start the machine\n");

                                // Send the action to P
                                if (FD_ISSET(pipe_SP[1], &wfds_S))
                                    write(pipe_SP[1], buffer_S, BufferSize);
                            }

                            //Command is STOP
                            else if (atoi(buffer_S) == stop)
                            {
                                // Send to P the action performed
                                if (FD_ISSET(pipe_SP[1], &wfds_S))
                                    write(pipe_SP[1], buffer_S, BufferSize);

                                // stop process P
                                kill(pid_P_, SIGSTOP);
                                // stop process G
                                kill(pid_G_, SIGSTOP);

                                printf("Stop the machine\n");
                            }
                            // Command is the dump log 
                            else if (atoi(buffer_S) == log)
                            {

                                int dump_size = BufferSize * 20;
                                // Send to P the action performed
                                if (FD_ISSET(pipe_SP[1], &wfds_S))
                                    write(pipe_SP[1], buffer_S, BufferSize);

                                logname = fopen(logfile, "r");
                                if (logname == NULL)
                                {
                                    printf("There is not Log File yet!!!! \n");
                                }
                                else
                                {
                                    fseek(logname, 0, SEEK_END);
                                    int size = ftell(logname);
                                    fseek(logname, size - dump_size, SEEK_SET);
                                    if (size >= dump_size)
                                    {
                                        char *buffer_log = malloc(dump_size + 1);
                                        // read  the file and put it in buffer_Log
                                        if (fread(buffer_log, 1, dump_size, logname) != dump_size)
                                        {
                                            perror("Reading the file");
                                            exit(-1);
                                        }
                                        fclose(logname);

                                        if (write(STDOUT_FILENO, buffer_log, dump_size) == -1)
                                        {
                                            perror("write() log on stdout");
                                            exit(-1);
                                        }
                                        fflush(stdout);
                                    }
                                    else
                                    {
                                        printf("The log file is empty.\n");
                                    }
                                }
                            }
                            else
                            {
                                printf("Select 1,2 or 3\n");
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}


