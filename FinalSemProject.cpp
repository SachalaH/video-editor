#include <opencv2/opencv.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <tinyfiledialogs/tinyfiledialogs.h>
#include <filesystem>
#include <string>
#include <iomanip>
#include <sstream>

// Constants
GLFWwindow* window;
cv::VideoCapture cap;
cv::Mat frame;
bool isPlaying = false;
bool videoLoaded = false;
std::string selectedVideoPath = "";
double videoDuration = 0.0;  // in seconds
long long fileSize = 0;      // in bytes

// Start and End time (editable)
float startTime = 0.0f;      // in seconds
float endTime = 0.0f;        // in seconds

// Function declarations
bool initialize();
void renderFrame(const cv::Mat& frame);
bool loadVideo(const char* filePath);
std::string formatTime(float timeInSeconds);
std::string formatSize(long long sizeInBytes);

int main() {
    // Initialize everything once
    if (!initialize()) {
        return -1;
    }

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Decode the next frame if playing and video is loaded
        if (videoLoaded && isPlaying && cap.read(frame)) {
            // Frame rendering happens below
        }
        else if (!isPlaying && videoLoaded) {
            cap.set(cv::CAP_PROP_POS_FRAMES, 0); // Restart video when paused
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main ImGui Window for UI Controls
        ImGui::Begin("Video Editor");

        // Import Video Button
        if (!videoLoaded && ImGui::Button("Import Video")) {
            // Open file dialog to select video
            const char* filterPatterns[2] = { "*.mp4", "*.avi" };
            const char* filePath = tinyfd_openFileDialog("Select a Video File", "", 2, filterPatterns, NULL, 0);

            if (filePath) {
                selectedVideoPath = filePath;
                videoLoaded = loadVideo(filePath);  // Load video and update metadata

                if (!videoLoaded) {
                    std::cerr << "Error: Unable to open selected video file!" << std::endl;
                }
            }
        }

        // Display video controls and frame rendering if video is loaded
        if (videoLoaded) {
            ImGui::Text("Selected Video: %s", selectedVideoPath.c_str());

            // Display Video Metadata
            ImGui::Text("Duration: %s", formatTime(videoDuration).c_str());
            ImGui::Text("File Size: %s", formatSize(fileSize).c_str());

            // Start Time and End Time Inputs (Editable)
            ImGui::InputFloat("Start Time (seconds)", &startTime, 1.0f, 5.0f, "%.2f");
            ImGui::InputFloat("End Time (seconds)", &endTime, 1.0f, 5.0f, "%.2f");

            // Play/Pause Button
            if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
                isPlaying = !isPlaying;
            }

            // Video Display in Sub-Window
            ImGui::BeginChild("VideoDisplay", ImVec2(640, 480), true);
            if (!frame.empty()) {
                renderFrame(frame);  // Render the frame within the child window area
            }
            ImGui::EndChild();
        }

        ImGui::End(); // End main window

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers for display
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

// Function to initialize ImGui, GLFW, and OpenGL once
bool initialize() {
    // Initialize GLFW and create main window
    if (!glfwInit()) {
        return false;
    }

    window = glfwCreateWindow(1280, 720, "Video Player", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glViewport(0, 0, 1280, 720);

    // Initialize ImGui context and attach to GLFW and OpenGL
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    return true;
}

// Function to load video, calculate duration, and get file size
bool loadVideo(const char* filePath) {
    cap.open(filePath);
    if (!cap.isOpened()) {
        return false;
    }

    // Calculate video duration in seconds
    double fps = cap.get(cv::CAP_PROP_FPS);
    int frameCount = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    videoDuration = frameCount / fps;

    // Set default end time to video duration
    startTime = 0.0f;
    endTime = static_cast<float>(videoDuration);

    // Get file size
    fileSize = std::filesystem::file_size(filePath);

    return true;
}

// Function to render frame as OpenGL texture
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
    ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2(640, 480));
}

// Utility function to format time as mm:ss
std::string formatTime(float timeInSeconds) {
    int minutes = static_cast<int>(timeInSeconds) / 60;
    int seconds = static_cast<int>(timeInSeconds) % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << minutes << ":"
        << std::setw(2) << std::setfill('0') << seconds;
    return oss.str();
}

// Utility function to format file size
std::string formatSize(long long sizeInBytes) {
    const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
    int suffixIndex = 0;
    double size = static_cast<double>(sizeInBytes);

    while (size > 1024 && suffixIndex < 4) {
        size /= 1024;
        suffixIndex++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << suffixes[suffixIndex];
    return oss.str();
}
