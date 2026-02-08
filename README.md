# Antiyoy-Clone

## About
All fans of the game [Antiyoy](https://github.com/yiotro/Antiyoy) by Yiotro probably know how hard and punishing bots can be in that game.
This project is an attempt to check if these bots can be surpassed using Reinforcement Learning. We made a faithful copy of the game in OpenGL and C++ and then we used it as an enviroment to train bots using Q-learning. We also added LAN multiplayer.

## Setup guide
Download the Antiyoy directory from the releases section for [windows](https://github.com/pknot212121/antiyoy-clone/releases/tag/windows) or [linux](https://github.com/pknot212121/antiyoy-clone/releases/tag/linux) , unpack it and activate the executable inside. There shoul be also a [config](Antiyoy/config.txt) file there to modify parameters of the game.

## Build

You can also build the game yourself. It should be pretty simple with [cmake](CMakeLists.txt) with all dependencies already included in the source code. The game has been only tested on g++ and MinGW and may not work properly on other compilers.

## Controls

While inside the game you can use the following controls:

| Key     | Action                                |
|---------|---------------------------------------|
| WASD    | Controls to move through the game map |
| ENTER   | End the turn                          |
| SCROLL  | To change the size of the map         |
| 1,2,3,4 | Place soliders of different tiers     |
| 5       | Place a farm                          |
| 6,7     | Place a towers of different tiers     |
| LMOUSE  | Choose a tile                         |


## Config

There are two configuration templates:
### Normal
Where the parameters go as:
```
map_width map_height
seed
min_province_size max_province_size
player_markers
max_turn_times
tcp_port
program_python program_ip
port_discovery
```
- The values are separated by whitespaces but it is recommended to use structure such as above.
- All values in lines 1-3 will be random if set to 0.
- Player markers are letters meaning the player type. Available player types are:
  - L (local player)
  - B (AI bot)
  - N (network player)
- If only one marker is inputed (excluding N) at the start of the game 2-8 players of this type will be created.
- Number of max time counts **must be the same** as the number of markers.
- -1 if inputted as max time means that the time is infinite.

Example:
```
5 5
0
2 3
LLB
-1 -1 10
2137
receiver 127.0.0.1
2138
```

### Net
```
"net"
port_discovery
```
- Allows to connect a network player to a game hosted in local network.
- Port Discovery **must be the same** as in host's config.
- Rest of the configuration is taken from host.

Example
```
net
2138
```
