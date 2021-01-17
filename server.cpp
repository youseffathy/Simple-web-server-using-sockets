#include <iostream>
#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fstream>
#include <algorithm>
#include <fcntl.h>
#define bufferSize 5000

using namespace std;

// counter used to trigger the number of connections alive
// and dynamically adjust the time to live for connection
int connectionCounters = 0;


/***************** error handler ********************/
void error(const char *msg)
{
    perror(msg);
    exit(1);
}
/****************************************************/

/**************** Parser class **********************/
class Parser
{
public:
    Parser() {}
    void getFilePath(char* buffer, char* filePath, int from)
    {
        int i = 0;
        while(buffer[from + i] != '\r')
        {
            filePath[i] = buffer[from + i];
            i++;
        }
    }
};
/****************************************************/

/************ read and write file class *************/
class FileReaderAndWriter
{
public:
    size_t readFile(string filePath, char* file)
    {
        ifstream fin(filePath, std::ios::binary | ios::in);
        if(!fin.good())
            return -1;
        fin.seekg(0, std::ios::end);
        size_t filesize = (int)fin.tellg();
        fin.seekg(0);
        if(fin.read((char*)file, filesize))
        {
            fin.close();
            return filesize;
        }
        return -1;;
    }
    bool writeFile(string filePath, char* file, size_t fileSize)
    {
        std::ofstream fout(filePath, std::ios::binary | ios::out);
        if(!fout.good())
            return false;
        fout.write((char*)file, fileSize);
        fout.close();
        return true;
    }
};
/****************************************************/

/************* GET request handler ******************/
void getRequestHandler(int socketfd, char*  buffer)
{
    FileReaderAndWriter frw;
    char filePath[40];
    bzero(filePath, 40);
    Parser parser;
    //file path
    parser.getFilePath(buffer, filePath, 4);
    bzero(buffer, bufferSize);
    size_t fileSize = frw.readFile(string(filePath), buffer);
    if(fileSize == -1) // file not exist or error occured
    {
        string resString= "HTTP/1.1 404 Not Found\r\n\r\n";
        strcpy(buffer, resString.c_str());
        write(socketfd, buffer, 30);
        cout << "server: HTTP/1.1 404 Not Found" << endl;
    }
    else
    {
        string temp = "HTTP/1.1 200 OK\r\n\r\n";
        //send status of response
        write(socketfd, temp.c_str(), 30);
        //send size of file
        write(socketfd,(void *)&fileSize, sizeof(size_t));
        //send file
        write(socketfd, buffer, fileSize);
        cout << "server: HTTP/1.1 200 OK" << endl;
    }
}
/****************************************************/

/***************** POST request handler *************/
void postRequestHandler(int socketfd, char* buffer)
{
    //receive size of data
    size_t fileSize;
    read(socketfd, &fileSize, sizeof(fileSize));
    char filePath[40];
    bzero(filePath, 40);
    Parser parser;
    //file path
    parser.getFilePath(buffer, filePath, 5);
    bzero(buffer, bufferSize);
    //receive data
    read(socketfd, buffer, fileSize);
    FileReaderAndWriter frw;
    if(frw.writeFile(string(filePath), buffer, fileSize))
    {
        bzero(buffer, bufferSize);
        string temp = "HTTP/1.1 200 OK\r\n\r\n";
        strcpy(buffer, temp.c_str());
        write(socketfd, buffer, 20);
        cout << "server: HTTP/1.1 200 OK" << endl;
    }
}
/****************************************************/

/**************** Connection handler ****************/

//connection handler
void connectionHandler(int socketfd)
{
    clock_t start;
    double duration;
    start = clock();
    while((clock() - start) / (double) CLOCKS_PER_SEC < 1.0 / (double) (10 * connectionCounters))
    {
        char* buffer= new char[bufferSize];
        bzero(buffer, bufferSize);
        //receive request
        if(read(socketfd, buffer, bufferSize) == bufferSize)
        {
            start = clock();
            cout << "client:" << string(buffer) << endl;
            // show method
            if( buffer[0] == 'G')
            {

                getRequestHandler(socketfd, buffer);
            }
            else if(buffer[0] == 'P')
            {
                postRequestHandler(socketfd, buffer);
            }
        }
        delete[] buffer;
    }
    //terminating socket connection
    close(socketfd);
}
/****************************************************/

int main(int argc, char *argv[])
{
    int portNo ;
    int sockfd, newsockfd, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    //check for port number if any. Otherwise, it will be 80 as default
    if(argc  < 2)
    {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    //specify port number of the server
    portNo = atoi(argv[1]);

    //create socket
    sockfd =  socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    // clear address structure
    bzero((char *) &serv_addr, sizeof(serv_addr));

    // specify parameters of socket
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // local IP address
    serv_addr.sin_port = htons(portNo);


    //bind socket file descriptor with the parameters

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");


    //listen to connections
    if (listen(sockfd, 20) < 0)
    {
        error("cannot listen on port\n");

    }
    cout << "server: Start listing to connections on port: " << portNo << endl;
    cout << "---------------------------------------------------------" << endl;


    while(1)
    {

        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr, &clilen);
        connectionCounters++;
        //set socket to NON-BOLOCKING mode
        fcntl(newsockfd, F_SETFL, O_NONBLOCK);
        printf("server: got connection from %s port %d\n",inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        int pid = fork();
        if(pid == 0)
        {
            connectionHandler(newsockfd);
            cout << "server: connection from " << inet_ntoa(cli_addr.sin_addr) << " port " << ntohs(cli_addr.sin_port) << " closed" << endl;
            connectionCounters--;
            return 0; // killing child process
        }

        close(newsockfd);
    }


    return 0;
}
