/* main.c

   This file written 2024 by Artur Podobas and Pedro Antunes

   For copyright and licensing, see file COPYING 
   
   File further edited by Adam Carlström and Arvid Wilhelmsson */


/* Below functions are external and found in other files. */

//#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

extern void print(const char*);
extern void print_dec(unsigned int);
extern void display_string(char*);
extern void time2string(char*,int);
extern void tick(int*);
extern void delay(int);
extern int nextprime( int );
extern void enable_interrupts(void);

void set_leds(int led_mask);
void set_displays(int display_number, int value);
int get_sw(void);
int get_btn(void);
void display_time(int mytime);

#define BOARD_SIZE 16

#define VGA_BASE 0x08000000
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define CELL_WIDTH (SCREEN_WIDTH / BOARD_SIZE)
#define CELL_HEIGHT (SCREEN_HEIGHT / BOARD_SIZE)


// Structs used for the snake

// Position struct to hold row and column
typedef struct {
    int row;
    int col;
} Position;

// Queue to hold the snake's body segments
typedef struct {
    Position segments[BOARD_SIZE * BOARD_SIZE];
    int head;
    int tail;
    int length;
    bool right;
    bool left;
    int direction;
    bool snake_playing;
} Snake;


// Global variables
// mostly necessary as they are used in handle_interrupt and other functions simultaneosly 
int seed = 0;
int timeoutcount = 0;
bool changeDirection = false;
bool speedup = false;

/** 
 * Below is the function that will be called when an interrupt is triggered. 
* @author Adam Carlström
* @author Arvid Wilhelmsson
* @arg cause, holds an integer value directly connected to what caused the interrupt
* In this case cause 16 is called because of the timer (see labinit)
* Cause 17 is for switches
*/
void handle_interrupt(unsigned cause) 
{
  if (cause == 16){
  volatile unsigned short *status_reg = (unsigned short*) 0x04000020;
  *status_reg = 0; // reset to 0 so that it doesn't continously call interrupts
  timeoutcount+=2; // increase global timer
  if(speedup) // if speedup is enabled increase timer again so it basically goes 2x speed
    timeoutcount+=2;
  }

  if(cause == 17) {// called every time a switch is changed
    volatile unsigned short *switch_reg_edge = (unsigned short*) 0x0400001C;
    *switch_reg_edge = 0; // reset so it doesn't continously call interrupts
    
    changeDirection = true; // change global value so that game knows that switches have changed
  }
}

/**
* @author Arvid Wilhelmsson
* @arg row, the row where something should be drawn
* @arg col, the column where something should be drawn
* @arg color, the color something should be drawn in
* Function is used to draw rectangles on certain coordinates for the board via the VGA
* Additionally it draws a border around each cell
 */
void draw_cell(int row, int col, int color) {
    volatile char *vga = (volatile char *) VGA_BASE;

    // Define cell boundaries
    int start_y = row * CELL_HEIGHT;
    int end_y = start_y + CELL_HEIGHT;
    int start_x = col * CELL_WIDTH;
    int end_x = start_x + CELL_WIDTH;

    // Border thickness (1 pixel)
    int border_color = 0xFFFFFF;  // Black or any other color
    int space = 2; // space between dots for border
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            // Draw border: top, bottom, left, right
            if ((y == start_y || y == end_y - 1 || x == start_x || x == end_x - 1) && (y%space == 0 && x%space == 0)) {
                vga[y * SCREEN_WIDTH + x] = (char) border_color;
            } else {
                // Draw the cell's main color inside the border
                vga[y * SCREEN_WIDTH + x] = (char) color;
            }
        }
    }
}
 /**
  * @author Arvid Wilhelmsson
  * @arg board[], the board containing information about where the snake and food is
  * @arg *snake, contains information about the snake (see snake struct)
  * This function is used to go through each part of the board
  * and check what is on there to determine what color the VGA should
  * draw on this space
  */
void render_board(int board[BOARD_SIZE][BOARD_SIZE], Snake *snake) {
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            int color = 0; // Default color (e.g., empty cell)
            if (board[row][col] == 1) {// meaning a snake part is here
              //overly complex if-state to check if this part of the snake is the head
              if(snake->segments[snake->head].row == row && snake->segments[snake->head].col == col){
                color =0x123456; // head color (blue-ish)
              }else{
                color = 0x654321; // Snake body color (white)
              }
            } else if (board[row][col] == 2) { // meaning a fruit is here
                color = 0x2B2DCC; // Fruit color (Orange-ish)
            }
            draw_cell(row, col, color);
        }
    }
}
 /**
  * @author Arvid Wilhelmsson
  * @arg color, the color something should be drawn
  * Used to clear the VGA screen by making every pixel the same color
  * CURRENTLY UNUSED (caused flickering and was found to be unecessary)
  */
void clear_screen(int color) {
    volatile char *vga = (volatile char *) VGA_BASE;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        vga[i] = (char) color;
    }
}

/**
 * @author Adam Carlström (copied by)
 * @arg seed, used to determine randomness
 * @return a (pseudo) random integer value
 * This code was found in a discussion on the Dtek canvas page created by Albin Sijmer
 * The code was created by Natan Odin Herman Hyötyläinen and further altered by Fredrik Lundevall
 */
unsigned int random_value(unsigned int* seed) {
  static int hasbeencalled = 0; /* flag */
  static unsigned int state;
  if( !hasbeencalled ) {
    hasbeencalled = 1;
    state = *seed; /* the pointer seed is only used once */ 
  }
  /* actual pseudo-random number generation starts here */
  state = state * 747796405 + 2891336453;
  unsigned int result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
  result = (result >> 22) ^ result;
  return result;
}

/**
 * @author Adam Carlström and Arvid Wilhelmsson
 * This is the same function used to solve Lab3 for the dtek course
 * Here certain registers have their interrupts enabled which is used
 * in the handle_interrupt function found further up
 * The general timer is also set to call timeouts every 100ms (10 per second)
 */
void labinit(void)
{
  //0x04000020-0x0400003F
  //0001000111100001 1010001100000000

  //periodl 0x04000028
  volatile unsigned short *control_reg = (unsigned short*) 0x04000024;
  volatile unsigned short *periodl = (unsigned short*) 0x04000028;
  volatile unsigned short *periodh = (unsigned short*) 0x0400002C;

  *(periodl) = (29999999/10) & 0xFFFF;  
  *(periodh) = (29999999/10) >> 16;
  
  *(control_reg) = 0x7;

  volatile unsigned short *switch_reg_interrupt = (unsigned short*) 0x04000018;

  *switch_reg_interrupt = 0b1111111111;

  enable_interrupts();
}

/**
 * @author Adam Carlström and Arvid Wilhelmsson
 * @arg led_mask, is an integer to determine which leds should be turned ON/OFF
 * This is the same function that is used in Lab3 for the Dtek course
 */
void set_leds(int led_mask){
  int lsb = led_mask & 0x3FF;
  volatile int *leds_address = (volatile int*) 0x04000000;
  // 0x04000000
  *leds_address = lsb;
}
/**
 * @author Adam Carlström and Arvid Wilhelmsson
 * @arg display_number, is the number for the display this value is to be shown at
 * @arg value, is the value 0-9 to be shown on the display
 * This is the same function that is used in Lab3 for the Dtek course
 */
void set_displays(int display_number, int value){//mellan 0-9

  //int lsb = display_number & 0x7F;
  int binary_value = 0;
  switch(value){
    case 0:
      binary_value = 0b11000000;
      break;
    case 1:
      binary_value = 0b11111001;
      break;
    case 2:
      binary_value = 0b10100100;
      break;
    case 3:
      binary_value = 0b10110000;
      break;
    case 4:
      binary_value = 0b10011001;
      break;
    case 5:
      binary_value = 0b10010010;
      break;
    case 6:
      binary_value = 0b10000010;
      break;
    case 7:
      binary_value = 0b11111000;
      break;
    case 8:
      binary_value = 0b10000000;
      break;
    case 9:
      binary_value = 0b10011000;
      break;
    default:
      binary_value = 0b11111111;//if its broken, show all leds turned on
      break;
  }
    
  volatile int *display_address = (volatile int*) (0x04000050 + 0x10 *display_number);
  *display_address = binary_value;
}

/**
 * @author Adam Carlström and Arvid Wilhelmsson
 * This is the same function that is used in Lab3 for the Dtek course
 * The function is used to get the value for all switches
 */
int get_sw(void){
  //0x04000010
  volatile int *toggle_address = (volatile int*) (0x04000010);
  return ((*toggle_address) & 0x3FF);
}

/**
 * @author Adam Carlström and Arvid Wilhelmsson
 * This is the same function that is used in Lab3 for the Dtek course
 * The function is used to get the value of the lower button
 */
int get_btn(void){
  //0x040000d0
  volatile int *btn_address = (volatile int*) (0x040000d0);
  return ((*btn_address) & 0x1);
}

/**
 * @author Adam Carlström
 * @arg snake, the variable holding the snake struct
 * @arg startrow, the starting row coordinate for the snake
 * @arg startcol, the starting column coordinate for the snake
 * @arg initalLength, the starting length for the snake
 * This function initializes all the values for the snake struct
 */
void initSnake(Snake *snake, int startRow, int startCol, int initialLength) {
    snake->head = initialLength - 1;
    snake->tail = 0;
    snake->length = initialLength;
    snake-> snake_playing = true;
    snake->right = false;
    snake->left = false;
    snake->direction = 1; // 0 = north, 1 = east, 2 = south, 3 = west

    // Populate the initial snake segments, starting horizontally from left to right
    for (int i = 0; i < initialLength; i++) {
        snake->segments[i] = (Position){startRow, startCol + i};
    }
}

/**
 * @author Adam Carlström
 * @arg Snake, the variable holding the snake struct
 * This function is called if the snake has died
 */
void gameOver(Snake *snake){
    print("YOU LOST \n");
    print("Press button to replay \n");
    snake->snake_playing = false;
    set_leds(2047);// 2^11-1 = all leds turned on
}

/**
 * @author Arvid Wilhelmsson
 * @arg Snake, the variable holding the snake struct
 * This function is called if the snake has a total length of 100
 * meaning it has filled the board and therefore won the game
 */
void gameWin(Snake *snake){
    print("You won \n");
    snake->snake_playing = false;
    set_leds(1365);// 2^10 + 2^8 + 2^6 + 2^4 + 2^2 + 2^0 = 1365 = every other led turned on
}

/**
 * @author Adam Carlström
 * @arg Snake, the variable holding the snake struct
 * @arg newhead, a position/coordinate for the new position of the head
 * Add a new head position to the snake
 */
void addHead(Snake *snake, Position newHead) {
    snake->head = (snake->head + 1) % (BOARD_SIZE * BOARD_SIZE);
    snake->segments[snake->head] = newHead;
    snake->length++;
}
/**
 * @author Adam Carlström
 * @arg Snake, the variable holding the snake struct
 * Remove the tail position of the snake
 */
void removeTail(Snake *snake) {
    snake->tail = (snake->tail + 1) % (BOARD_SIZE * BOARD_SIZE);
    snake->length--;
}

/**
 * @author Adam Carlström
 * @arg board, the board used for the game
 * The function makes sure new fruit spawns in a position that is empty
 */
void fruitSpawnRandom(int board[BOARD_SIZE][BOARD_SIZE]){
  int x = 0;
  int y = 0;
  do{
    unsigned int tmp = (unsigned int)seed;
    x = random_value(&tmp) % BOARD_SIZE;//random x value between 0 and 9
    y = random_value(&tmp) % BOARD_SIZE;//random y value between 0 and 9
  }while(board[x][y] != 0);
  board[x][y] = 2;//mat
}

/**
 * @author Adam Carlström
 * @arg snake, the variable holding the snake struct
 * @arg board, the board for this game
 * The function updates the position of the snake based on 
 * the direction it wants to go and updates its head and tail
 * correspondingly. Additionally it checks how this new position
 * might affect the game by checking collision with itself,
 * walls, and fruits. Also checks if the snake dies or wins.
 */
void moveSnake(Snake *snake, int board[BOARD_SIZE][BOARD_SIZE]) {
    int direction_rows = 0;
    int direction_columns = 0;
    switch(snake->direction){
        case 0://north
            direction_rows = -1;
            direction_columns = 0;
        break;
        case 1://east
            direction_rows = 0;
            direction_columns = 1;
        break;
        case 2://south
            direction_rows = 1;
            direction_columns = 0;
        break;
        case 3://west
            direction_rows = 0;
            direction_columns = -1;
        break;
        default: // in the case it gets here, reset directions to default
            snake->direction = 1; //east som default
            snake->right = false;//rakt fram som default
            snake->left = false;
            break;
    }

    Position newHead = {
        snake->segments[snake->head].row + direction_rows,
        snake->segments[snake->head].col + direction_columns
    };

    if (board[newHead.row][newHead.col] == 0) {// means snake is moving where nothing else is
        // Remove the tail if the snake isn't growing
        Position tailPos = snake->segments[snake->tail];
        board[tailPos.row][tailPos.col] = 0; // Clear tail position on board
        removeTail(snake);
    }else if(board[newHead.row][newHead.col] == 1){// means that the snake has moved into itself
        gameOver(snake);
    }else if(board[newHead.row][newHead.col] == 2){// means that fruit is found here 
        if(snake->length <= BOARD_SIZE*BOARD_SIZE-3){// only spawn fruit if there is space for it
          fruitSpawnRandom(board); // spawn new fruit so that there is always 3 of them
        }
        if(snake->length >= BOARD_SIZE*BOARD_SIZE){ // check win condition
          gameWin(snake);
        }
    }
    // Check collision with walls
    if(newHead.row >= BOARD_SIZE || newHead.row <= -1 || newHead.col >= BOARD_SIZE || newHead.col <= -1){//outofBounds
        gameOver(snake);
    }
    // Add the new head to the snake
    if(snake->snake_playing){
      addHead(snake, newHead);
      board[newHead.row][newHead.col] = 1; // Mark new head position on board
    }
}

/**
 * @author Adam Carlström
 * @arg right, boolean value to show if the user wants to turn right
 * @arg left, boolean value to show if the user wants to turn left
 * @arg currentDirection, holds the integer value for the current direction
 * @return an int for the direction it should go if it turns right/left from here
 */
int calculateDirectionChange(bool right,bool left, int currentDirection){
  int returnvalue = currentDirection;
  if(right && !left){
      returnvalue += 1;
      if(returnvalue >= 4){//if on west = 3 and wants to go right, go to north = 0
          return 0;
      }   
  }else if(!right && left){
      returnvalue -= 1;
      if(returnvalue <= -1){//if on north = 0 and wants to go left, go to west = 3
          return 3;
      }
  }
  return returnvalue;
}

/**
 * @author Adam Carlström
 * @arg snake, the variable holding the snake struct
 * @arg right, boolean value to show if the user wants to turn right
 * @arg left, boolean value to show if the user wants to turn left
 * Used to change the direction of the snake
 */
void changeDirectionSnake(Snake *snake, bool right, bool left){
    snake->right = right;
    snake->left = left;
    if(right && left){
        //no change = go straight
    }else{
      
      snake->direction = calculateDirectionChange(snake->right,snake->left, snake->direction);
    }
}

/**
 * @author Arvid Wilhelmsson
 * @arg board, the board of the game
 * Function to print the board (for debugging)
 */
void printBoard(int board[BOARD_SIZE][BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            print_dec(board[i][j]);
            print(" ");
        }
        print("\n");
    }
    print("\n");
}

/**
 * @author Adam Carlström
 * @arg snake, the variable holding the snake struct
 * @arg board, the board of the game
 * The function is used to initialize variables to start the game
 */
void startgame(Snake *snake, int board[BOARD_SIZE][BOARD_SIZE]){
  //print("Game Started \n");
  //board[x][y]
  int initialLength = 3;
  initSnake(snake, BOARD_SIZE/2, 1,initialLength);
  // Mark initial snake positions on the board
  for (int i = 0; i < initialLength; i++) {
      Position pos = snake->segments[i];
      board[pos.row][pos.col] = 1;
  }

  board[BOARD_SIZE/2+1][BOARD_SIZE-BOARD_SIZE/4-1] = 2;//mat
  board[BOARD_SIZE/2][BOARD_SIZE-BOARD_SIZE/4] = 2;//mat
  board[BOARD_SIZE/2-1][BOARD_SIZE-BOARD_SIZE/4-1] = 2;//mat
}

/**
 * @author Arvid Wilhelmsson (copied by)
 * @arg ptr
 * @arg value
 * @arg num
 * This code snippet was found on GeeksForGeeks.
 * Function is used to fill a block of memory with a specific value.
 * It replaces the contents of the first `num` bytes in the memory block 
 * pointed to by `ptr` with the constant byte value `value`.
 * 
 * @return The function returns a pointer to the memory block `ptr`.
 */

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*) ptr;
    for (size_t i = 0; i < num; i++) {
        p[i] = (unsigned char) value;
    }
    return ptr;
}

/**
 * @author Adam Carlström
 * arg snake
 * The function is used to update the score on the displays
 */
void updateScore(Snake *snake){
  for (int i = 1; i < 7; i++)
  {
    int div = 10;
    for (int j = 1; j < i; j++)
    {
      div *=10;
    }
    int rest = snake->length%div;
    int tot = rest / (div/10);
    set_displays(i-1,tot);
  }
}

/**
 * @author Adam Carlström and Arvid Wilhelmsson
 * This function is used to run the game with its loop 
 * continously going as long as the snake is alive
 */
void runGame(){
  set_leds(0);
  int board[BOARD_SIZE][BOARD_SIZE] = {0}; //globalize board
  Snake snake;

  // the snipped of code below is used to check the initial values
  // of the switches and if the snake should turn/speed up from start
  int switch_values = get_sw();
  int switchbits[3];
  int i = 0;
  while(i<3){
    switchbits[i] = (switch_values >> i) & 0b1;
    i++;
  }
  startgame(&snake,board); // initialise values for the game
  if(switchbits[0]){
    snake.right = true;
  }
  if(switchbits[1]){
    snake.left = true;
  }
  if(switchbits[2]){
    speedup = true;
  } else {
    speedup = false;
  }
  //print("before loop, ");
  // while loop that goes on as long as the snake is alive and playing
  while(snake.snake_playing){
    // This if statement checks if the user wants to change direction
    // which comes from the global variable that is changed upon a switch interrupt
    if(changeDirection){
      changeDirection = false;
      switch_values = get_sw();
      i = 0;
      while(i<3){
        switchbits[i] = (switch_values >> i) & 0b1;
        i++;
      }
      if(switchbits[1] == 1){
        snake.left = true;
      }else{
        snake.left = false;
      }
      if(switchbits[0] == 1){
        snake.right = true;
      }else{
        snake.right = false;
      }
      if(switchbits[2]){
        speedup = true;
      } else {
        speedup = false;
      }
      //showDirection(&snake, calculateDirectionChange(snake.right,snake.left,snake.direction));
    }

    // Update game logic
    print("");//print to delay - avoid race condition
    if (timeoutcount >= 10){
      changeDirectionSnake(&snake, snake.right,snake.left);
      //showDirection(&snake, snake.direction);
      moveSnake(&snake, board);
      timeoutcount=0;
      updateScore(&snake);
      render_board(board, &snake);

    }
  }
}

// main function called when running file
int main() {
  unsigned int tmp = (unsigned int) 1234567890;
  seed = random_value(&tmp);
  labinit();
  runGame();

  while(1){//go here after game is done
    if(get_btn()){// press button to play again
      runGame();
    }
  }
  return 0;
}