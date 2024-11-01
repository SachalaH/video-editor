#include <opencv2/opencv.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <tinyfiledialogs/tinyfiledialogs.h>

// Fixed path to a video file
//const char* videoPath = "C:\\Users\\admin\\Downloads\\walk.mp4";

// Function declarations
void renderFrame(const cv::Mat& frame);
cv::VideoCapture openVideo(const char* path);

// Globals
cv::VideoCapture cap;
cv::Mat frame;
bool isPlaying = false;
bool videoLoaded = false;
std::string selectedVideoPath = "";

// Main function
int main() {
    // 1. Initialize GLFW and OpenGL context
    if (!glfwInit()) {
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Video Player", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glViewport(0, 0, 1280, 720);

    // 2. Initialize ImGui
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // 3. Open the video file
    cv::VideoCapture cap = openVideo(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Error: Unable to open video file!" << std::endl;
        return -1;
    }

    bool isPlaying = false;
    cv::Mat frame;

    // Main loop
    //ImGui::BeginChild("VideoDisplay", ImVec2(640, 480), true);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // 4. Decode next frame if playing
        if (isPlaying && cap.read(frame)) {
            //renderFrame(frame); // Render the decoded frame
        }
        else if (!isPlaying) {
            // Optional: handle pause logic here if needed
        }
        else {
            cap.set(cv::CAP_PROP_POS_FRAMES, 0); // Restart video
        }

        // 5. Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main ImGui Window for Controls and Video Display
        ImGui::Begin("Video Editor");


        // 6. ImGui Play/Pause button
        if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
            isPlaying = !isPlaying;
        }

        // Sub-Window for Video Display
        ImGui::BeginChild("VideoDisplay", ImVec2(640, 480), true);
        {
            if (!frame.empty()) {
                // Render the frame within the child window area
                renderFrame(frame);
            }
        }
        ImGui::EndChild();

        ImGui::End();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// Function to open video using OpenCV
cv::VideoCapture openVideo(const char* path) {
    cv::VideoCapture cap(path);
    if (!cap.isOpened()) {
        std::cerr << "Error opening video file!" << std::endl;
    }
    return cap;
}

// Function to render frame as OpenGL texture
//void renderFrame(const cv::Mat& frame) {
//    // Convert BGR frame to RGB (OpenCV uses BGR by default)
//    cv::Mat rgbFrame;
//    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
//
//    // Generate and bind texture
//    GLuint texture;
//    glGenTextures(1, &texture);
//    glBindTexture(GL_TEXTURE_2D, texture);
//
//    // Set texture parameters
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//    // Upload frame data to texture
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbFrame.cols, rgbFrame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
//
//    // Render texture to the screen
//    /*glEnable(GL_TEXTURE_2D);
//    glBindTexture(GL_TEXTURE_2D, texture);
//    glBegin(GL_QUADS);
//    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
//    glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, -1.0f);
//    glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 1.0f);
//    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
//    glEnd();
//    glDisable(GL_TEXTURE_2D);*/
//    // Render texture in the area reserved for video in ImGui
//    ImGui::Image((void *)(intptr_t)texture, ImVec2(640, 480)); // Render the texture in the ImGui child window
//
//    // Cleanup
//    glDeleteTextures(1, &texture);
//}

void renderFrame(const cv::Mat& frame) {
    static GLuint texture = 0;

    // If the texture hasn't been created, generate it once
    if (texture == 0) {
        glGenTextures(1, &texture);
    }

    // Convert BGR frame to RGB (OpenCV uses BGR by default)
    cv::Mat rgbFrame;
    cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);

    // Bind texture and upload frame data
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbFrame.cols, rgbFrame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);

    // Render the texture in the ImGui child window
    ImGui::Image((ImTextureID)texture, ImVec2(640, 480));  // Direct cast to ImTextureID

    // No need to delete texture here; it stays allocated while the program runs
}


