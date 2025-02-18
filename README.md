# Dtek Mini project
## Snake Game

The projekt we have made is a simple Snake Game using the RISC-V board. This means that the game follows the same simple rules as any other snake game. The two dimensional snake is therefore able to move in all four directions of a grid, can die by going into itself and going into walls, and increase in length upon eating fruits.

In order to control the snake you use the two switches furthest to the right to control which direction
the snake should go. This is done by making these switches make the snake either turn right or left, which is connected to the right switch and the left switch. If both of these switches are up, then the snake will go forwards. By having a switch up it means that it will continue to turn until it is turned down.

Another functionality that has been added to this game is the feature to challenge yourself by increasing the speed that the game is played at. This is done by turning the third most switch from the right up. 

In order to run the game, enter the directiory called 'FungerandeSnake'. Since the game uses VGA to show progress, it is required for it to be connected to a monitor in order to be able to see the game. To start running begin by turn on jtagd in the terminal by writing:
- jtagd --user-start

and then compile the game by simply writing:
- make

before finally running the game by writing:
- dtekv-run main.bin

This starts the game immediately.

Upon death, the game can be restarted by pressing the second button, the one below the reset button

## Without board

To play the game without the RISC-V board you still need to compile the game using make, however you can run the game by inserting the main.bin file in the following website:

https://dtekv.fritiof.dev/

### By Adam Carlström och Arvid Wilhelmsson
