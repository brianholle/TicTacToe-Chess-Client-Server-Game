/* Program Name: asgn9server-team1.c
Author: Brian Holle
Date: Dec.10, 2015
File name: chess_serverv2.c
Compile: cc -lpthread chess_serverv2.c
Run: ./a.out
Description: This program is a simulation of a tic-tac-toe game
where player 1 (X) and player 2(O) alternate between turns to
choose coordinates of a tic tac toe board. The players connect to
this server using client processes, which have the socket information
needed to connect to this. The players alternate turns using the
layout below and Using (Row, Column) as input. This Version handlehandles
multiple users playing games at the same time on the server.

COLUMN
0 1 2
0|___|___|___|
ROW 1|___|___|___|
2|___|___|___|

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>
#include <pthread.h>

// the hostname of the HTTP server
#define HOST "server1.cs.scranton.edu"
// the HTTP port client will be connecting to
#define HTTPPORT "32100"
// how many pending connections queue will hold
#define BACKLOG 10
#define ROWS 3
#define COLS 3
#define CHESSROWS 8
#define CHESSCOLS 8
#define TRUE 1
#define FALSE 0
typedef char Board[ROWS][COLS];
typedef char ChessBoard[CHESSROWS][CHESSCOLS];
typedef char playerPieceLoc[CHESSROWS][CHESSCOLS];

struct TURN{
    int row;
    int column;
};

struct CHESSTURN{
    int pieceRow;
    int pieceColumn;
    int row;
    int column;
};

struct CONTEXT{
    int socketFdPlayer1;
    int socketFdPlayer2;
    struct CONNECTIONS *Connections;
};

struct CONNECTIONS{
    int con_fd[256];
    int loginStatus[256];//0 = not-logged-in 1 = logged-in
    char userName[256][256];
    int playWaiting[256];
    int invites[256];
    int chessWaiting[256];
};

struct LOGIN{
    int pfd;
    char pName[80];
    char pPass[80];
    struct CONNECTIONS *Connections;
};

struct HIGHSCORE {
    char playerName[80];
    int playerWins;
    int playerLosses;
    int playerTies;
    int playerRank;
};

struct TEMP {
    char playerName[80];
    int playerWins;
    int playerLosses;
    int playerRank;
};

struct WLT {
    int wins;
    int losses;
    int ties;
};


void *get_in_addr(struct sockaddr * sa); // get internet address
int get_server_socket(char *hostname, char *port); // get a server socket
int start_server(int serv_socket, int backlog); // start server's listening
// accept a connection from client
int accept_client(int serv_sock);
void start_subserver(int reply_sock_fd1, int reply_sock_fd2, struct CONNECTIONS
*Connections);// start subserver as a thread
void *subserver(void *reply_sock_fd_ptr); // subserver - subserver
int isWinner(Board board); // isWinner
int newMoveHandler(int clientTurn, int clientWaiting, Board board,
int turnCount); // handles new valid moves
struct TURN getTurn(struct TURN Turn, Board board, int clientTurn,
int errorCode); //gets the turn from player
//checks if move is valid
int isValidMove(struct TURN Turn, Board board, int clientTurn);
//compared login name/password to record in logins.bin
int validLogin(char *pName, char *pPass);
//used to see if login Name exists in logins.bin
int playerExistLogin(char *pName);
int checkForFiles(); //used to check if "logins.bin" exists
//starts login subserver
void start_loginsubserver(int reply_sock_fd, struct CONNECTIONS *Connections);
//login subserver responsible for user input and login
void *loginsubserver(void * loginVoidPtr);
//update new connection files
void updateConnectionFd(int temp_sock_fd, struct CONNECTIONS *Connections );
//starts find match subserver
void start_findmatchsubserver(struct CONNECTIONS *Connections);
// subserver in charge for pairing matches
void *findmatchsubserver(void * connectionsVoidPtr);
// handles the input from user
void handleInput(int reply_sock_fd, struct LOGIN *loginPtr);
int getMaxRank(); //determines max amounts of wins
void updateLoserScore(char *loser);//update loser score
void updateWinnerScore(char *winner); // updates winners score
void updateTieScore(char *player);// updates players score in case of tie
void sortScores(); //assigns rank to each player
int playerExistHighScore(char *player); //scan to see if player has a high score
struct WLT getRecord(char *pName); // record that returns a record of a given pname
void printHighScore(); //prints the highscore board on the server side after each completed game.
void badEncrypt(char *s); // a simple encryption algorithm for passwords
void getPName(int reply_sock_fd, struct CONTEXT *contextPtr, char *pName);
void clearPlayerConC( int reply_sock_fd, struct CONTEXT *contextPtr);
void clearPlayerConL( int reply_sock_fd, struct LOGIN *loginPtr);
void sendHighScore(int reply_sock_fd);
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;


//Chess stuff!
void start_chesssubserver(int reply_sock_fd1, int reply_sock_fd2, struct CONNECTIONS *Connections);
void *chesssubserver(void * contextVoidPtr);
int isValidChess(struct CHESSTURN ChessTurn, ChessBoard Chessboard, ChessBoard pWaitingLoc, char pieceCur, int player);
void updateBoardChess(struct CHESSTURN ChessTurn, ChessBoard Chessboard, playerPieceLoc pTurnLoc, playerPieceLoc pWaitingLoc);
void printBoardChess(ChessBoard Chessboard, playerPieceLoc p2Loc);
int isWinnerChess(playerPieceLoc p1Loc, playerPieceLoc p2Loc);
struct CHESSTURN getTurnChess(ChessBoard Chessboard, playerPieceLoc p2Loc, int clientTurn, int errorCode);


/*--------------------------------------------------*
| The main() function controls the overall structure |
| of the server. |
*--------------------------------------------------*/

int main(void){
    struct CONNECTIONS Connections;
    int http_sock_fd; // http server socket
    int temp_sock_fd = -1; // client 1 connection
    int loopCount = 255;
    // steps 1-2: get a socket and bind to ip address and port
    http_sock_fd = get_server_socket(HOST, HTTPPORT);
    
    while(loopCount > -1){
        Connections.con_fd[loopCount] = -1;
        Connections.loginStatus[loopCount] = -1;
        Connections.playWaiting[loopCount] = -1;
        Connections.invites[loopCount] = -1;
        Connections.chessWaiting[loopCount] = -1;
        loopCount--;
    }
    
    // step 3: get ready to accept connection #1
    if (start_server(http_sock_fd, BACKLOG) == -1) {
        printf("start server error\n");
        exit(1);
    }
    if (start_server(http_sock_fd, BACKLOG) != -1) {
        printf("server started\n");
    }
    start_findmatchsubserver(&Connections);
    while(1){
        //printf("Top of Main Loop!\n");
        temp_sock_fd = accept_client(http_sock_fd);
        //printf("Middle of loop!\n");
        if(temp_sock_fd != -1){
            updateConnectionFd(temp_sock_fd, &Connections);
            start_loginsubserver(temp_sock_fd, &Connections);
        }
        temp_sock_fd = -1;
    }
}

/*---------------------------------------------------------------*
| updateConnectionfd() is a function that updates the connections|
| struct to add the new fd. |
*---------------------------------------------------------------*/

void updateConnectionFd(int temp_sock_fd, struct CONNECTIONS *Connections ){
    int loopCount = 0;
    printf("Temp_sock_fd = %d \n", temp_sock_fd);
    printf("Checking against:\n");
    while (Connections->con_fd[loopCount] != -1){
        printf(" connection #%d = %d \n",loopCount,
        Connections->con_fd[loopCount]);
        loopCount++;
    }
    Connections->con_fd[loopCount] = temp_sock_fd;
}


/*-----------------------------------------------------*
| The accept_client() function is used when waiting for |
| a new client to connect. It then returns a file |
| descriptor to be used when referencing connections. |
*-----------------------------------------------------*/

int accept_client(int serv_sock) {
    int reply_sock_fd = -1;
    socklen_t sin_size = sizeof(struct sockaddr_storage);
    struct sockaddr_storage client_addr;
    char client_printable_addr[INET6_ADDRSTRLEN];
    
    if ((reply_sock_fd = accept(serv_sock,
    (struct sockaddr *)&client_addr, &sin_size)) == -1) {
        printf("socket accept error\n");
    }
    else {
        // Extra information about client connection.
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr),
        client_printable_addr, sizeof client_printable_addr);
        printf("server: connection from %s at port %d\n", client_printable_addr,
        ((struct sockaddr_in*)&client_addr)->sin_port);
        
    }
    return reply_sock_fd;
}

/*-------------------------------------------------------------*
| The start_subserver() function accepts two file desciptors |
| for the client connections and starts a sub-server containing |
| a game for them to play. |
*-------------------------------------------------------------*/

void start_subserver(int reply_sock_fd1, int reply_sock_fd2, struct CONNECTIONS *Connections) {
    pthread_t pthread;
    struct CONTEXT *contextPtr;
    contextPtr = (struct CONTEXT *)malloc(sizeof(struct CONTEXT));
    contextPtr->socketFdPlayer1 = reply_sock_fd1;
    contextPtr->socketFdPlayer2 = reply_sock_fd2;
    contextPtr->Connections = Connections;
    if (pthread_create(&pthread, NULL, subserver, (void*)contextPtr) != 0) {
        printf("failed to start subserver\n");
        
    }
    else {
        printf("Game-subserver %ld started\n", (unsigned long)pthread);
    }
}

/*-------------------------------------------*
| start_loginsubserver() starts tictactoe. |
*-------------------------------------------*/

void start_loginsubserver(int reply_sock_fd, struct CONNECTIONS *Connections){
    pthread_t pthread;
    struct LOGIN *loginPtr;
    loginPtr = (struct LOGIN *)malloc(sizeof(struct LOGIN));
    loginPtr->pfd = reply_sock_fd;
    loginPtr->Connections = Connections;
    if (pthread_create(&pthread, NULL, loginsubserver, (void*)loginPtr) != 0) {
        printf("failed to start subserver\n");
    }
    else {
        printf("Login-subserver %ld started\n", (unsigned long)pthread);
    }
}

/*---------------------------------------------------*
| start_findmatchsubserver() starts findmatch
*---------------------------------------------------*/

void start_findmatchsubserver(struct CONNECTIONS *Connections){
    pthread_t pthread;
    struct CONNECTIONS *connectionsPtr;
    connectionsPtr = (struct CONNECTIONS *)malloc(sizeof(struct CONNECTIONS));
    connectionsPtr = Connections;
    if (pthread_create(&pthread, NULL, findmatchsubserver, (void*)connectionsPtr) != 0) {
        printf("failed to start subserver\n");
    }
    else {
        printf("findmatch-subserver %ld started\n", (unsigned long)pthread);
    }
    free(Connections);
}

/*---------------------------------------------------*
| start_chesssubserver() starts chess game |
*---------------------------------------------------*/

void start_chesssubserver(int reply_sock_fd1, int reply_sock_fd2, struct CONNECTIONS *Connections) {
    pthread_t pthread;
    struct CONTEXT *contextPtr;
    contextPtr = (struct CONTEXT *)malloc(sizeof(struct CONTEXT));
    contextPtr->socketFdPlayer1 = reply_sock_fd1;
    contextPtr->socketFdPlayer2 = reply_sock_fd2;
    contextPtr->Connections = Connections;
    if (pthread_create(&pthread, NULL, chesssubserver, (void*)contextPtr) != 0) {
        printf("failed to start subserver\n");
        
    }
    else {
        printf("Game-subserver %ld started\n", (unsigned long)pthread);
    }
}

/*--------------------------------------------------------*
| chesssubserver() starts a gess game between clients |
*--------------------------------------------------------*/

void *chesssubserver(void * contextVoidPtr){
    struct CONTEXT *contextPtr = (struct CONTEXT *) contextVoidPtr;
    struct CHESSTURN ChessTurn; //struct containing the rows and columns of the players input
    int turnCount = 0;//the number of turns
    int winner = FALSE;
    char p1Name[80];
    char p2Name[80];
    int doneSearch = FALSE;
    int loopCount = 0;
    int playerWinner = -1;
    
    int pieceRow;
    int pieceColumn;
    int row;
    int column;
    int i = 0;
    int winningPlayer;
    char space = ' ';
    char pieceDestination;
    char pieceCur;
    int validResult;
    int player = 0;
    int currentPlayerfd;
    int errorCode = 0;
    
    ChessBoard ChessBoard = {
        {'R','K','B','Q','+','B','K','R'},
        {'P','P','P','P','P','P','P','P'},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {'P','P','P','P','P','P','P','P'},
{'R','K','B','Q','+','B','K','R'}};
    
    playerPieceLoc p1Loc = {
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {'P','P','P','P','P','P','P','P'},
{'R','K','B','Q','+','B','K','R'}};
    
    playerPieceLoc p2Loc = {
        {'R','K','B','Q','+','B','K','R'},
        {'P','P','P','P','P','P','P','P'},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
        {' ',' ',' ',' ',' ',' ',' ',' '},
{' ',' ',' ',' ',' ',' ',' ',' '}};
    int reply_sock_fd1 = contextPtr->socketFdPlayer1;
    int reply_sock_fd2 = contextPtr->socketFdPlayer2;
    
    getPName(reply_sock_fd1, *&contextPtr, p1Name);
    getPName(reply_sock_fd2, *&contextPtr, p2Name);
    
    //loops through Connections to find matching fd and copies pname
    send(reply_sock_fd1, &p2Name, sizeof(p2Name),0);
    send(reply_sock_fd2, &p1Name, sizeof(p1Name),0);
    
    while(!winner){
        player = (i%2)+ 1;
        errorCode = 0;
        //getTurn from FD and save into ChessTurn.row and .Column
        if(player == 1){
            currentPlayerfd = reply_sock_fd1;
            ChessTurn = getTurnChess(ChessBoard, p2Loc, currentPlayerfd, errorCode);
            pieceDestination = p1Loc[ChessTurn.row][ChessTurn.column];
            pieceCur = p1Loc[ChessTurn.pieceRow][ChessTurn.pieceColumn];
            }else if( player == 2){
            currentPlayerfd = reply_sock_fd2;
            ChessTurn = getTurnChess(ChessBoard, p2Loc, currentPlayerfd, errorCode);
            pieceDestination = p2Loc[ChessTurn.row][ChessTurn.column];
            pieceCur = p2Loc[ChessTurn.pieceRow][ChessTurn.pieceColumn];
        }
        printf("pieceDestination: %c\n", pieceDestination);
        printf("pieceCurrent: %c\n", pieceCur);
        
        validResult = isValidChess(ChessTurn, ChessBoard, (player ==1)?p2Loc:p1Loc, pieceCur, player);
        errorCode = validResult;
        while(validResult == FALSE || strncmp(&pieceDestination, " ", 1) != 0 || (ChessTurn.row<0 || ChessTurn.row>7 || ChessTurn.column<0 || ChessTurn.column>7) || (strncmp(&pieceCur, " ", 1) == 0)){
            ChessTurn = getTurnChess(ChessBoard, p2Loc, currentPlayerfd, errorCode);
            if(player == 1){
                pieceDestination = p1Loc[ChessTurn.row][ChessTurn.column];
                pieceCur = p1Loc[ChessTurn.pieceRow][ChessTurn.pieceColumn];
                }else if( player == 2){
                pieceDestination = p2Loc[ChessTurn.row][ChessTurn.column];
                pieceCur = p2Loc[ChessTurn.pieceRow][ChessTurn.pieceColumn];
            }
            printf("pieceDestination: %c\n", pieceDestination);
            printf("pieceCurrent: %c\n", pieceCur);
            validResult = isValidChess(ChessTurn, ChessBoard, (player ==1)?p2Loc:p1Loc, pieceCur, player);
        }
        //once a valid move has been entered, update the ChessBoard, check for winner and end if winner exists.
        errorCode = validResult;
        updateBoardChess(ChessTurn, ChessBoard, (player ==1)?p1Loc:p2Loc, (player ==2)?p1Loc:p2Loc);
        winningPlayer = isWinnerChess(p1Loc, p2Loc);
        if(winningPlayer != 0){
            winner = TRUE;
            printf("The winner is %s!\n",(winningPlayer ==1)?p1Name:p2Name);
            errorCode = 2;//2 = winner
            send((winningPlayer ==1)?reply_sock_fd1:reply_sock_fd2, &errorCode, sizeof(errorCode), 0);
            send((winningPlayer ==1)?reply_sock_fd1:reply_sock_fd2, ChessBoard, sizeof(ChessBoard), 0);
            send((winningPlayer ==1)?reply_sock_fd1:reply_sock_fd2, p2Loc, sizeof(playerPieceLoc), 0);
            errorCode = 3;//3 = loser
            send((winningPlayer ==1)?reply_sock_fd2:reply_sock_fd1, &errorCode, sizeof(errorCode), 0);
            send((winningPlayer ==1)?reply_sock_fd2:reply_sock_fd1, ChessBoard, sizeof(ChessBoard), 0);
            send((winningPlayer ==1)?reply_sock_fd2:reply_sock_fd1, p2Loc, sizeof(playerPieceLoc), 0);
            } else {
            send(currentPlayerfd, &errorCode, sizeof(errorCode), 0);
            send(currentPlayerfd, ChessBoard, sizeof(ChessBoard), 0);
            send(currentPlayerfd, p2Loc, sizeof(playerPieceLoc), 0);
        }
        i++;
    }
    
    return NULL;
}

/*----------------------------------------------------------*
| The isValidChess() function is rather lengthy, but crucial to|
| to the program. It receives the main board and the opponent|
| board and determines if the move is valid. It handles each |
| pieces allowed behaviour. |
*----------------------------------------------------------*/

int isValidChess(struct CHESSTURN ChessTurn, ChessBoard Chessboard, ChessBoard pWaitingLoc, char pieceCur, int player){
    int result = FALSE;
    int done = FALSE;
    int rowDif;
    int colDif;
    int temp = 0;
    printf("Is Valid piece current: %c\n", pieceCur);
    
    rowDif = abs(ChessTurn.row - ChessTurn.pieceRow);
    colDif = abs(ChessTurn.column - ChessTurn.pieceColumn);
    
    if(strncmp(&pieceCur, "P", 1) == 0){
        if((rowDif == 1 && (colDif == 1 || colDif == 0)) && (ChessTurn.row = ChessTurn.pieceRow +((player ==1)?-1:1))){
            if (colDif == 0 && strncmp(&pWaitingLoc[ChessTurn.row][ChessTurn.column], " ", 1) !=0){
                }else if(ChessTurn.pieceColumn != ChessTurn.column){
                if(strncmp(&pWaitingLoc[ChessTurn.row][ChessTurn.column], " ", 1) != 0 ){
                    result = TRUE;
                }
                }else{
                result = TRUE;
            }
        }
    }
    if(strncmp(&pieceCur, "R", 1) == 0){
        if((rowDif != 0 && colDif == 0) || (rowDif == 0 && colDif != 0)){
            if(rowDif != 0){
                if(ChessTurn.row > ChessTurn.pieceRow && ChessTurn.column == ChessTurn.pieceColumn){
                    while(rowDif >= 1 && done == FALSE){
                        if( rowDif == 1){
                            result = TRUE;
                            done = TRUE;
                            } else if(strncmp(&Chessboard[ChessTurn.pieceRow + (rowDif-1)][ChessTurn.pieceColumn], " ", 1) != 0){
                            done = TRUE;
                            }else{
                            rowDif --;
                        }
                    }
                    }else if (ChessTurn.row < ChessTurn.pieceRow && ChessTurn.column == ChessTurn.pieceColumn){
                    while(rowDif >= 1 && done == FALSE){
                        if( rowDif == 1){
                            result = TRUE;
                            done = TRUE;
                            } else if(strncmp(&Chessboard[ChessTurn.pieceRow - (rowDif -1)][ChessTurn.pieceColumn], " ", 1) != 0){
                            done = TRUE;
                            }else{
                            rowDif --;
                        }
                    }
                    
                }
                //
                } else if (colDif != 0) {
                if(ChessTurn.column > ChessTurn.pieceColumn && ChessTurn.row == ChessTurn.pieceRow){
                    while(colDif >= 1 && done == FALSE){
                        if( colDif == 1){
                            result = TRUE;
                            done = TRUE;
                            } else if(strncmp(&Chessboard[ChessTurn.pieceRow][ChessTurn.pieceColumn + (colDif -1)], " ", 1) != 0){
                            done = TRUE;
                            }else{
                            colDif --;
                        }
                    }
                    }else if (ChessTurn.column < ChessTurn.pieceColumn && ChessTurn.row == ChessTurn.pieceRow){
                    while(colDif >= 1 && done == FALSE){
                        if( colDif == 1){
                            result = TRUE;
                            done = TRUE;
                            } else if(strncmp(&Chessboard[ChessTurn.pieceRow][ChessTurn.pieceColumn - (colDif -1)], " ", 1) != 0){
                            done = TRUE;
                            }else{
                            colDif --;
                        }
                    }
                }
            }
        }
    }
    if(strncmp(&pieceCur, "K", 1) == 0){
        if((rowDif == 2 && colDif == 1) || (rowDif == 1 && colDif == 2)){
            result = TRUE;
        }
        
    }
    if(strncmp(&pieceCur, "B", 1) == 0){
        if(rowDif == colDif){
            printf("B has right format!\n");
            if(ChessTurn.row > ChessTurn.pieceRow && ChessTurn.column > ChessTurn.pieceColumn){
                printf("case1\n");
                while(rowDif >= 1 && done == FALSE){
                    if( rowDif == 1){
                        result = TRUE;
                        done = TRUE;
                        } else if(strncmp(&Chessboard[ChessTurn.pieceRow + (rowDif-1)][ChessTurn.pieceColumn + (rowDif-1)], " ", 1) != 0){
                        done = TRUE;
                        }else{
                        rowDif --;
                    }
                }
                }else if(ChessTurn.row > ChessTurn.pieceRow && ChessTurn.column < ChessTurn.pieceColumn){
                printf("case2\n");
                while(rowDif >= 1 && done == FALSE){
                    if( rowDif == 1){
                        result = TRUE;
                        done = TRUE;
                        } else if(strncmp(&Chessboard[ChessTurn.pieceRow + (rowDif-1)][ChessTurn.pieceColumn - (rowDif-1)], " ", 1) != 0){
                        done = TRUE;
                        }else{
                        rowDif --;
                    }
                }
                }else if(ChessTurn.row < ChessTurn.pieceRow && ChessTurn.column > ChessTurn.pieceColumn){
                printf("case3\n");
                while(rowDif >= 1 && done == FALSE){
                    if( rowDif == 1){
                        result = TRUE;
                        done = TRUE;
                        } else if(strncmp(&Chessboard[ChessTurn.pieceRow - (rowDif-1)][ChessTurn.pieceColumn + (rowDif-1)], " ", 1) != 0){
                        done = TRUE;
                        }else{
                        rowDif --;
                    }
                }
                }else if(ChessTurn.row < ChessTurn.pieceRow && ChessTurn.column < ChessTurn.pieceColumn){
                printf("case4\n");
                printf("Checking Chessboard[%d][%d] = %c\n",ChessTurn.pieceRow - (rowDif-1), ChessTurn.pieceColumn - (rowDif-1), Chessboard[ChessTurn.pieceRow + (rowDif-1)][ChessTurn.pieceColumn - (rowDif-1)]);
                while(rowDif >= 1 && done == FALSE){
                    if( rowDif == 1){
                        result = TRUE;
                        done = TRUE;
                        } else if(strncmp(&Chessboard[ChessTurn.pieceRow - (rowDif-1)][ChessTurn.pieceColumn - (rowDif-1)], " ", 1) != 0){
                        done = TRUE;
                        }else{
                        rowDif --;
                    }
                }
            }
            
        }
    }
    if(strncmp(&pieceCur, "Q", 1) == 0){
        result = isValidChess(ChessTurn, Chessboard, pWaitingLoc, 'R', player);
        if (result == FALSE){
            result = isValidChess(ChessTurn, Chessboard, pWaitingLoc, 'B', player);
        }
        
    }
    if(strncmp(&pieceCur, "+", 1) == 0){
        if((rowDif == 1 || rowDif == 0) && (colDif == 1 || colDif == 0)){
            result = TRUE;
        }
    }
    printf("Result: %d\n", result);
    return result;
}

/*-------------------------------------------------------*
| The updateBoardChess() function is only called once a valid |
| move has been entered. If the move is valid, the main |
| board will update. In the event that a piece is taken, |
| this function will also remove that piece from the |
| opponents board. |
*-------------------------------------------------------*/

void updateBoardChess(struct CHESSTURN ChessTurn, ChessBoard Chessboard, playerPieceLoc pTurnLoc, playerPieceLoc pWaitingLoc){
    
    if(strncmp(&pWaitingLoc[ChessTurn.row][ChessTurn.column], " ", 1) != 0){
        pWaitingLoc[ChessTurn.row][ChessTurn.column] = ' ';
    }
    Chessboard[ChessTurn.row][ChessTurn.column] = pTurnLoc[ChessTurn.pieceRow][ChessTurn.pieceColumn];
    pTurnLoc[ChessTurn.row][ChessTurn.column] = Chessboard[ChessTurn.row][ChessTurn.column];
    pTurnLoc[ChessTurn.pieceRow][ChessTurn.pieceColumn] = ' ';
    Chessboard[ChessTurn.pieceRow][ChessTurn.pieceColumn] = ' ';
}

/*--------------------------------------------------*
| The printBoardChess() function is in charge of printing |
| the player board. It also determines which pieces |
| are to be colored green to distinguish players. |
*--------------------------------------------------*/

void printBoardChess(ChessBoard Chessboard, playerPieceLoc p2Loc){
    int outerLoop = 0;
    int innerLoop = 0;
    
    
    printf("\n");
    printf(" 0 1 2 3 4 5 6 7 \n");
    while(outerLoop < 8){
        printf(" +---+---+---+---+---+---+---+---+\n");
        printf("%d ", outerLoop);
        while(innerLoop < 8 ){
            if(strncmp(&Chessboard[outerLoop][innerLoop], &p2Loc[outerLoop][innerLoop], 1) == 0){
                printf("| 33[22;32m%c 33[0m", Chessboard[outerLoop][innerLoop]);
                } else {
                printf("| %c ", Chessboard[outerLoop][innerLoop]);
            }
            innerLoop++;
        }
        innerLoop = 0;
        printf("|\n");
        outerLoop++;
    }
    printf(" +---+---+---+---+---+---+---+---+\n");
    
}

/*--------------------------------------------------------*
| Function to check the two players board for the existence|
| of the "+" king piece. If it does not exist, the player |
| has lost the game. |
*--------------------------------------------------------*/

int isWinnerChess(playerPieceLoc p1Loc, playerPieceLoc p2Loc){
    int outerLoop = 0;
    int innerLoop = 0;
    int p1Alive = 0;
    int p2Alive = 0;
    int result;
    
    while(outerLoop < 8){
        while(innerLoop < 8 ){
            if(strncmp(&p1Loc[outerLoop][innerLoop], "+", 1) == 0){
                p1Alive = TRUE;
                }else if(strncmp(&p2Loc[outerLoop][innerLoop], "+", 1) == 0){
                p2Alive = TRUE;
            }
            innerLoop++;
        }
        innerLoop = 0;
        outerLoop++;
    }
    
    if (p1Alive && p2Alive){
        result = 0;
        } else if (p1Alive){
        result = 1;
        } else if (p2Alive){
        result = 2;
    }
    return result;
}

/*-------------------------------------------------*
| The getTurnChess() function is used to get the players |
| next turn and then store it into the Turn struct |
*-------------------------------------------------*/

struct CHESSTURN getTurnChess(ChessBoard Chessboard, playerPieceLoc p2Loc, int clientTurn, int errorCode){
    struct CHESSTURN ChessTurn;
    send(clientTurn, &errorCode, sizeof(errorCode), 0);
    send(clientTurn, Chessboard, sizeof(ChessBoard), 0);
    send(clientTurn, p2Loc, sizeof(playerPieceLoc), 0);
    recv(clientTurn, &ChessTurn, sizeof(ChessTurn), 0);
    fprintf(stdout, "ChessTurn:n Row %dn Column %dn\n", ChessTurn.row, ChessTurn.column);
    return ChessTurn;
}






















/*---------------------------------------------------------------*
| findmatchsubserver() is incahrge of finding two clients in queue|
| to put into match together. |
*---------------------------------------------------------------*/

void *findmatchsubserver(void * connectionsVoidPtr){
    struct CONNECTIONS *connectionsPtr = (struct CONNECTIONS *) connectionsVoidPtr;
    int loopCount = 0;
    int loopCount2 = 0;
    int tempTic = -1;
    int tempChess = -1;
    int doneSearch = FALSE;
    int reply_sock_fd1 = -1;//used for normal queue
    int reply_sock_fd2 = -1;//
    int reply_sock_fd3 = -1;//used for invite games
    int reply_sock_fd4 = -1;//
    int reply_sock_fd5 = -1;//used for chess games
    int reply_sock_fd6 = -1;//
    int invite;
    
    while(1){
        loopCount = 0;
        doneSearch = FALSE;
        while(loopCount < 256 && doneSearch == FALSE){
            
            if(loopCount == tempTic || loopCount == tempChess){//temps are used to be referenced when match is found. It resets waiting value.
                
                }else if(connectionsPtr->playWaiting[loopCount] == 1){//handles standard waiting queue for tic-tac-toe
                if(reply_sock_fd1 == -1){
                    reply_sock_fd1 = connectionsPtr->con_fd[loopCount];
                    tempTic = loopCount; // code is set to 0 but reinitialized every call to findMatch
                }
                else if (reply_sock_fd2 == -1){
                    reply_sock_fd2 = connectionsPtr->con_fd[loopCount];
                    connectionsPtr->playWaiting[loopCount] = -1;
                }
                }else if(connectionsPtr->chessWaiting[loopCount] == 1){//handles standard waiting queue for chess
                if(reply_sock_fd5 == -1){
                    reply_sock_fd5 = connectionsPtr->con_fd[loopCount];
                    tempChess = loopCount; // code is set to 0 but reinitialized every call to findMatch
                }
                else if (reply_sock_fd2 == -1){
                    reply_sock_fd6 = connectionsPtr->con_fd[loopCount];
                    connectionsPtr->chessWaiting[loopCount] = -1;
                }
                }else if(connectionsPtr->invites[loopCount] != -1){//handles invites for tic-tac-toe
                reply_sock_fd3 = connectionsPtr->con_fd[loopCount];
                invite = connectionsPtr->invites[loopCount];
                while(connectionsPtr->con_fd[loopCount2] != invite){
                    loopCount2++;
                }
                if(connectionsPtr->invites[loopCount2] == reply_sock_fd3){
                    reply_sock_fd4 = connectionsPtr->con_fd[loopCount2];
                    start_subserver(reply_sock_fd3, reply_sock_fd4, *&connectionsPtr);
                    connectionsPtr->invites[loopCount] = -1;
                    connectionsPtr->invites[loopCount2] = -1;
                }
                reply_sock_fd3 = -1;
                reply_sock_fd4 = -1;
                loopCount2 = 0;
            }
            
            if(reply_sock_fd1 != -1 && reply_sock_fd2 != -1){//starts queued tic tac toe game
                start_subserver(reply_sock_fd1, reply_sock_fd2, *&connectionsPtr);
                connectionsPtr->playWaiting[tempTic] = -1;
                doneSearch = TRUE;
                reply_sock_fd1 = -1;
                reply_sock_fd2 = -1;
                tempTic = -1;
                } else if (reply_sock_fd5 != -1 && reply_sock_fd6 != -1){//starts queued chess game
                start_chesssubserver(reply_sock_fd5, reply_sock_fd6, *&connectionsPtr);
                connectionsPtr->playWaiting[tempChess] = -1;
                doneSearch = TRUE;
                reply_sock_fd5 = -1;
                reply_sock_fd6 = -1;
                tempChess = -1;
                }else{
                loopCount++;
            }
        }
    }
    free(connectionsPtr);
    return NULL;
}

/*-----------------------------------------------------------------*
| loginsubserver() is in charge of handeling user requests and login|
*-----------------------------------------------------------------*/

void *loginsubserver(void * loginVoidPtr){
    struct LOGIN *loginPtr = (struct LOGIN *) loginVoidPtr;
    char pName[80];
    char pPass[80];
    int done = FALSE;
    int errorCode = 0; // 0 = invalid 1 = successful 2 == created new
    int reply_sock_fd = loginPtr->pfd;
    int loopCount = 0;
    
    while(done == FALSE){
        recv(reply_sock_fd, pName, sizeof(pName), 0);
        recv(reply_sock_fd, pPass, sizeof(pPass), 0);
        printf("Username: %snPassword: %s\n", pName, pPass);
        badEncrypt(pPass);
        printf("Encrypted Password: %s\n", pPass);
        errorCode = validLogin(pName, pPass);
        if ( errorCode == 1 || errorCode == 2){
            done = TRUE;
            //find which player to modify loginStatus and userName
            while(loginPtr->Connections->con_fd[loopCount] != reply_sock_fd){
                loopCount++;
            }
            printf("Connection #%d\n", loopCount);
            printf("successful login\n");
            loginPtr->Connections->loginStatus[loopCount] = 1;
            strcpy(loginPtr->Connections->userName[loopCount], pName);
        }
        printf("Sending loginCode: %d\n",errorCode);
        send(reply_sock_fd, &errorCode, sizeof(errorCode), 0);
        if ( errorCode == 1 || errorCode == 2 ){
            handleInput(reply_sock_fd, *&loginPtr);
            
        }
    }
    free(loginPtr);
    return NULL;
}
/*------------------------------------------------------------*
| The subserver() function accepts the new client pair pointer |
| and proceeds to handle the communication between the server |
| and the clients. This is where the game play takes place. |
*------------------------------------------------------------*/

void *subserver(void * contextVoidPtr) {
    struct CONTEXT *contextPtr = (struct CONTEXT *) contextVoidPtr;
    struct WLT WltP1;
    struct WLT WltP2;
    struct TURN Turn; //struct containing the rows and columns of the players input
    int turnCount = 0;//the number of turns
    int errorCode = 0;//refer to top for code meanings
    int winner = FALSE;
    char p1Name[80];
    char p2Name[80];
    int skipCode = -1;
    int dontSkipCode = 0;
    int doneSearch = FALSE;
    int loopCount = 0;
    int playerWinner = -1;
    int winsLossesTiesP1[3];
    int winsLossesTiesP2[3];
    int k = 0;
    
    Board board = {
          {'-','-','-'},{'-','-','-'},{'-','-','-'}};
    
    int reply_sock_fd1 = contextPtr->socketFdPlayer1;
    int reply_sock_fd2 = contextPtr->socketFdPlayer2;
    
    getPName(reply_sock_fd1, *&contextPtr, p1Name);
    getPName(reply_sock_fd2, *&contextPtr, p2Name);
    
    //loops through Connections to find matching fd and copies pname
    send(reply_sock_fd1, &p2Name, sizeof(p2Name),0);
    send(reply_sock_fd2, &p1Name, sizeof(p1Name),0);
    send(reply_sock_fd1, &skipCode, sizeof(skipCode),0);
    send(reply_sock_fd2, &dontSkipCode, sizeof(dontSkipCode),0);
    
    pthread_mutex_lock(&mutex2);
    WltP1 = getRecord(p1Name);
    pthread_mutex_unlock(&mutex2);
    
    winsLossesTiesP1[0] = WltP1.wins;
    winsLossesTiesP1[1] = WltP1.losses;
    winsLossesTiesP1[2] = WltP1.ties;
    
    pthread_mutex_lock(&mutex2);
    WltP2 = getRecord(p2Name);
    pthread_mutex_unlock(&mutex2);
    
    winsLossesTiesP2[0] = WltP2.wins;
    winsLossesTiesP2[1] = WltP2.losses;
    winsLossesTiesP2[2] = WltP2.ties;
    
    send(reply_sock_fd1, &winsLossesTiesP1, sizeof(winsLossesTiesP1), 0);
    send(reply_sock_fd1, &winsLossesTiesP2, sizeof(winsLossesTiesP2), 0);
    
    send(reply_sock_fd2, &WltP2, sizeof(WltP2), 0);
    send(reply_sock_fd2, &WltP1, sizeof(WltP1), 0);
    
    while (!winner && turnCount<9){
        int player = (turnCount%2)+ 1;
        errorCode = 0;
        
        //Sends Error Code and Board. Then receives turn.
        printf("%s ", (player ==1)?p1Name:p2Name);
        Turn = getTurn(Turn, board,(player ==1)?reply_sock_fd1:reply_sock_fd2, errorCode);
        
        //Checks turn to make sure move is valid. If not valid, errorCode = 1.
        errorCode = isValidMove(Turn, board, (player ==1)?reply_sock_fd1:reply_sock_fd2);
        
        //If the move is valid. Add turn to board and handle move.
        if(errorCode == 0){
            board[Turn.row][Turn.column] = (player ==1)?'X':'O';
            //This section handles the conditions of a win, loss and tie.
            errorCode = newMoveHandler((player ==1)?reply_sock_fd1:reply_sock_fd2,
            (player ==2)?reply_sock_fd1:reply_sock_fd2, board, turnCount);
            if( errorCode != 4 ){
                winner = TRUE;
                playerWinner = player;
            }
            turnCount++;
        }
    }
    if (errorCode == 6){
        pthread_mutex_lock(&mutex2);
        updateTieScore(p1Name);
        updateTieScore(p2Name);
        pthread_mutex_unlock(&mutex2);
    }
    else {
        
        pthread_mutex_lock(&mutex2);
        updateWinnerScore((playerWinner == 1)?p1Name:p2Name);
        updateLoserScore((playerWinner == 1)?p2Name:p1Name);
        pthread_mutex_unlock(&mutex2);
    }
    pthread_mutex_lock(&mutex2);
    sortScores();
    pthread_mutex_unlock(&mutex2);
    
    printf("%s is the winner\n", (winner ==1)?p1Name:p2Name);
    printf("Game has ended.\n");
    
    //Sets player connections empty
    clearPlayerConC(reply_sock_fd1, *&contextPtr);
    clearPlayerConC(reply_sock_fd2, *&contextPtr);
    
    printHighScore();
    close(reply_sock_fd1);
    close(reply_sock_fd2);
    free(contextPtr);
    pthread_mutex_destroy(&mutex2);
    return NULL;
}

/*-------------------------------------------------*
| The getTurn() function is used to get the players |
| next turn and then store it into the Turn struct |
*-------------------------------------------------*/

struct TURN getTurn(struct TURN Turn, Board board, int clientTurn, int errorCode){
    send(clientTurn, &errorCode, sizeof(errorCode), 0);
    send(clientTurn, board, sizeof(Board), 0);
    recv(clientTurn, &Turn, sizeof(Turn), 0);
    fprintf(stdout, "Turn:n Row %dn Column %dn\n", Turn.row, Turn.column);
    return Turn;
    
}

/*----------------------------------------------------*
| The isValidMove() function is used to check if the |
| players turn is in fact valid. It checks for boundry |
| issues and already filled spot issues. |
*----------------------------------------------------*/

int isValidMove(struct TURN Turn, Board board, int clientTurn){
    int errorCode = 0;
    
    if(((board[Turn.row][Turn.column] == 'X') || (board[Turn.row][Turn.column] == 'O'))
    || (Turn.row<0 || Turn.row>2 || Turn.column<0 || Turn.column>2)){
        errorCode = 1;
        send(clientTurn, &errorCode, sizeof(errorCode), 0);
    }
    return errorCode;
}

/*-----------------------------------------------------------*
| The newMoveHandler() function handles all user moves. If a |
| move is valid, it is then sent to this function to determine|
| if that move infact has caused a winner or tie. If neither |
| of these occur, then the function returns the "4" code. This|
| simply means that the move was placed and it is the next |
| players turn. |
*-----------------------------------------------------------*/

int newMoveHandler(int clientTurn, int clientWaiting, Board board, int turnCount){
    struct TURN Turn;
    int errorCode = 0;
    int winner = FALSE;
    
    winner = isWinner(board);
    
    //If there is a winner, tell both clients, display the board
    //and end game loop.
    if (winner == TRUE){
        errorCode = 2;
        send(clientTurn, &errorCode, sizeof(errorCode), 0);
        send(clientTurn, board, sizeof(Board), 0);
        errorCode = 5;
        send(clientWaiting, &errorCode, sizeof(errorCode), 0);
        send(clientWaiting, board, sizeof(Board), 0);
    }
    //If there is a tie on the board, tell both clients,
    //send them the updated board and end game loop.
    else if(!winner && turnCount==8){
        errorCode = 3;
        send(clientTurn, &errorCode, sizeof(errorCode), 0);
        send(clientTurn, board, sizeof(Board), 0);
        errorCode = 6;
        send(clientWaiting, &errorCode, sizeof(errorCode), 0);
        send(clientWaiting, board, sizeof(Board), 0);
    }
    //If there is no win or tie, the board is then returned to the user
    //so they can see the updated board. Then they wait for a new error
    //code stating it is their turn again.
    else{
        errorCode = 4;
        send(clientTurn, &errorCode, sizeof(errorCode), 0);
        send(clientTurn, board, sizeof(Board), 0);
    }
    return errorCode;
}

/*-------------------------------------------*
| The isWinner() function checks the board |
| and determines if either player is a winner |
*-------------------------------------------*/

int isWinner(Board board){
    int k = 0, retVal = 0;
    
    //CHECKS DIAGONAL 1
    if((board[0][2] == board[1][1]) && (board[1][1] == board[2][0])
    && board[0][2] != '-' && board[1][1] != '-' && board[2][0] != '-'){
        retVal = 1;
    }
    //CHECKS DIAGONAL 2
    else if((board[0][0] == board[1][1]) && (board[1][1] == board[2][2])
    && board[0][0] != '-' && board[1][1] != '-' && board[2][2] != '-'){
        retVal = 1;
    }
    else{
        for(k = 0; k < 3; k ++){
            //CHECKS VERTICALS
            if((board[0][k] == board[1][k]) && (board[1][k] == board[2][k])
            && board[0][k] != '-' && board[1][k] != '-' && board[2][k] != '-'){
                retVal = 1;
            }
            //CHECKS HORIZONTALS
            else if((board[k][0] == board[k][1]) && (board[k][1] == board[k][2])
            && board[k][0] != '-' && board[k][1] != '-' && board[k][2] != '-'){
                retVal = 1;
            }
        }
    }
    //Return 1 if one of the winning coniditons is met.
    return retVal;
}

/*-------------------------------------------------*
| The validLogin() function is used to check if the |
| user attempting to log in has used a correct user |
| login Name and Password. If the user has the wrong|
| password they will not be allowed to play the |
| game. New users can make a new account simply by |
| writing a new username / password combo! |
*-------------------------------------------------*/

int validLogin(char *pName, char *pPass){
    struct LOGIN Login;
    int doneSearch = FALSE;
    int loginsfd = 0;
    int num = 0;
    int loopCount = 0;
    int result = 0;
    
    printf("Searching logins.bin for %s\n", pName);
    //if Player exists, find it and check if the password is correct
    pthread_mutex_lock(&mutex1);
    if (playerExistLogin(pName)){
        loginsfd = open("logins.bin", O_RDWR|O_CREAT, S_IRWXU);
        num = read(loginsfd, &Login, sizeof(Login));
        while(doneSearch == FALSE){
            if(strcmp(Login.pName, pName) == 0){
                doneSearch = TRUE;
                if(strcmp(Login.pPass,pPass) == 0){
                    result = 1;
                }
            }
            else{
                loopCount++;
                num = read(loginsfd, &Login, sizeof(Login));
            }
        }
        close(loginsfd);
        //if the player does not exist, create a new user
    }
    else{
        loginsfd = open("logins.bin", O_RDWR|O_CREAT|O_APPEND, S_IRWXU);
        strcpy(Login.pName, pName);
        strcpy(Login.pPass, pPass);
        num = write(loginsfd, &Login, sizeof(Login));
        close(loginsfd);
        result = 2;
        printf("New User Created\n");
    }
    pthread_mutex_unlock(&mutex1);
    pthread_mutex_destroy(&mutex1);
    return result;
}
/*
//
//
// Below are helpful Functions that allow for the server/client to connect.
//
//
*/

void *get_in_addr(struct sockaddr * sa) {
    if (sa->sa_family == AF_INET) {
        printf("Player Connected using Ipv4\n");
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    else {
        printf("Plaer Connected using Ipv6\n");
        return &(((struct sockaddr_in6 *)sa)->sin6_addr);
    }
}

//Gets host server socket id for the main to use
int get_server_socket(char *hostname, char *port) {
    struct addrinfo hints, *servinfo, *p;
    int status;
    int server_socket;
    int yes = 1;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }
    
    for (p = servinfo; p != NULL; p = p ->ai_next) {
        // step 1: create a socket
        if ((server_socket = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
            printf("socket socket \n");
            continue;
        }
        // if the port is not released yet, reuse it.
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            printf("socket option\n");
            continue;
        }
        
        // step 2: bind socket to an IP addr and port
        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            printf("socket bind \n");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo); // servinfo structure is no longer needed. free it.
    
    return server_socket;
}


//Starts the server using the server_socket.
int start_server(int serv_socket, int backlog) {
    int status = 0;
    if ((status = listen(serv_socket, backlog)) == -1) {
        printf("socket listen error\n");
    }
    return status;
}

/*-----------------------------------------------*
| The playerExistLogin() function is used to check if a|
| given login name exists in the logins.bin file. |
*-----------------------------------------------*/

int playerExistLogin(char *pName){
    struct LOGIN Login;
    struct stat statBuffer;
    int doneSearch = FALSE;
    int loginsfd = 0, num = 0, fileLeft = 0, loopCount = 0;
    int BUFFER_SIZE = 1024;
    
    loginsfd = open("logins.bin", O_RDONLY);
    fileLeft = fstat(loginsfd, &statBuffer);
    close(loginsfd);
    fileLeft = statBuffer.st_size;
    
    if(checkForFiles()){
        loginsfd = open("logins.bin", O_RDWR);
        num = read(loginsfd, &Login, sizeof(Login));
        while(fileLeft != 0 && doneSearch == FALSE){
            printf("Login.pName = %s Login.pPass = %s | Searching for %s\n",
            Login.pName, Login.pPass, pName);
            if(strcmp(Login.pName, pName) == 0){
                doneSearch = TRUE;
            }
            else{
                fileLeft = fileLeft - sizeof(Login);
                num = read(loginsfd, &Login, sizeof(Login));
            }
        }
        close(loginsfd);
    }
    return doneSearch;
}

/*------------------------------------------------------*
| playerExistHighScore() is a function that will check to|
| see if the a given player already exists in the high |
| score file. |
*------------------------------------------------------*/

int playerExistHighScore(char *player){
    struct HIGHSCORE HighScore;
    struct stat statBuffer;
    int doneSearch = FALSE;
    int highScorefd = 0, num = 0, fileLeft = 0, loopCount = 0;
    int BUFFER_SIZE = 1024;
    
    highScorefd = open("highScore.bin", O_RDONLY);
    fileLeft = fstat(highScorefd, &statBuffer);
    close(highScorefd);
    fileLeft = statBuffer.st_size;
    
    if(checkForFiles()){
        highScorefd = open("highScore.bin", O_RDWR);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        while(fileLeft != 0 && doneSearch == FALSE){
            if(strcmp(HighScore.playerName, player) == 0){
                doneSearch = TRUE;
            }
            else{
                fileLeft = fileLeft - sizeof(HighScore);
                num = read(highScorefd, &HighScore, sizeof(HighScore));
            }
        }
        close(highScorefd);
    }
    return doneSearch;
}

/*--------------------------------------------*
| The checkForFiles() function currently only |
| check to see if the logins file exists. Later|
| it may check for other files such as a score |
| board. |
*--------------------------------------------*/

int checkForFiles(){
    struct LOGIN Login;
    int loginsfd = 0, num = 0;
    int i = 0, empty = -1, loopCount = 0;
    int result = 0;
    
    if (access( "logins.bin", F_OK) != -1){
        result = 1;
    }
    if (access( "highScore.bin", F_OK) != -1){
        //printf("Loaded High Score File\n");
        result = 1;
    }
    return result;
}

/*-----------------------------------------------------*
| handleInput() is a function whose purpose is to handle|
| the input of a user and act accordingly. |
*-----------------------------------------------------*/

void handleInput(int reply_sock_fd, struct LOGIN *loginPtr){
    char input[2];
    char pName[80];
    int temp_sock_fd;
    int done = FALSE;
    int playersOnline = 0;
    int loopCount = 0;
    int loopCount2 = 0;
    int hsDone;
    
    while(done == FALSE){
        recv(reply_sock_fd, &input, sizeof(input), 0);
        printf("User input = %c\n", input[0]);
        if(input[0] == 'P' ){
            printf("User input = 'P'\n");
            while(loginPtr->Connections->con_fd[loopCount] != reply_sock_fd){
                loopCount++;
            }
            loginPtr->Connections->playWaiting[loopCount] = 1;
            done = TRUE;
        }
        else if ( input[0] == 'C'){
            printf("User input == 'C'\n");
            while(loginPtr->Connections->con_fd[loopCount] != reply_sock_fd){
                loopCount++;
            }
            loginPtr->Connections->chessWaiting[loopCount] = 1;
            done = TRUE;
        }
        else if ( input[0] == 'H'){
            printf("User input = 'H'\n");
            if (access( "highScore.bin", F_OK) != -1){
                sendHighScore(reply_sock_fd);
                } else {
                hsDone = TRUE;
                send(reply_sock_fd, &hsDone, sizeof(hsDone),0);
            }
        }
        else if ( input[0] == 'L'){
            printf("User input = 'L'\n");
            while (loginPtr->Connections->loginStatus[playersOnline] != -1){
                playersOnline++;
            }
            send(reply_sock_fd, &playersOnline, sizeof(playersOnline),0);
            if(playersOnline > 1){
                while(playersOnline > 0){
                    strcpy(pName, loginPtr->Connections->userName[playersOnline-1]);
                    send(reply_sock_fd, &pName, sizeof(pName), 0);
                    sleep(1);
                    playersOnline--;
                }
            }
            playersOnline = 0;
        }
        else if ( input[0] == 'A'){
            while(loginPtr->Connections->con_fd[loopCount] != reply_sock_fd){
                loopCount++;
            }
            temp_sock_fd = loginPtr->Connections->invites[loopCount];
            while(loginPtr->Connections->con_fd[loopCount2] != temp_sock_fd){
                loopCount2++;
            }
            strcpy(pName, loginPtr->Connections->userName[loopCount2]);
            printf("Sending invitation from: %s\n", pName);
            send(reply_sock_fd, &pName, sizeof(pName),0);
            recv(reply_sock_fd, &input, sizeof(input),0);
            if (input[0] == 'Y'){
                loginPtr->Connections->invites[loopCount2] = reply_sock_fd;
                done = TRUE;
                }else if(input[0] =='N'){
                loginPtr->Connections->invites[loopCount] = -1;
            }
        }
        else if (input[0] == 'S'){
            recv(reply_sock_fd, &pName, sizeof(pName), 0);
            printf("sending invite to: %s\n",pName);
            while(loginPtr->Connections->loginStatus[loopCount] != -1){
                if (strcmp(loginPtr->Connections->userName[loopCount], pName) == 0){
                    if (loginPtr->Connections->invites[loopCount] == -1){
                        printf("set invite value\n");
                        loginPtr->Connections->invites[loopCount] = reply_sock_fd;
                        printf("username: %s\n",loginPtr->Connections->userName[loopCount]);
                        printf("inviteValue = %d\n",loginPtr->Connections->invites[loopCount]);
                    }
                }
                loopCount++;
            }
            loopCount = 0;
            done = TRUE;
        }
        else if ( input[0] == 'Q'){
            printf("User input = 'Q'\n");
            done = TRUE;
            clearPlayerConL(reply_sock_fd, *&loginPtr);
        }
        else if (input[0] == '?'){
            printf("User input = '?'\n");
        }
        else {
            printf("User entered invalid command\n");
        }
    }
}


/*---------------------------------------------------------*\
| sortScores() is a function used to assign high scores with|
| a rank based on how many people they have more wins than. |
| These ranks are used to display the high scores in order  |
| of most wins by user. Will eventually be updated to rank  |
| players based on win/loss ratio. Currently hangs up on    |
| wins/0.                                                   |
\*---------------------------------------------------------*/

void sortScores(){
    struct HIGHSCORE HighScore;
    struct stat statBuffer;
    struct TEMP Temp;
    int highScorefd = 0, num = 0, fileLeft = 0, fileSize = 0, loopCount = 0, rank = 0;
    int BUFFER_SIZE = 1024;
    int temp;
    
    highScorefd = open("highScore.bin", O_RDONLY);
    fileLeft = fstat(highScorefd, &statBuffer);
    close(highScorefd);
    fileLeft = statBuffer.st_size;
    fileSize = fileLeft;
    
    while(fileSize != 0){
        rank = 0;
        highScorefd = open("highScore.bin", O_RDWR|O_CREAT, S_IRWXU);
        lseek(highScorefd, loopCount * sizeof(HighScore), 0);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        strcpy(Temp.playerName,HighScore.playerName);
        Temp.playerWins = HighScore.playerWins;
        Temp.playerLosses = HighScore.playerLosses;
        lseek(highScorefd, 0, 0);
        fileLeft = statBuffer.st_size;
        while(fileLeft != 0){
            num = read(highScorefd, &HighScore, sizeof(HighScore));
            if (strcmp(Temp.playerName, HighScore.playerName) ==0){
            }
            else if (Temp.playerWins > HighScore.playerWins){
                rank++;
            }
            fileLeft = fileLeft - sizeof(HighScore);
        }
        lseek(highScorefd, loopCount * sizeof(HighScore), 0);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        lseek(highScorefd, loopCount * sizeof(HighScore), 0);
        HighScore.playerRank = rank;
        num = write(highScorefd, &HighScore, sizeof(HighScore));
        close(highScorefd);
        
        
        loopCount++;
        fileSize = fileSize - sizeof(HighScore);
    }
}

/*------------------------------------------------------*
| UpdateWinnerScore() is a function that will either add |
| or update the winner's score in the highscore file. |
*------------------------------------------------------*/

void updateWinnerScore(char *winner){
    struct HIGHSCORE HighScore;
    int doneSearch = FALSE;
    int highScorefd = 0, num = 0, loopCount = 0;
    
    if (playerExistHighScore(winner) == TRUE){
        highScorefd = open("highScore.bin", O_RDWR|O_CREAT, S_IRWXU);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        while(doneSearch == FALSE){
            if(strcmp(HighScore.playerName, winner) == 0){
                HighScore.playerWins = HighScore.playerWins+1;
                doneSearch = TRUE;
            }
            else{
                loopCount++;
                num = read(highScorefd, &HighScore, sizeof(HighScore));
            }
        }
        lseek(highScorefd, loopCount * sizeof(HighScore), 0);
        num = write(highScorefd, &HighScore, sizeof(HighScore));
        close(highScorefd);
    }
    else{
        highScorefd = open("highScore.bin", O_RDWR|O_CREAT|O_APPEND, S_IRWXU);
        strcpy(HighScore.playerName, winner);
        HighScore.playerWins = 1;
        HighScore.playerLosses = 0;
        HighScore.playerTies = 0;
        num = write(highScorefd, &HighScore, sizeof(HighScore));
        close(highScorefd);
    }
}

/*------------------------------------------------------*
| UpdateLoserScore() is a function that will either add |
| or update the loser's score in the highscore file. |
*------------------------------------------------------*/

void updateLoserScore(char *loser){
    struct HIGHSCORE HighScore;
    int doneSearch = FALSE;
    int highScorefd = 0, num = 0, loopCount = 0;
    
    if (playerExistHighScore(loser) == TRUE){
        //printf("Player Exists!\n");
        highScorefd = open("highScore.bin", O_RDWR|O_CREAT, S_IRWXU);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        while(doneSearch == FALSE){
            if(strcmp(HighScore.playerName, loser) == 0){
                HighScore.playerLosses = HighScore.playerLosses+1;
                doneSearch = TRUE;
            }
            else{
                loopCount++;
                num = read(highScorefd, &HighScore, sizeof(HighScore));
            }
        }
        lseek(highScorefd, loopCount * sizeof(HighScore), 0);
        num = write(highScorefd, &HighScore, sizeof(HighScore));
        close(highScorefd);
    }
    else{
        highScorefd = open("highScore.bin", O_RDWR|O_CREAT|O_APPEND, S_IRWXU);
        strcpy(HighScore.playerName, loser);
        HighScore.playerWins = 0;
        HighScore.playerLosses = 1;
        HighScore.playerTies = 0;
        num = write(highScorefd, &HighScore, sizeof(HighScore));
        close(highScorefd);
    }
}

/*------------------------------------------------------*
| UpdateTieScore() is a function that will either add |
| or update the loser's score in the highscore file. |
*------------------------------------------------------*/

void updateTieScore(char *player){
    struct HIGHSCORE HighScore;
    int doneSearch = FALSE;
    int highScorefd = 0, num = 0, loopCount = 0;
    
    if (playerExistHighScore(player) == TRUE){
        //printf("Player Exists!\n");
        highScorefd = open("highScore.bin", O_RDWR|O_CREAT, S_IRWXU);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        while(doneSearch == FALSE){
            if(strcmp(HighScore.playerName, player) == 0){
                HighScore.playerTies = HighScore.playerTies+1;
                doneSearch = TRUE;
            }
            else{
                loopCount++;
                num = read(highScorefd, &HighScore, sizeof(HighScore));
            }
        }
        lseek(highScorefd, loopCount * sizeof(HighScore), 0);
        num = write(highScorefd, &HighScore, sizeof(HighScore));
        close(highScorefd);
    }
    else{
        highScorefd = open("highScore.bin", O_RDWR|O_CREAT|O_APPEND, S_IRWXU);
        strcpy(HighScore.playerName, player);
        HighScore.playerWins = 0;
        HighScore.playerLosses = 0;
        HighScore.playerTies = 1;
        num = write(highScorefd, &HighScore, sizeof(HighScore));
        close(highScorefd);
    }
}

/*----------------------------------------------------------------------*
| getMaxRank() is a function used to find the highest "rank" earned by |
| the players in the highscore.bin file. This is useful for the printing |
| loop for the printing of the high scores. |
*----------------------------------------------------------------------*/

int getMaxRank(){
    struct HIGHSCORE HighScore;
    struct stat statBuffer;
    int highScorefd = 0, num = 0, fileLeft = 0, loopCount = 0;
    int BUFFER_SIZE = 1024;
    int maxRank;
    int tempRank;
    
    highScorefd = open("highScore.bin", O_RDONLY);
    fileLeft = fstat(highScorefd, &statBuffer);
    close(highScorefd);
    fileLeft = statBuffer.st_size;
    
    highScorefd = open("highScore.bin", O_RDONLY);
    num = read(highScorefd, &HighScore, sizeof(HighScore));
    tempRank = HighScore.playerRank;
    while (fileLeft != 0){
        if (tempRank > maxRank){
            maxRank = tempRank;
        }
        fileLeft = fileLeft - sizeof(HighScore);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        tempRank = HighScore.playerRank;
    }
    return maxRank;
}

/*----------------------------------------------------------*
| getRecord() in charge of finding a user record based on the|
| given pName and return it in a struct called WLT |
*----------------------------------------------------------*/

struct WLT getRecord(char *pName){
    struct HIGHSCORE HighScore;
    struct WLT Wlt;
    struct stat statBuffer;
    int doneSearch = FALSE;
    int highScorefd = 0, num = 0, fileLeft = 0, loopCount = 0;
    int BUFFER_SIZE = 1024;
    int winsLossesTies[3];
    
    highScorefd = open("highScore.bin", O_RDONLY|O_CREAT, S_IRWXU);
    fileLeft = fstat(highScorefd, &statBuffer);
    close(highScorefd);
    fileLeft = statBuffer.st_size;
    if (fileLeft == 0){
        Wlt.wins = 0;
        Wlt.losses = 0;
        Wlt.ties = 0;
    }
    
    highScorefd = open("highScore.bin", O_RDWR);
    num = read(highScorefd, &HighScore, sizeof(HighScore));
    while(fileLeft != 0 && doneSearch == FALSE){
        if(strcmp(HighScore.playerName, pName) == 0){
            Wlt.wins = HighScore.playerWins;
            Wlt.losses = HighScore.playerLosses;
            Wlt.ties = HighScore.playerTies;
            doneSearch = TRUE;
        }
        else{
            fileLeft = fileLeft - sizeof(HighScore);
            num = read(highScorefd, &HighScore, sizeof(HighScore));
            Wlt.wins = 0;
            Wlt.losses = 0;
            Wlt.ties = 0;
        }
    }
    close(highScorefd);
    
    return Wlt;
}

/*----------------------------------------------*
| printHighScore() is a function that prints the |
| high scores |
*----------------------------------------------*/

void printHighScore(){
    int maxRank = getMaxRank();
    struct HIGHSCORE HighScore;
    struct stat statBuffer;
    struct TEMP Temp;
    int highScorefd = 0, num = 0, fileLeft = 0, fileSize = 0, loopCount = 0,
    innerLoopCount = 0, entries = 0, i = 1;
    int BUFFER_SIZE = 1024;
    int doneSearch;
    
    highScorefd = open("highScore.bin", O_RDONLY);
    fileLeft = fstat(highScorefd, &statBuffer);
    close(highScorefd);
    fileLeft = statBuffer.st_size;
    fileSize = fileLeft;
    
    printf("+-----------Top 10 High Scores-----------+\n");
    printf("|tName Wins Losses Ties |\n");
    printf("+----------------------------------------+\n");
    
    if (maxRank > 10){
        maxRank = 10;
    }
    while(maxRank >= 0){
        doneSearch = FALSE;
        entries = fileSize / sizeof(HighScore);
        highScorefd = open("highScore.bin", O_RDONLY);
        lseek(highScorefd, loopCount * sizeof(HighScore), 0);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        lseek(highScorefd,0, 0);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        while(entries != 0){
            if (HighScore.playerRank == maxRank){
                printf("|t%s t %d t %d t %d |\n",
                HighScore.playerName, HighScore.playerWins,
                HighScore.playerLosses, HighScore.playerTies);
            }
            num = read(highScorefd, &HighScore, sizeof(HighScore));
            entries--;
        }
        maxRank--;
        fileLeft = fileLeft - sizeof(HighScore);
        loopCount++;
    }
    close(highScorefd);
    printf("+----------------------------------------+\n");
}

/*-----------------------------------------*
| badEncrypt() uses an extremely simply |
| encryption algorithm to encrypt passwords |
*-----------------------------------------*/

void badEncrypt(char *s)
{
    int i, l = strlen(s);
    for(i = 0; i < l; i++)
    s[i] -= 15;
}

/*-------------------------------------------*
| getPName() is used to get a connections name|
| and then sets it in the connections struct |
*-------------------------------------------*/

void getPName(int reply_sock_fd, struct CONTEXT *contextPtr, char *pName){
    int loopCount = 0;
    int doneSearch = FALSE;
    
    while(doneSearch == FALSE){
        if (reply_sock_fd == contextPtr->Connections->con_fd[loopCount]){
            doneSearch = TRUE;
            strcpy(pName,contextPtr->Connections->userName[loopCount]);
        }
        loopCount++;
    }
}

/*----------------------------------------*
| clearPlayerConC() is called at the end of |
| a game when a connection is terminated. |
| used for Context structs. |
*----------------------------------------*/

void clearPlayerConC( int reply_sock_fd, struct CONTEXT *contextPtr){
    int doneSearch = FALSE;
    int loopCount = 0;
    
    while(doneSearch == FALSE){
        if (reply_sock_fd == contextPtr->Connections->con_fd[loopCount]){
            doneSearch = TRUE;
            contextPtr->Connections->con_fd[loopCount] = -1;
            contextPtr->Connections->loginStatus[loopCount] = -1;
            strcpy(contextPtr->Connections->userName[loopCount], "");
            contextPtr->Connections->playWaiting[loopCount] = -1;
        }
        loopCount++;
    }
    loopCount = 0;
    doneSearch = FALSE;
}

/*----------------------------------------*
| clearPlayerConL() is called at the end of |
| a game when a connection is terminated. |
| used for login struct. |
*----------------------------------------*/

void clearPlayerConL( int reply_sock_fd, struct LOGIN *loginPtr){
    int doneSearch = FALSE;
    int loopCount = 0;
    
    while(doneSearch == FALSE){
        if (reply_sock_fd == loginPtr->Connections->con_fd[loopCount]){
            doneSearch = TRUE;
            loginPtr->Connections->con_fd[loopCount] = -1;
            loginPtr->Connections->loginStatus[loopCount] = -1;
            strcpy(loginPtr->Connections->userName[loopCount], "");
            loginPtr->Connections->playWaiting[loopCount] = -1;
        }
        loopCount++;
    }
    loopCount = 0;
    doneSearch = FALSE;
}

void sendHighScore(int reply_sock_fd){
    struct HIGHSCORE HighScore;
    int maxRank = getMaxRank();
    struct stat statBuffer;
    int highScorefd = 0, num = 0, fileLeft = 0, fileSize = 0, loopCount = 0,
    innerLoopCount = 0, entries = 0;
    int hsDone = FALSE;
    
    highScorefd = open("highScore.bin", O_RDONLY);
    fileLeft = fstat(highScorefd, &statBuffer);
    close(highScorefd);
    fileLeft = statBuffer.st_size;
    fileSize = fileLeft;
    if (maxRank > 10){
        maxRank = 10;
    }
    while(maxRank >= 0){
        entries = fileSize / sizeof(HighScore);
        highScorefd = open("highScore.bin", O_RDONLY);
        lseek(highScorefd, loopCount * sizeof(HighScore), 0);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        lseek(highScorefd,0, 0);
        num = read(highScorefd, &HighScore, sizeof(HighScore));
        while(entries != 0){
            if (HighScore.playerRank == maxRank){
                send(reply_sock_fd, &hsDone, sizeof(hsDone),0);
                send(reply_sock_fd, &HighScore, sizeof(HighScore),0);
            }
            num = read(highScorefd, &HighScore, sizeof(HighScore));
            entries--;
        }
        maxRank--;
        fileLeft = fileLeft - sizeof(HighScore);
        loopCount++;
        close(highScorefd);
    }
    hsDone = TRUE;
    send(reply_sock_fd, &hsDone, sizeof(hsDone),0);
}
