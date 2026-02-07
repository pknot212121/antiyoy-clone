# Antiyoy-Clone

## About
All fans of the game Antiyoy by Yiotro probably know how hard and punishing bots can be in that game.
This project is an attempt to check if these bots can be surpassed using Reinforcement Learning. We made a faithful copy of the game in OpenGL and C++ and then we used it as an enviroment to train bots using Q-learning.

## Accessibility

In the Antiyoy directory we included executables for both windows and linux (should also work for mac).
There is also a config.txt file there to change parameters of the game.

## Build

You can also build the game yourself. It should pretty simple with cmake with all dependencies already included in the source code.

## Controls

While inside the game you can use the following controls:

| Key     | Action                                |
|---------|---------------------------------------|
| WASD    | Controls to move through the game map |
| ENTER   | End the turn                          |
| SCROLL  | To change the size of the map         |
| F       | Toggle to fullscreen (off by default) |
| 1,2,3,4 | Place soliders of different tiers     |
| 5       | Place a farm                          |
| 6,7     | Place a towers of different tiers     |
| LMOUSE  | Choose a tile                         |


## Config