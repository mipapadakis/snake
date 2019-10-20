
/*
THIS IS THE SNAKE GAME.
CREATED IT USING A TABLE OF 'SNAKE' STRUCTS
*/

//right ctrl+F to exit fullscreen
#include <stdio.h>
#include <termios.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
/*----------------------ADJUST THE SPEED OF SNAKE, AND THE WIDTH OR HEIGHT OF THE TABLE.----------------------*/
#define WIDTH 30
#define HEIGHT 20
#define TIME 170000 //This is useconds between the frames, so increasing TIME will decrease the snake's speed
/*------------------------------------------------------------------------------------------------------------*/
#define UP_ARROW 65
#define DOWN_ARROW 66
#define RIGHT_ARROW 67
#define LEFT_ARROW 68
const char EMPTY = ' ';
const char BODY = 'O';
const char SNAKEHEAD = 'X';
const char FOOD = '@';
const char LINEMARGIN = '|';
const char COLMARGIN = '=';
const char GAMEOVER = 'g';
const char ERROR = 'e';
const char WIN = 'w';
const char FOUNDFOOD = 'f';
const char UP = '^';
const char DOWN = 'v';
const char LEFT = '<';
const char RIGHT = '>';
int xi,xj,score=0;

struct snake {
	
	int i,j;
	char value; //BODY or SNAKEHEAD or EMPTY or FOOD or '|' or '=' or '\n'
	struct snake *prev; //used to follow the snake from head to tail
	struct snake *left;
	struct snake *right;
	struct snake *up;
	struct snake *down;
	
};

typedef struct snake snake_T;

char generate_food( snake_T table[HEIGHT+2][WIDTH+3]);

char move(snake_T table[HEIGHT+2][WIDTH+3], char old, char new);

void delete_tail(snake_T table[HEIGHT+2][WIDTH+3]);

void print(snake_T table[HEIGHT+2][WIDTH+3]);

int getch(void);


/* TABLE DIMENSIONS;
 
 <------------WIDTH+3------------->
 
 ================================\n     ^
 |                  ^           |\n     |
 |                  |           |\n     |
 |                  |           |\n     |
 |<------WIDTH------|---------->|\n     |
 |                  |           |\n     |
 |                  |           |\n     |
 |                  |           |\n  HEIGHT+2
 |                HEIGHT        |\n     | 
 |                  |           |\n     |
 |                  |           |\n     |
 |                  |           |\n     |
 |                  v           |\n     |
 ================================\n     v
 
 */

int main( int argc, char *argv[]){
	char c, old = '\0', new = '\0';
	int i,j,fd[2],command,counter=0;
	time_t t;
	pid_t child_pid, temp_child_pid;
	snake_T table[HEIGHT+2][WIDTH+3], margin_node;
	
	//table's pointers that point out of the table (eg. table[0][5].up or table[18][0].left) will all point to the margin_node.
	//Initializing margin_node:
	margin_node.i = -1;
	margin_node.j = -1;
	margin_node.value = ERROR;
	margin_node.prev = NULL;
	margin_node.up = NULL;
	margin_node.down = NULL;
	margin_node.left = NULL;
	margin_node.right = NULL;
	
	//Intialize random number generator
	srand((unsigned) time(&t));
	
	//initialize snake table (margins and EMPTY values)
	for(i=0;i<HEIGHT+2;i++){
		for(j=0;j<WIDTH+3;j++){
			if(i==0 || i==(HEIGHT+1) ){
				if(j!=WIDTH+2){
					table[i][j].value = COLMARGIN; // '='
				}
				else{
					table[i][j].value = '\n';
				}
			}
			else if( j==0 || j==(WIDTH+1) ){
				table[i][j].value = LINEMARGIN; // '|'
			}
			else if( j==(WIDTH+2) ){
				table[i][j].value = '\n';
			}
			else{
				table[i][j].value = EMPTY;
			}
		}
	}
	
	//Initialize {up, down, left, right, prev, i, j} pointers of the nodes
	for(i=0;i<HEIGHT+2;i++){
		for(j=0;j<WIDTH+3;j++){
			table[i][j].prev = NULL;
			table[i][j].i = i;
			table[i][j].j = j;
			if(i>0)
				table[i][j].up = &table[i-1][j];
			else
				table[i][j].up = &margin_node;
			if(i+1<HEIGHT+2)
				table[i][j].down = &table[i+1][j];
			else
				table[i][j].down = &margin_node;
			if(j>0)
				table[i][j].left = &table[i][j-1];
			else
				table[i][j].left = &margin_node;
			if(j+2<WIDTH+3)
				table[i][j].right = &table[i][j+1];
			else
				table[i][j].right = &margin_node;
		}
	}
	
	//Place initial snake
	table[1][1].value = BODY;
	table[1][1].prev = NULL;
	
	table[1][2].value = BODY;
	table[1][2].prev = &table[1][1];
	
	table[1][3].value = SNAKEHEAD;
	table[1][3].prev = &table[1][2];
	xi = 1;
	xj = 3;
	
	//Generate first food
	c = generate_food( table );
	if(c!=FOOD){
		printf("\nSomething went wrong.\n");
		return(1);
	}
	
	//Print Initialized table
	print(table);
	usleep(TIME);
	
	pipe(fd);
	
	//At the start of the game, the snake moves to the right.
	new = RIGHT;
	
	/*OVERVIEW;
	I will use 3 processes:
	1)The parent process, which reads the user's command and sends it to temp_child.
	2)The first child process, which receives a command through the pipe and immediately prints the adjusted table to the console.
	3)The second child process (temp_child), which repeatedly sends a command to the first child. If the user sends a new command,
	  the existing temp_child gets killed and a new temp_child is then created, repeatedly sending the new command. And so on.
	  This happens because even when the user doesn't type anything, the snake must keep going.
	*/

	child_pid = fork();
	
	/*-----------------------------------------------------------------C H I L D:---------------------------------------------------------------------*/
	if(child_pid == 0){ //Child process that continuously prints the table

		//Child is reading only, so I close the write-descriptor.
		close(fd[1]);
		
		do{
			//Receive a command from temp_child:
			if( read(fd[0], &new, sizeof(new))==-1 ){
				printf("\nSomething went wrong.\n");
				close(fd[0]);
				break;
			}
			
			//MOVE SNAKE AND PRINT TABLE:
			c = move( table, old, new);
			if (c==FOUNDFOOD){ //If after move function the snake has eaten food, call generate_food
				score++;
				c = generate_food( table );
				if (c==WIN){
					print(table);
					close(fd[0]);
					printf("\n\nYOU WON!\n");
					break;
				}
				else if(c==FOUNDFOOD){
					printf("\nThere is food in table.\n");
				}
				else if(c!=FOOD){
					printf("\nSomething went wrong.\n");
					close(fd[0]);
					break;
				}
			}
			else if(c==GAMEOVER){
				print(table);
				close(fd[0]);
				printf("\n\nGAME OVER!\n");
				break;
			}
			else if(c==ERROR){
				print(table);
				close(fd[0]);
				printf("\n\nERROR!\n");
				break;
			}
			print(table);
			
			//If the command prompts the snake to move to its own neck:
			if( ((new == UP)&&(table[xi][xj].up==table[xi][xj].prev))||
			((new == DOWN)&&(table[xi][xj].down==table[xi][xj].prev))||
			((new == LEFT)&&(table[xi][xj].left==table[xi][xj].prev))||
			((new == RIGHT)&&(table[xi][xj].right==table[xi][xj].prev)) ){
				new=old;
			}
			
			old=new;
			
		}while(1);
		kill(getppid(),SIGTERM); //Terminate the parent process
	}
	/*----------------------------------------------------------------P A R E N T:--------------------------------------------------------------------*/
	else{
		//Parent is writing only, so close read-descriptor.
		close(fd[0]);
		
		if( write(fd[1], &new, sizeof(new)) == -1){
			printf("\nSomething went wrong.\n");
			kill(child_pid,SIGKILL); //Kill the child process
			return(1);
		}
		
		//RECEIVE COMMAND (and send it to the child process):
		do{
			
			/*
			The temp_child will repeatedly send the 'new' command to the first child.
			Meanwhile, the parent process will go on waiting for the user's next command,
			and when it gets it, it will kill the old temp_child and create a new one,
			which in turn will start to repeatedly send the new command, and so on.
			*/
			temp_child_pid = fork();
			
			/*-------------------------------------------------------T E M P _ C H I L D----------------------------------------------------------------------*/
			if(temp_child_pid==0){
				do{
					//Send 'new' to pipe:
					if( write(fd[1], &new, sizeof(new)) == -1){
						printf("\nSomething went wrong.\n");
						kill(child_pid,SIGKILL);
						break;
					}
					usleep(TIME);
					
				}while(1);
				kill(getppid(),SIGTERM); //Terminate the parent process
			}
			/*------------------------------------------------------------------------------------------------------------------------------------------------*/
			
			
			/* The integer form of the arrows on the keyboard are 3 integers. The first two are the same for all 4 arrows: 27 and 91.
			Only the last integers are different (65,66,67,68), so I use only them to distinguish which arrow was pressed. Thus:   */
			for(counter=0;counter<3;counter++){
				command = getch();
				if( command == -1 ){
					kill(child_pid,SIGKILL); //Kill the child process
					break;
				}
			}
			
			//Initialize new:
			if(command == UP_ARROW){ //UP_ARROW=65
				new = UP;
				old=new;//
			}
			else if(command == DOWN_ARROW){ //DOWN_ARROW=66
				new = DOWN;
				old=new;//
			}
			else if(command == RIGHT_ARROW){ //RIGHT_ARROW=67
				new = RIGHT;
				if( old!='\0')//
					old=new;
			}
			else if(command == LEFT_ARROW){ //LEFT_ARROW=68
				if( old=='\0') //At the start of the game, if the first key I press is left, the snake must keep moving RIGHT!
					new = RIGHT;
				else{
					new = LEFT;
					old=new;//
				}
			}
			//else, new doesn't change, so the snake moves to the same direction
			

			//Kill temp_child (in case the command has changed)
			if( kill(temp_child_pid,SIGKILL)==-1 ){
				printf("\nSomething went wrong.\n");
				break;
			}
			
			if( kill(child_pid,0)==-1 ){
				//Check if the child exists, otherwise exit.
				break;
			}
			
		}while(1);
		
		close(fd[1]);
	}
	
	return(0);
}


char generate_food( snake_T table[HEIGHT+2][WIDTH+3]){
	int i,j,flag=0;
	
	//Check if full
	for(i=1;i<HEIGHT+1;i++){
		for(j=1;j<WIDTH+1;j++){
			if(table[i][j].value != EMPTY){
				if(table[i][j].value == FOOD){
					return(FOUNDFOOD); //Found food, so there is no need to generate food => exit function
				}
				//If table has not a single ' ' character (and no placed food), then the program never enters the else below
			}
			else{
				flag=1;//Flag=1 when at least one EMPTY value is found
			}
		}
	}
	
	if(flag){ //flag up, and no food found (otherwise the function would've returned FOUNDFOOD). Thus, generate food in new position
		do{
			i = rand()%(HEIGHT) + 1;
			j = rand()%(WIDTH) + 1;
			if( table[i][j].value == EMPTY){
				table[i][j].value = FOOD; //Place food in the random empty position
				break;
			}
		}while(1);
		return(FOOD);
	}
	else{
		//No EMPTY found, no food left, Game Won!
		return(WIN);
	}
	
}

void delete_tail(snake_T table[HEIGHT+2][WIDTH+3]){ 
	snake_T temp;
	
	temp = table[xi][xj]; //temp=head
	
	//search and delete TAIL
	do{
		if(temp.prev->prev==NULL){ //(The temp node is the node right before the tail)
			
			//The tail is temp.prev. Deleting table[temp.i][temp.j]:
			table[temp.i][temp.j].prev->value = EMPTY;
			table[temp.i][temp.j].prev = NULL;
			break;
		}
		else{
			//Tail not found yet, move to next node:
			temp = *temp.prev;
		}
	}while(1);
}

char move(snake_T table[HEIGHT+2][WIDTH+3], char old, char new){
	int i=xi,j=xj;
	char c,value;
	
	if( ((new == UP)&&(table[i][j].up == NULL))||
		((new == DOWN)&&(table[i][j].down == NULL))||
		((new == LEFT)&&(table[i][j].left == NULL))||
		((new == RIGHT)&&(table[i][j].right == NULL)) ){
		printf("ERROR\n");
		return(ERROR);
	}
	
	if(new == '\0'){
		if(old=='\0') //If no command has been given yet, the snake will continue to the right until a command is given
			c = move(table, old, RIGHT);
		else{
			printf("ERROR\n");
			return(ERROR);
		}
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//If the command ('new') prompts the snake to move to its own body, then there are two possibilities:
	//i) if the command prompts the snake to move to its own neck, the snake continues moving to the old direction (so new = old for the next loop!).
	//ii)if the command prompts the snake to move to a random part of its body, Game Over.
	else if( ((table[i][j].up->value == BODY) && (new == UP))||
		((table[i][j].down->value == BODY) && (new == DOWN))||
		((table[i][j].left->value == BODY) && (new == LEFT))||
		((table[i][j].right->value == BODY) && (new == RIGHT)) ){
		
		if( ((new == UP)&&(table[i][j].up==table[i][j].prev))||
			((new == DOWN)&&(table[i][j].down==table[i][j].prev))||
			((new == LEFT)&&(table[i][j].left==table[i][j].prev))||
			((new == RIGHT)&&(table[i][j].right==table[i][j].prev)) ){
			
			//case i): Continue same direction
			if(new == UP)
				c = move(table, old, DOWN);
			else if(new == DOWN)
				c = move(table, old, UP);
			else if(new == LEFT)
				c = move(table, old, RIGHT);
			else if(new == RIGHT)
				c = move(table, old, LEFT);
			else
				return(ERROR);
		}
		else{
			//case ii)
			c = move( table, new, GAMEOVER);
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		
	}
	else{
		if( (new==UP)||(old == UP && new==GAMEOVER) ){
			if(new==GAMEOVER){ //The player moved the snake to a BODY character. Game Over!
				//Move snakehead up
				table[i][j].up->value = SNAKEHEAD; 
				//(I must not initialize {table[i][j].up->prev}, in order for the delete_tail to work!)
				table[i][j].value = BODY;
				xi--; //initialize xi of the new head
				delete_tail(table);
				return(GAMEOVER);
			}
			
			//Initialize value with the char of the node to which the snake is commanded to move:
			value = table[i][j].up->value;
			
			//Move snakehead up
			table[i][j].up->value = SNAKEHEAD;
			table[i][j].up->prev = &table[i][j];
			table[i][j].value = BODY;
			xi--; //initialize xi of the new head
			if(value == FOOD){
				//No need to delete tail, as the snake increased in size
				return(FOUNDFOOD);
			}
			else{
				delete_tail(table);
				if(value == EMPTY){
					return(EMPTY);
				}
				else{ //The player moved the snake to a MARGIN character. Game Over!
					return(GAMEOVER);
				}
			}
		}
		if( (new==DOWN)||(old == DOWN && new==GAMEOVER) ){
			if(new==GAMEOVER){ //The player moved the snake to a BODY character. Game Over!
				printf("MOVED TO BODY CHAR!\n");
				//Move snakehead down
				table[i][j].down->value = SNAKEHEAD; 
				//(I must not initialize {table[i][j].up->prev}, in order for the delete_tail to work!)
				table[i][j].value = BODY;
				xi++; //initialize xi of the new head
				delete_tail(table);
				printf("MOVED TO BODY CHAR!!\n");
				return(GAMEOVER);
			}
			
			//Initialize value with the char of the node to which the snake is commanded to move:
			value = table[i][j].down->value;
			
			//Move snakehead down
			table[i][j].down->value = SNAKEHEAD;
			table[i][j].down->prev = &table[i][j];
			table[i][j].value = BODY;
			xi++; //initialize xi of the new head
			if(value == FOOD){
				//No need to delete tail, as the snake increased in size
				return(FOUNDFOOD);
			}
			else{
				delete_tail(table);
				if(value == EMPTY){
					return(EMPTY);
				}
				else{ //The player moved the snake to a MARGIN character. Game Over!
					return(GAMEOVER);
				}
			}
		}
		if( (new==LEFT)||(old == LEFT && new==GAMEOVER) ){
			if(new==GAMEOVER){ //The player moved the snake to a BODY character. Game Over!
				printf("MOVED TO BODY CHAR!\n");
				//Move snakehead left
				table[i][j].left->value = SNAKEHEAD; 
				//(I must not initialize {table[i][j].left->prev}, in order for the delete_tail to work!)
				table[i][j].value = BODY;
				xj--; //initialize xj of the new head
				delete_tail(table);
				printf("MOVED TO BODY CHAR!!\n");
				return(GAMEOVER);
			}
			
			//Initialize value with the char of the node to which the snake is commanded to move:
			value = table[i][j].left->value;
			
			//Move snakehead left
			table[i][j].left->value = SNAKEHEAD;
			table[i][j].left->prev = &table[i][j];
			table[i][j].value = BODY;
			xj--; //initialize xj of the new head
			if(value == FOOD){
				//No need to delete tail, as the snake increased in size
				return(FOUNDFOOD);
			}
			else{
				delete_tail(table);
				if(value == EMPTY){
					return(EMPTY);
				}
				else{ //The player moved the snake to a MARGIN character. Game Over!
					return(GAMEOVER);
				}
			}
		}
		if( (new==RIGHT)||(old == RIGHT && new==GAMEOVER) ){
			if(new==GAMEOVER){ //The player moved the snake to a BODY character. Game Over!
				printf("MOVED TO BODY CHAR!\n");
				//Move snakehead right
				table[i][j].right->value = SNAKEHEAD; 
				//(I must not initialize {table[i][j].right->prev}, in order for the delete_tail to work!)
				table[i][j].value = BODY;
				xj++; //initialize xj of the new head
				delete_tail(table);
				printf("MOVED TO BODY CHAR!!\n");
				return(GAMEOVER);
			}
			
			//Initialize value with the char of the node to which the snake is commanded to move:
			value = table[i][j].right->value;
			
			//Move snakehead right
			table[i][j].right->value = SNAKEHEAD;
			table[i][j].right->prev = &table[i][j];
			table[i][j].value = BODY;
			xj++; //initialize xj of the new head
			if(value == FOOD){
				//No need to delete tail, as the snake increased in size
				return(FOUNDFOOD);
			}
			else{
				delete_tail(table);
				if(value == EMPTY){
					return(EMPTY);
				}
				else{ //The player moved the snake to a MARGIN character. Game Over!
					return(GAMEOVER);
				}
			}
		}
		else{
			printf("ERROR\n");
			return(ERROR); //if new != {UP || DOWN || LEFT || RIGHT || '\0'} we have an error
		}
	}
	return(c);
}

void print(snake_T table[HEIGHT+2][WIDTH+3]){
	int i,j;
	
	printf("Move the snake with the arrows. Press any other key to exit game!\n");
	for(i=0;i<HEIGHT+2;i++){
		for(j=0;j<WIDTH+3;j++){
			if(i>0 && i<HEIGHT+1 && j>=0 && j<WIDTH+1)
				printf("%c ",table[i][j].value); // (I print a space char between all the characters in the lines of the table)
			else if((i==0 || i==HEIGHT+1) && j<WIDTH)
				printf("%c=",table[i][j].value);
			else
				printf("%c",table[i][j].value);
		}
	}
	printf("Score: %d\n",score);
}

//This function reads any key typed by the user, without the need for him to press enter, and returns a specific integer for that key.
int getch(void) { 
	int c=0;

	struct termios org_opts, new_opts;
	int res=0;
	//Store old settings
	res=tcgetattr(STDIN_FILENO, &org_opts);
	assert(res==0);
	
	//Set new terminal parms
	memcpy(&new_opts, &org_opts, sizeof(new_opts));
	new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
	c=getchar();
	
	//Restore old settings
	res=tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);
	assert(res==0);
	
	//When I press the arrows, the getch() function returns one of 3 integers each time called,
	// first 27, then 91, then UP_ARROW=65 or DOWN_ARROW=66 or RIGHT_ARROW=67 or LEFT_ARROW=68
	if( c != 27 && c != 91 && c != UP_ARROW && c != DOWN_ARROW && c != RIGHT_ARROW && c != LEFT_ARROW ){
		c=-1;
	}
	return(c);
}
