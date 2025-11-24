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
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, false);

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

    if(!(file >> x >> y >> seed >> playerMarkers))
    {
        std::cout << "Invalid content of config.txt\n";
        getchar();
        return 1;
    }

    maxMoveTimes.reserve(playerMarkers.length());
    for(int i = 0; i < playerMarkers.length(); i++)
    {
        if(playerMarkers[i] != 'L' && playerMarkers[i] != 'B')
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
