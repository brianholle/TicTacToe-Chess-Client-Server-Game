/* Program Name: asgn9client-team1.c
Author:  Brian Holle
Date: Dec.10, 2015
File name: chess_clientv2.c
Compile:  cc -o client chess_clientv2.c
Run:  ./client server1.cs.scranton.edu
Description: This program is a simulation of a tic-tac-toe game
where player 1 (X) and player 2(O) alternate between turns to
choose coordinates of a tic tac toe board. The players connect to
this server using client processes, which have the socket information
needed to connect to this. The players alternate turns using the
layout below and Using (Row, Column) as input. This Version handles
multiple users playing games at the same time on the server.

I also implemented a chess game. It is not completed, but is able to play games in full.

        COLUMN
        0  1  2
    0|___|___|___|
ROW 1|___|___|___|
    2|___|___|___|

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define HTTPPORT "32100"
#define BUFFERSIZE 256
#define ROWS 3
#define COLS 3
#define CHESSROWS 8
#define CHESSCOLS 8
#define TRUE 1
#define FALSE 0
typedef char Board[ROWS][COLS];
typedef char ChessBoard[CHESSROWS][CHESSCOLS];
typedef char playerPieceLoc[CHESSROWS][CHESSCOLS];

//Struct to hold the input for each turn
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

struct WLT {
    int wins;
    int losses;
    int ties;
};

struct HIGHSCORE {
    char playerName[80];
    int playerWins;
    int playerLosses;
    int playerTies;
    int playerRank;
};

int get_server_connection(char *hostname, char *port);
void ticTacToe(int http_conn);
void printBoard(Board brd);
void login(int http_conn);

//for chess
void chess(int http_conn);
void printBoardChess(ChessBoard Chessboard, playerPieceLoc p2Loc);


int main(int argc, char *argv[]){
    struct HIGHSCORE HighScore;
    int http_conn;
    char http_request[BUFFERSIZE];
    int done = FALSE;
    char input[2];
    int maxRank = 1;
    int hsDone = FALSE;
    int playersOnline;
    char oNames[80];
    
    //Get the connection to the server, if it fails, give error
    if ((http_conn = get_server_connection(argv[1], HTTPPORT)) == -1) {
        printf("connection error\n");
        exit(1);
    }
    
    login(http_conn);
    printf("Commands:\n");
    printf(" P-Join Waiting Queue\n");
    printf(" C-Join Chess Queue\n");
    printf(" H-Show Top 10 High Scores\n");
    printf(" L-List Online Players\n");
    printf(" S-Send Invite(for tic-tac-toe)\n");
    printf(" A-Accept Invites(for tic-tac-toe)\n");
    printf(" ?-Shows Commands\n");
    printf(" Q-Quit\n");
    while( done == FALSE ){
        printf("What would you like to do?\n");
        scanf("%s", input);
        send(http_conn, &input, sizeof(input), 0);
        if ( input[0] == 'P' ){
            ticTacToe(http_conn);
            done = TRUE;
            } else if ( input[0] == 'C'){
            chess(http_conn);
            done = TRUE;
            } else if ( input[0] == 'H' ){
            printf("+-----------Top 10 High Scores-----------+\n");
            printf("|tName Wins Losses Ties  |\n");
            printf("+----------------------------------------+\n");
            while(hsDone == FALSE){
                recv(http_conn, &hsDone, sizeof(hsDone),0);
                if(hsDone == FALSE){
                    recv(http_conn, &HighScore, sizeof(HighScore),0);
                    printf("|t%s t %d t %d t %d  |\n",
                    HighScore.playerName, HighScore.playerWins,
                    HighScore.playerLosses, HighScore.playerTies);
                }
            }
            hsDone = FALSE;
            printf("+----------------------------------------+\n");
            } else if ( input[0] == 'L' ){
            recv(http_conn, &playersOnline, sizeof(playersOnline),0);
            if (playersOnline == 1){
                printf("No Players Online\n");
                } else {
                printf("+-Online Players-+\n");
                while(playersOnline > 0){
                    recv(http_conn, &oNames, sizeof(oNames),0);
                    printf("|%s\n", oNames);
                    playersOnline--;
                }
                printf("+-Online Players-+\n");
            }
            } else if (input[0] == 'S'){
            printf("Who would you like to send an invitation to?\n");
            scanf("%s", oNames);
            send(http_conn, &oNames, sizeof(oNames),0);
            printf("Invitation Sent.\n");
            ticTacToe(http_conn);
            done = TRUE;
            } else if (input[0] == 'A'){
            recv(http_conn, &oNames, sizeof(oNames), 0);
            printf("You have an invite from: %s\n", oNames);
            printf("Press 'Y' to accept or 'N' to deny.\n");
            scanf("%s", input);
            send(http_conn, &input, sizeof(input), 0);
            if (input[0] == 'Y'){
                ticTacToe(http_conn);
                done = TRUE;
            }
            } else if (input[0] == 'Q'){
            done = TRUE;
            //quit server connection
            } else if ( input[0] == '?'){
            printf("Commands:\n");
            printf(" P-Join Waiting Queue\n");
            printf(" H-Show Top 10 High Scores\n");
            printf(" L-List Online Players\n");
            printf(" A-Accept Invites\n");
            printf(" ?-Shows Commands\n");
            printf(" ?-Shows Commands\n");
            printf(" Q-Quit\n");
            }else {
            printf("invalid input\n");
        }
    }
    //close the socket when done
    close(http_conn);
}
//Get connection to server using the hostname and the port
int get_server_connection(char *hostname, char *port) {
    int serverfd;
    struct addrinfo hints, *servinfo, *p;
    int status;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        printf("getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }
    
    for (p = servinfo; p != NULL; p = p ->ai_next) {
        if ((serverfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
            printf("socket not found \n");
            continue;
        }
        
        if ((status=connect(serverfd, p->ai_addr, p->ai_addrlen)) == -1) {
            close(serverfd);
            printf("socket connect failure \n");
            continue;
        }
        break;
    }
    
    freeaddrinfo(servinfo);
    if(status == -1){
        return -1;
    }
    else{
        return serverfd;
    }
}

//prints the tic-tac-toe board in a readable fashion.
void printBoard(Board brd){
    printf("\n");
    printf("-------------\n");
    printf("| %c | %c | %c |\n", brd[0][0], brd[0][1], brd[0][2]);
    printf("-------------\n");
    printf("| %c | %c | %c |\n", brd[1][0], brd[1][1], brd[1][2]);
    printf("-------------\n");
    printf("| %c | %c | %c |\n", brd[2][0], brd[2][1], brd[2][2]);
    printf("-------------\n");
}


//Tic-Tac_Toe game for a client/server game. The clients are matched using
//the server and play against each other taking turns entering their input
//in the form of row_column.
void ticTacToe(int http_conn) {
    struct WLT WltP1;
    struct WLT WltP2;
    struct WLT WltP3;
    struct WLT WltP4;
    struct TURN Turn;
    char buf[256];
    int row;
    int column;
    int returnedValue;
    int skipCode;
    int errorCode;
    int winner = FALSE;
    char opponentName[80];
    int recordP2[3];
    int recordP1[3];
    int k;
    
    Board board;
    
    //If no winner exists on the board, receive from the
    //the server an ErrorCode that tells it what to do.
    printf("waiting for opponent to connect.\n");
    recv(http_conn, &opponentName, sizeof(opponentName),0);
    printf("Connected. Opponent's name: %s\n", opponentName);
    recv(http_conn, &skipCode, sizeof(skipCode),0);
    recv(http_conn, &WltP1, sizeof(WltP1), 0);
    recv(http_conn, &WltP2, sizeof(WltP2), 0);
    printf("Your:\n");
    printf("Wins Losses Ties\n");
    printf("%d %d %d\n",WltP1.wins, WltP1.losses, WltP1.ties);
    printf("%s's:\n", opponentName);
    printf("Wins Losses Ties\n");
    printf("%d %d %d\n",WltP2.wins, WltP2.losses, WltP2.ties);
    while(!winner){
        if (skipCode != -1){
            printf("waiting for %s's move.\n", opponentName);
        }
        skipCode++;
        //Receive from the server an errorCode that lets the
        //Player know what to do next.
        recv(http_conn, &errorCode, sizeof(errorCode),0);
        //Receive the board so the client holds an updated
        //version of the board.
        recv(http_conn, board, sizeof(board),0);
        //Print the board.
        printBoard(board);
        
        //while it's your turn or you put invalid input it enter
        //a move.
        while(errorCode == 0 || errorCode == 1){
            printf("Enter move:");
            scanf("%d%d", &row, &column);
            Turn.row = row;
            Turn.column = column;
            send(http_conn, &Turn, sizeof(Turn), 0);
            recv(http_conn, &errorCode, sizeof(errorCode),0);
            if(errorCode == 1){
                printf("error: invalid input, try again\n");
                recv(http_conn, &errorCode, sizeof(errorCode),0);
                recv(http_conn, board, sizeof(board),0);
            }
        }
        //Lets the client know they're the winner of the board
        if(errorCode == 2){
            recv(http_conn, board, sizeof(board), 0);
            printBoard(board);
            printf("You are the winner\n");//there's a winner
            winner = TRUE;
            WltP1.wins++;
            WltP2.losses++;
        }
        //Lets the client know the game was a tie.
        if(errorCode == 3){
            recv(http_conn, board, sizeof(board), 0);
            printBoard(board);
            printf("The game is a tie\n");
            winner = TRUE;
            WltP1.ties++;
            WltP2.ties++;
        }
        
        //Lets the client know their move was valid.
        if(errorCode == 4){
            recv(http_conn, board, sizeof(board), 0);
            printBoard(board);
        }
        //Lets the client know they're the loser.
        if(errorCode == 5){
            recv(http_conn, board, sizeof(board), 0);
            printf("You are the loser\n");
            printf("\n");
            winner = TRUE;
            WltP1.losses++;
            WltP2.wins++;
        }
        //Lets the client know that the board is a tie.
        //Two of these are needed to ensure the board
        //doesn't print twice for the player who doesn't
        //initiate the tie.
        if(errorCode == 6){
            recv(http_conn, board, sizeof(board), 0);
            printf("The game is a tie\n");
            winner = TRUE;
            WltP1.ties++;
            WltP2.ties++;
        }
    }
    printf("Your:\n");
    printf("Wins Losses Ties\n");
    printf("%d %d %d\n",WltP1.wins, WltP1.losses, WltP1.ties);
    printf("%s's:\n", opponentName);
    printf("Wins Losses Ties\n");
    printf("%d %d %d\n",WltP2.wins, WltP2.losses, WltP2.ties);
}

void chess(int http_conn){
    struct CHESSTURN ChessTurn;
    int errorCode;
    int row;
    int column;
    int winner = FALSE;
    char opponentName[80];
    
    ChessBoard ChessBoard;
    playerPieceLoc p2Loc;
    
    //If no winner exists on the board, receive from the
    //the server an ErrorCode that tells it what to do.
    printf("waiting for opponent to connect.\n");
    recv(http_conn, &opponentName, sizeof(opponentName),0);
    printf("Connected. Opponent's name: %s\n", opponentName);
    
    while(!winner){
        printf("Waiting for opponent\n");
        recv(http_conn, &errorCode, sizeof(errorCode),0);
        recv(http_conn, ChessBoard, sizeof(ChessBoard),0);
        recv(http_conn, p2Loc, sizeof(playerPieceLoc), 0);
        printBoardChess(ChessBoard, p2Loc);
        
        while(errorCode == 0){
            printf("Please enter your move (current loc destination loc):\n");
            scanf("%d%d%d%d", &ChessTurn.pieceRow, &ChessTurn.pieceColumn, &ChessTurn.row, &ChessTurn.column);
            send(http_conn, &ChessTurn, sizeof(ChessTurn), 0);
            recv(http_conn, &errorCode, sizeof(errorCode),0);
            if(errorCode == 0){
                recv(http_conn, ChessBoard, sizeof(ChessBoard),0);
                recv(http_conn, p2Loc, sizeof(playerPieceLoc), 0);
                printf("error: invalid input, try again\n");
            }
        }
        if (errorCode == 1 ){
            recv(http_conn, ChessBoard, sizeof(ChessBoard),0);
            recv(http_conn, p2Loc, sizeof(p2Loc), 0);
            printBoardChess(ChessBoard, p2Loc);
        }
        if (errorCode == 2 ){
            recv(http_conn, ChessBoard, sizeof(ChessBoard),0);
            recv(http_conn, p2Loc, sizeof(p2Loc), 0);
            printBoardChess(ChessBoard, p2Loc);
            printf("You are the winner!\n");
			winner = TRUE;
        }
        if (errorCode == 3 ){
            printf("You are the Loser!\n");
			winner = TRUE;
        }
    }
}

void printBoardChess(ChessBoard Chessboard, playerPieceLoc p2Loc){
    int outerLoop = 0;
	int innerLoop = 0;
	
	printf("\n");
    printf("    0    1   2   3   4   5   6   7   \n");
	    while(outerLoop < 8){
			printf("  +---+---+---+---+---+---+---+---+\n");
		    printf("%d ", outerLoop);
			while(innerLoop < 8 ){
				if(strncmp(&Chessboard[outerLoop][innerLoop], &p2Loc[outerLoop][innerLoop], 1) == 0){
					printf("| \033[22;32m%c \033[0m", Chessboard[outerLoop][innerLoop]);
				} else {
					printf("| %c ", Chessboard[outerLoop][innerLoop]);
				}
                innerLoop++;
			}
			innerLoop = 0;
		    printf("|\n");
		    outerLoop++;
        }
	printf("  +---+---+---+---+---+---+---+---+\n");
	
}

void login(int http_conn){
    char playerName[80];
    char playerPass[80];
    int done = FALSE;
    int validLogin = 0;
    
    printf("Please enter a username:\n");
    scanf("%s",playerName);
    printf("Please enter a password (no spaces):\n");
    scanf("%s",playerPass);
    
    while(done == FALSE){
        
        send(http_conn, playerName, sizeof(playerName), 0);
        send(http_conn, playerPass, sizeof(playerPass), 0);
        recv(http_conn, &validLogin, sizeof(validLogin), 0);
        if (validLogin == 1){
            done = TRUE;
            printf("Successfully Logged in!\n");
            }else if (validLogin == 2){
            printf("New User Created! Welcome!\n");
            done = TRUE;
            }else{
            printf("Invalid Username or Password!\n");
            printf("Please enter a username:\n");
            scanf("%s",playerName);
            printf("Please enter a password (no spaces):\n");
            scanf("%s",playerPass);
        }
    }
}
