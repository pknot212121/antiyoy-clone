#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "game.h"
#include "resource_manager.h"

#include <iostream>
#include <fstream>

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

    coord x, y;
    int seed;
    std::string playerMarkers;
    std::vector<int> maxMoveTimes;
    std::string pythonProgram;
    std::string ipAddress;
    int port;
    int discoveryPort;

    bool shouldRunAI = false;
    bool shouldNetwork = false;

    if(!(file >> x >> y >> seed >> playerMarkers))
    {
        std::string net;
        file.clear();
        file.seekg(0, std::ios::beg);
        if(!(file >> net) && net == "net") // Jeśli w pliku jest "net", i port discovery to łączymy się z inną grą
        {
            if(!(file >> discoveryPort))
            {
                std::cout << "Invalid content of config.txt\n";
                getchar();
                return 1;
            }
            std::cout << "Searching for a match...\n";
            searchForServer(discoveryPort, &ipAddress, &port);
            std::cout << "Match found, awaiting configuration data...\n";

            goto skipFileConfiguration;
        }
        else
        {
            std::cout << "Invalid content of config.txt\n";
            getchar();
            return 1;
        }
    }

    maxMoveTimes.reserve(playerMarkers.length());
    for(int i = 0; i < playerMarkers.length(); i++)
    {
        if(playerMarkers[i] == 'B') shouldRunAI = true;
        else if(playerMarkers[i] != 'L' && playerMarkers[i] != 'B')
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
        maxMoveTimes.push_back(maxMoveTime);
    }

    if(!(file >> pythonProgram >> ipAddress >> port >> discoveryPort))
    {
        std::cout << "Invalid content of config.txt\n";
        getchar();
        return 1;
    }

    // SOCKETY
    if(shouldRunAI || shouldNetwork)
    {
        initializeSocket(port);
        if(sock == -1)
        {
            std::cout << "Socket initialization failed, communication impossible\n";
            getchar();
            return 1;
        }

        if(shouldRunAI)
        {
        #ifdef _WIN32
            std::string cmd = "start python \"" + pythonProgram + ".py\" " + ipAddress + " " + std::to_string(port);
        #else
            std::string cmd = "xterm -geometry 100x30 -e \"python3 " + pythonProgram + ".py " + ipAddress + " " + std::to_string(port) + "\" &";
        #endif

            std::system(cmd.c_str());

            std::cout << "Awaiting Python client...\n";
            acceptSocketClient();
            if(invalidSocks())
            {
                std::cout << "Socket client initialization failed, communication impossible\n";
                getchar();
                return 1;
            }
            std::cout << "Python client connected!\n";
        }

        if(shouldNetwork)
        {
            
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
    Anti->Init(x, y, seed, playerMarkers, maxMoveTimes);

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
    if (key >= 0 && key < 1024)
    {
        if (action == GLFW_PRESS)
            Anti->Keys[key] = true;
        else if (action == GLFW_RELEASE)
            Anti->Keys[key] = false;
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
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
    Anti->Resize(width, height);
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
