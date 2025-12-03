#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "game.h"
#include "resource_manager.h"

#include <iostream>
#include <fstream>
#include <limits>

// GLFW function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// The Width of the screen
unsigned int SCREEN_WIDTH = 800;
// The height of the screen
unsigned int SCREEN_HEIGHT = 600;
bool fullScreen = false;
bool fPressed = false;



Game *Anti = new Game(SCREEN_WIDTH, SCREEN_HEIGHT);

int main(int argc, char *argv[])
{
    // INICJALIZACJA PLANSZY I GRACZY
    std::ifstream file("config.txt");
    if (!file.is_open())
    {
        file.open("Antiyoy/config.txt");
    }
    if (!file.is_open())
    {
        std::cout << "Cannot open config.txt\n";
        getchar();
        return 1;
    }

    GameConfigData gcd;
    std::string pythonProgram;
    std::string ipAddress;
    int port;
    int discoveryPort;

    bool shouldRunAi = false;
    uint8 networkPlayers = 0;

    if(!(file >> gcd.x >> gcd.y >> gcd.seed >> gcd.minProvinceSize >> gcd.maxProvinceSize >> gcd.playerMarkers))
    {
        std::string net;
    #ifdef _WIN32
        file.close();
        file.open("config.txt");
        if (!file.is_open())
        {
            file.open("Antiyoy/config.txt");
        }
        if (!file.is_open())
        {
            std::cout << "Cannot open config.txt\n";
            getchar();
            return 1;
        }
    #else
        file.clear();
        file.seekg(0, std::ios::beg);
    #endif
        if((file >> net) && net == "net") // Jeśli w pliku jest "net", i port discovery to łączymy się z inną grą
        {
            if(!(file >> discoveryPort))
            {
                std::cout << "Invalid content of config.txt\n";
                getchar();
                return 1;
            }
            //std::cout << "Discovery port: " << discoveryPort << '\n';
            std::cout << "Searching for a server...\n";
            searchForServer(discoveryPort, &ipAddress, &port);

            std::cout << "IP: " << ipAddress << ", Port: " << port << '\n';
            std::cout << "Server found, connecting...\n";
            if(!connectToServer(ipAddress, port))
            {
                std::cout << "Failed to connect to the server\n";
                getchar();
                return 1;
            }

            std::cout << "Connected, awaiting connection confirmation...\n";
            if(!receiveMagicNumbers(sock, true))
            {
                std::cout << "Confirmation failed\n";
                getchar();
                closeSockets();
                return 1;
            }

            std::cout << "Confirmation received, awaiting configuration data...\n";
            if(!gcd.receiveFromSocket(sock, true))
            {
                std::cout << "Configuration failed\n";
                getchar();
                closeSockets();
                return 1;
            }
            std::cout << "Successfully configured!\n";

            goto skipFileConfiguration;
        }
        else
        {
            std::cout << "Invalid content of config.txt\n";
            getchar();
            return 1;
        }
    }

    if(gcd.x < 1 || gcd.y < 1)
    {
        std::cout << "X and Y need to be greater than 0\n";
        getchar();
        return 1;
    }
    if(gcd.minProvinceSize < 2 || gcd.maxProvinceSize < 2)
    {
        std::cout << "Min and Max province size need to be at least 2\n";
        getchar();
        return 1;
    }
    if(gcd.playerMarkers.length() < 2)
    {
        std::cout << "At least 2 players required\n";
        getchar();
        return 1;
    }

    gcd.maxMoveTimes.reserve(gcd.playerMarkers.length());
    for(int i = 0; i < gcd.playerMarkers.length(); i++)
    {
        if(gcd.playerMarkers[i] == 'B') shouldRunAi = true;
        if(gcd.playerMarkers[i] == 'N') networkPlayers++;
        else if(gcd.playerMarkers[i] != 'L' && gcd.playerMarkers[i] != 'B' && gcd.playerMarkers[i] != 'N')
        {
            std::cout << "Unidentified player markers in config.txt\n";
            getchar();
            return 1;
        }
        int maxMoveTime;
        if(!(file >> maxMoveTime) || maxMoveTime < -1)
        {
            std::cout << "Invalid content of config.txt\n";
            getchar();
            return 1;
        }
        gcd.maxMoveTimes.push_back(maxMoveTime);
    }

    if(!(file >> port >> pythonProgram >> ipAddress >> discoveryPort))
    {
        std::cout << "Invalid content of config.txt\n";
        getchar();
        return 1;
    }

    // SOCKETY
    if(shouldRunAi || networkPlayers)
    {
        initializeSocket(port);
        if(sock == -1)
        {
            std::cout << "Socket initialization failed, communication impossible\n";
            getchar();
            return 1;
        }

        if(shouldRunAi)
        {
            int oldSize = clientSocks.size();
            std::cout << "Awaiting Python client...\n";
            runAi(pythonProgram, ipAddress, port);
            if(clientSocks.size() <= oldSize)
            {
                std::cout << "Socket client initialization failed, communication impossible\n";
                getchar();
                return false;
            }
            std::cout << "Python client connected!\n";
        }

        if(networkPlayers)
        {
            std::cout << "Searching for players...\n";
            for(int i = 0; i < networkPlayers; i++)
            {
                searchForSocketClient(discoveryPort, port);
                std::cout << "Player found!\n";
            }
            std::cout << "All players found!\n";
        }

        sendMagicNumbers();
    }

skipFileConfiguration: // Dla gracza sieciowego

    // OPENGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, false);

    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Anti", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    
    glfwSwapInterval(1); // FPS zsynchronizowany z odświeżaniem monitora (zazwyczaj 60 FPS)

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);


    // OpenGL configuration
    // --------------------
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // initialize game
    // ---------------
    Anti->Init(gcd);

    // deltaTime variables
    // -------------------
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        // calculate delta time
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        glfwPollEvents();

        // manage user input
        // -----------------
        Anti->ProcessInput(deltaTime);

        // update game state
        // -----------------
        Anti->Update(deltaTime);

        // render
        // ------
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Anti->Render();

        glfwSwapBuffers(window);
    }

    // delete all resources as loaded using the resource manager
    // ---------------------------------------------------------
    ResourceManager::Clear();
    delete Anti;
    closeSockets();
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    // when a user presses the escape key, we set the WindowShouldClose property to true, closing the application
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (key == GLFW_KEY_F && action == GLFW_PRESS && !fullScreen && !fPressed)
    {
        glfwSetWindowMonitor(window,glfwGetPrimaryMonitor(),0,0,glfwGetVideoMode(glfwGetPrimaryMonitor())->width,glfwGetVideoMode(glfwGetPrimaryMonitor())->height,GLFW_DONT_CARE);
        fPressed = true;
    }
    else if (key == GLFW_KEY_F && action == GLFW_RELEASE && !fullScreen && fPressed){fPressed=false;fullScreen=true;}
    else if (key == GLFW_KEY_F && action == GLFW_PRESS && fullScreen && !fPressed)
    {
        glfwSetWindowMonitor(window,NULL,0,0,SCREEN_WIDTH,SCREEN_HEIGHT,GLFW_DONT_CARE);
        fPressed = true;
    }
    else if (key == GLFW_KEY_F && action == GLFW_RELEASE && fullScreen && fPressed){fullScreen=false;fPressed=false;}
    // std::cout << "PRESSED: " << fPressed << " FULLSCREEN: " << fullScreen << std::endl;
    if (key > 0 && key < 1024)
    {
        if (action == GLFW_PRESS)
        {
            if (Anti->pressedKey==-1)
                Anti->pressedKey = key;
            if (Anti->clickedMovingKeys.contains(key))
            {
                Anti->clickedMovingKeys[key]=true;
            }
        }
        else if (action == GLFW_RELEASE)
        {
            Anti->pressedKey = -1;
            if (Anti->clickedMovingKeys.contains(key))
            {
                Anti->clickedMovingKeys[key]=false;
            }
        }

    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS){
        Anti -> mousePressed = true;
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        Anti -> cursorPosX = xpos;
        Anti -> cursorPosY = ypos;
    }
    // else if(action == GLFW_RELEASE){
    //     Anti -> mousePressed = false;
    // }
        
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    if (width!=Anti->Width || height!=Anti->Height)
    {
        glViewport(0, 0, width, height);
        Anti->Resize(width, height);
    }

}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // std::cout << "SCROLL: " << xoffset << " " << yoffset << std::endl;
    if(yoffset==-1)
    {
        Anti->scroll = -1;
    }
    else if(yoffset==1)
    {
        Anti->scroll = 1;
    }
}
