#include <windows.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "tinyfiledialogs/tinyfiledialogs.h"
#include <iostream>
#include <string>
#include "opencv2/opencv.hpp"
#include <filesystem>
#include <sstream>
#include <libavutil/imgutils.h>
#include <thread>
#include <atomic>
#include "playback.h"
// Include necessary FFmpeg libraries
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace fs = std::filesystem;

// State management for UI
enum class UIState {
    MainMenu,
    ImportVideo,
    MergeVideos,
    InsertAd
};
UIState uiState = UIState::MainMenu;

// Data structure for storing video file paths and assigned orders
struct VideoInfo {
    std::string path;
    int order;
};

// Data structure for storing video paths and roles
struct AdVideoInfo {
    std::string path;
    bool isMainVideo;  // True if it's the main video, false if it's the ad video
};

AdVideoInfo mainVideo;
AdVideoInfo adVideo;
bool isMainVideoSet = false;
bool isAdVideoSet = false;
int insertAfterSeconds = 0;  // Time in seconds after which the ad should be inserted
int durationOfAd = 0; // Time in seconds for the duration of the ad in the main video
bool insertAdError = false;  // Error flag to indicate invalid input

// Function prototypes for inserting an ad
void insertAdUI();
void addMainVideo(bool isMainVideo);
bool validateInsertAd();
void previewInsertAd();
void saveInsertAd();

void AttachConsoleOutput() {
    AllocConsole();
    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout); // Safe version
    freopen_s(&stream, "CONOUT$", "w", stderr); // Safe version
    std::cout << "Console attached!" << std::endl;
}


// Maximum number of videos to merge
const int maxVideos = 6;
std::vector<VideoInfo> videos;  // Stores selected videos and their order
bool orderError = false;  // Flag to track order uniqueness errors

// Function prototypes for merge videos
void mergeVideosUI();
void addVideo();
bool checkUniqueOrders();
void previewMergedVideo();
void saveMergedVideo();

// Video import information
static std::string videoPath = "";
static uintmax_t videoSize = 0;
static char startTime[16] = "00:00";
static char endTime[16] = "00:00";  // Updated dynamically after getting duration
static bool muteAudio = false;
static int selectedSpeedIndex = 2;  // Default to normal speed (1x)
static int selectedFilterIndex = 0;  // Default to no filter

// Speed and filter options
const float speedOptions[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f };
const char* speedLabels[] = { "0.5x", "0.75x", "1x", "1.25x", "1.5x"};
const char* filterLabels[] = { "No filter","Sepia", "Grayscale", "Edge Detection", "Blur"};

// Function prototypes for the button actions
void importVideo();
void mergeVideos();
void insertAd();
bool getVideoDuration(const std::string& path, char* endTimeStr, size_t endTimeSize);

// Atomic flag for play/pause functionality
std::atomic<bool> is_playing(true);

// function prototype for opening a video object using opencv
cv::VideoCapture openVideo(const std::string& filePath);
void processVideo(cv::VideoCapture& video, int startSeconds, int endSeconds, float speedMultiplier, const std::string& filter, const std::string& outputFile);



int getSecondsFromTimeString(const char* timeStr);


// Callback to handle window resizing
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    // Initialize FFmpeg for metadata reading
    //av_register_all();
    AttachConsoleOutput(); // Attach console at startup
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // Set up OpenGL version and profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(800, 600, "Video Editor - Main Menu", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // Set up Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Set up ImGui style and platform/renderer bindings
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Poll for and process events
        glfwPollEvents();

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // UI logic based on the current UI state
        if (uiState == UIState::MainMenu) {
            ImGui::Begin("Video Editor - Main Menu");

            if (ImGui::Button("Import a video")) {
                importVideo();
            }

            if (ImGui::Button("Merge videos (up to 6)")) {
                uiState = UIState::MergeVideos;
                videos.clear();  // Clear any previously selected videos
            }

            if (ImGui::Button("Insert an ad (main + ad video)")) {
                uiState = UIState::InsertAd;
                isMainVideoSet = false;
                isAdVideoSet = false;
                insertAfterSeconds = 0;
                durationOfAd = 0;
                insertAdError = false;
                
            }

            ImGui::End();
        }
        else if (uiState == UIState::InsertAd) {
            insertAdUI();
        }
        else if (uiState == UIState::ImportVideo) {
            ImGui::Begin("Video Editor - Import Video");

            // Display selected video path and size
            ImGui::Text("Selected Video: %s", videoPath.c_str());
            ImGui::Text("Video Size: %.2f MB", videoSize / (1024.0 * 1024.0));

            ImGui::Separator();

            // Start and end time input boxes
            //ImGui::InputText("Start Time (mm:ss)", startTime, IM_ARRAYSIZE(startTime));
            //ImGui::InputText("End Time (mm:ss)", endTime, IM_ARRAYSIZE(endTime), ImGuiInputTextFlags_ReadOnly);
             // Start and end time input boxes
             // Parse video duration in seconds for validation
            static int videoDurationSeconds = getSecondsFromTimeString(endTime);

            // Parse user input times
            int startSeconds = getSecondsFromTimeString(startTime);
            int endSeconds = getSecondsFromTimeString(endTime);

            // Clamp start and end times to valid ranges
            if (startSeconds < 0) startSeconds = 0;
            if (startSeconds >= videoDurationSeconds) startSeconds = videoDurationSeconds - 1;

            if (endSeconds <= startSeconds) endSeconds = startSeconds + 1;
            if (endSeconds > videoDurationSeconds) endSeconds = videoDurationSeconds;

            // Reflect corrected values in the UI
            snprintf(startTime, IM_ARRAYSIZE(startTime), "%02d:%02d", startSeconds / 60, startSeconds % 60);
            snprintf(endTime, IM_ARRAYSIZE(endTime), "%02d:%02d", endSeconds / 60, endSeconds % 60);

            // Start and end time input boxes
            ImGui::InputText("Start Time (mm:ss)", startTime, IM_ARRAYSIZE(startTime));
            ImGui::InputText("End Time (mm:ss)", endTime, IM_ARRAYSIZE(endTime));

            // Display validation message
            if (startSeconds < 0 || startSeconds >= videoDurationSeconds || endSeconds > videoDurationSeconds || endSeconds <= startSeconds) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: Invalid time range. Adjust start and end times.");
            }
            else {
                //ImGui::TextColored(ImVec4(0, 1, 0, 1), "Valid time range.");
            }

            ImGui::Separator();

            // Mute audio checkbox
            ImGui::Checkbox("Mute Audio", &muteAudio);

            // Speed selection radio buttons
            ImGui::Text("Select Speed:");
            for (int i = 0; i < IM_ARRAYSIZE(speedOptions); ++i) {
                ImGui::RadioButton(speedLabels[i], &selectedSpeedIndex, i);
            }

            // Filter selection radio buttons
            ImGui::Text("Select Filter:");
            for (int i = 0; i < IM_ARRAYSIZE(filterLabels); ++i) {
                ImGui::RadioButton(filterLabels[i], &selectedFilterIndex, i);
            }

            // Preview and Save buttons
            if (ImGui::Button("Preview")) {
                std::cout << "Previewing video: " << videoPath << "\n";
                
                // Add logic to preview the video here
                //startPreview(videoPath

                std::cout << "Previewing video with the following settings:\n";
                std::cout << "Start Time: " << startTime << " (" << startSeconds << " seconds)\n";
                std::cout << "End Time: " << endTime << " (" << endSeconds << " seconds)\n";

                // Print mute audio state
                std::cout << "Mute Audio: " << (muteAudio ? "Yes" : "No") << "\n";

                // Print selected speed
                std::cout << "Playback Speed: " << speedLabels[selectedSpeedIndex] << "\n";

                // Print selected filter
                std::cout << "Filter: " << filterLabels[selectedFilterIndex] << "\n";

                // open the video object 
                cv::VideoCapture video = openVideo(videoPath);
                // trim and apply the speed
                std::string outputFile = "output_processed.mp4";
                processVideo(video, startSeconds, endSeconds, speedOptions[selectedSpeedIndex], filterLabels[selectedFilterIndex], outputFile);
                // apply the relevant filters
                // check if audio is to be muted or no

                //Playback playback(videoPath);
                //playback.play();
                //break;
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                std::cout << "Saving video with options:\n";
                std::cout << "Start Time: " << startTime << "\n";
                std::cout << "End Time: " << endTime << "\n";
                std::cout << "Mute Audio: " << (muteAudio ? "Yes" : "No") << "\n";
                std::cout << "Speed: " << speedOptions[selectedSpeedIndex] << "x\n";
                std::cout << "Filter: " << filterLabels[selectedFilterIndex] << "\n";
                // Add logic to save the video here
                // Open a Save File dialog for the user to specify the output file path
                const char* filters[] = { "*.mp4", "*.avi", "*.mov" }; // Supported formats
                const char* outputPath = tinyfd_saveFileDialog("Save Processed Video", "output_video.mp4", 3, filters, "Video Files");

                if (outputPath) {
                    std::cout << "Saving video to: " << outputPath << "\n";

                    // Reprocess the video with the user's selected file path
                    try {
                        // open the video object 
                        cv::VideoCapture video = openVideo(videoPath);
                        processVideo(video, startSeconds, endSeconds, speedOptions[selectedSpeedIndex], filterLabels[selectedFilterIndex], outputPath);
                        std::cout << "Video successfully saved to: " << outputPath << "\n";
                    }
                    catch (const std::exception& ex) {
                        std::cerr << "Error: Failed to save video. Exception: " << ex.what() << "\n";
                    }
                }
                else {
                    std::cerr << "Save operation canceled or failed.\n";
                }
            }

            if (ImGui::Button("Back")) {
                uiState = UIState::MainMenu;
            }

            ImGui::End();
        }
        else if (uiState == UIState::MergeVideos) {
            mergeVideosUI();
        }
        // Render ImGui
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
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


// Function to open a video file and return a cv::VideoCapture object
cv::VideoCapture openVideo(const std::string& filePath) {
    cv::VideoCapture video(filePath); // Open the video file
    if (!video.isOpened()) {         // Check if the file was successfully opened
        throw std::runtime_error("Error: Could not open video file."); // Throw an exception on failure
    }
    return video; // Return the opened video object
}



// Function to handle video import using TinyDialogs
void importVideo() {
    const char* filters[] = { "*.mp4", "*.avi", "*.mov" };
    const char* path = tinyfd_openFileDialog("Select a video file", "", 3, filters, "Video Files", 0);

    if (path) {
        videoPath = path;
        videoSize = fs::file_size(videoPath);

        // Reset start and end time, and mute audio options
        strcpy_s(startTime, sizeof(startTime), "00:00");

        if (getVideoDuration(videoPath, endTime, sizeof(endTime))) {
            std::cout << "Video duration set to " << endTime << std::endl;
        }

        muteAudio = false;
        uiState = UIState::ImportVideo;
    }
    else {
        std::cerr << "No file selected or failed to open.\n";
        uiState = UIState::MainMenu;
    }
}

// Helper function to get video duration using FFmpeg
bool getVideoDuration(const std::string& path, char* endTimeStr, size_t endTimeSize) {
    AVFormatContext* fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, path.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Error: Could not open video file.\n";
        return false;
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        std::cerr << "Error: Could not retrieve stream info.\n";
        avformat_close_input(&fmtCtx);
        return false;
    }

    int64_t duration = fmtCtx->duration / AV_TIME_BASE;
    int minutes = duration / 60;
    int seconds = duration % 60;
    snprintf(endTimeStr, endTimeSize, "%02d:%02d", minutes, seconds);

    avformat_close_input(&fmtCtx);
    return true;
}


int getSecondsFromTimeString(const char* timeStr) {
    int minutes = 0, seconds = 0;
    if (sscanf_s(timeStr, "%d:%d", &minutes, &seconds) != 2) {
        return -1; // Invalid time format
    }
    if (minutes < 0 || seconds < 0 || seconds >= 60) {
        return -1; // Invalid range
    }
    return minutes * 60 + seconds;
}


// function to trim the video and apply the speed
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <string>

// Function to trim and change the speed of a video
// Function to process video: trim, change speed, and apply filter
void processVideo(cv::VideoCapture& video, int startSeconds, int endSeconds, float speedMultiplier, const std::string& filter, const std::string& outputFile) {
    if (!video.isOpened()) {
        throw std::runtime_error("Error: Could not open video.");
    }

    // Get video properties
    int frameWidth = static_cast<int>(video.get(cv::CAP_PROP_FRAME_WIDTH));
    int frameHeight = static_cast<int>(video.get(cv::CAP_PROP_FRAME_HEIGHT));
    double originalFps = video.get(cv::CAP_PROP_FPS);
    int fourcc = static_cast<int>(video.get(cv::CAP_PROP_FOURCC));

    // Calculate new FPS based on speed multiplier
    double newFps = originalFps * speedMultiplier;

    // Create the output video writer
    cv::VideoWriter writer(outputFile, fourcc, newFps, cv::Size(frameWidth, frameHeight), filter != "Grayscale");
    if (!writer.isOpened()) {
        throw std::runtime_error("Error: Could not open output video file.");
    }

    // Set the start position in the video
    video.set(cv::CAP_PROP_POS_MSEC, startSeconds * 1000);

    cv::Mat frame;
    while (video.get(cv::CAP_PROP_POS_MSEC) < endSeconds * 1000 && video.read(frame)) {
        // Apply filters based on the selected option
        if (filter == "Sepia") {
            cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
                0.272, 0.534, 0.131,
                0.349, 0.686, 0.168,
                0.393, 0.769, 0.189);
            cv::transform(frame, frame, kernel);
        }
        else if (filter == "Grayscale") {
            cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
        }
        else if (filter == "Edge Detection") {
            cv::Mat edges;
            cv::Canny(frame, edges, 100, 200);
            cv::cvtColor(edges, frame, cv::COLOR_GRAY2BGR); // Convert edges back to BGR for saving
        }
        else if (filter == "Blur") {
            cv::GaussianBlur(frame, frame, cv::Size(15, 15), 0);
        }
        // "No filter" is the default case; no changes to the frame

        writer.write(frame);  // Write the frame to the output video
    }

    // Release resources
    writer.release();
    video.release();

    std::cout << "Processed video with filter (" << filter << ") saved as: " << outputFile << std::endl;
}





// Placeholder functions for the other actions
void mergeVideos() {
    std::cout << "Merge Videos button clicked.\n";
    // Add logic to select and merge up to 6 videos
}

void insertAd() {
    std::cout << "Insert Ad button clicked.\n";
    // Add logic for inserting an ad video into the main video
}


void mergeVideosUI() {
    ImGui::Begin("Merge Videos - Select up to 6");

    // Add Video Button
    if (ImGui::Button("Add Video") && videos.size() < maxVideos) {
        addVideo();
    }

    // Display each selected video with its order dropdown
    for (size_t i = 0; i < videos.size(); ++i) {
        ImGui::Text("Video %d: %s", static_cast<int>(i + 1), videos[i].path.c_str());

        // Order dropdown for each video
        std::string comboLabel = "Order##" + std::to_string(i);
        if (ImGui::BeginCombo(comboLabel.c_str(), std::to_string(videos[i].order).c_str())) {
            for (int j = 1; j <= videos.size(); ++j) {
                bool isSelected = (videos[i].order == j);
                if (ImGui::Selectable(std::to_string(j).c_str(), isSelected)) {
                      videos[i].order = j;                    
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    // Display order error message if there is an order conflict
    if (orderError) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: Each video must have a unique order.");
    }

    // Preview and Save Buttons
    if (ImGui::Button("Preview")) {
        if (checkUniqueOrders()) {
            previewMergedVideo();
            orderError = false;
        }
        else {
            orderError = true;  // Set the error flag if orders are not unique
        }   
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        saveMergedVideo();
    }

    // Back Button
    if (ImGui::Button("Back")) {
        uiState = UIState::MainMenu;
        videos.clear();
    }

    ImGui::End();
}

// Function to add a new video using tinyfiledialogs
void addVideo() {
    const char* filters[] = { "*.mp4", "*.avi", "*.mov" };
    const char* path = tinyfd_openFileDialog("Select a video file", "", 3, filters, "Video Files", 0);

    if (path) {
        // Assign default order based on the current list size
        int defaultOrder = static_cast<int>(videos.size() + 1);
        videos.push_back({ std::string(path), defaultOrder });
    }
}

// Function to check if each video has a unique order
bool checkUniqueOrders() {
    std::set<int> uniqueOrders;
    for (const auto& video : videos) {
        // If the order is already in the set, there's a duplicate
        if (!uniqueOrders.insert(video.order).second) {
            return false;  // Duplicate found
        }
    }
    return true;
}


// Function to handle preview of merged video (stub function)
void previewMergedVideo() {
    std::cout << "Previewing merged video with order:\n";
    std::sort(videos.begin(), videos.end(), [](const VideoInfo& a, const VideoInfo& b) {
        return a.order < b.order;
        });

    for (const auto& video : videos) {
        std::cout << "Order " << video.order << ": " << video.path << "\n";
    }
    // Add actual preview functionality here
}

// Function to handle saving of merged video (stub function)
void saveMergedVideo() {
    std::cout << "Saving merged video with order:\n";
    std::sort(videos.begin(), videos.end(), [](const VideoInfo& a, const VideoInfo& b) {
        return a.order < b.order;
        });

    for (const auto& video : videos) {
        std::cout << "Order " << video.order << ": " << video.path << "\n";
    }
    // Add actual save functionality here
}


void insertAdUI() {
    ImGui::Begin("Insert Ad - Select Main and Ad Video");

    // Add Main Video Button
    if (ImGui::Button("Add Main Video") && !isMainVideoSet) {
        addMainVideo(true);  // Adds main video
    }

    // Add Ad Video Button
    if (!isAdVideoSet && ImGui::Button("Add Ad Video")) {
        addMainVideo(false);  // Adds ad video
    }

    // Display selected videos and roles
    if (isMainVideoSet) {
        ImGui::Text("Main Video: %s", mainVideo.path.c_str());
    }
    if (isAdVideoSet) {
        ImGui::Text("Ad Video: %s", adVideo.path.c_str());
    }

    // Dropdown to select main/ad video role if both videos are set
    if (isMainVideoSet && isAdVideoSet) {
        ImGui::Text("Select Role for Each Video:");

        // Dropdown for main video role selection
        const char* roleOptions[] = { "Main Video", "Ad Video" };
        if (ImGui::BeginCombo("Role - Main Video", mainVideo.isMainVideo ? "Main Video" : "Ad Video")) {
            if (ImGui::Selectable("Main Video", true)) {
                mainVideo.isMainVideo = true;
                adVideo.isMainVideo = false;
            }
            if (ImGui::Selectable("Ad Video", false)) {
                mainVideo.isMainVideo = false;
                adVideo.isMainVideo = true;
            }
            ImGui::EndCombo();
        }

        // Dropdown for ad video role selection
        if (ImGui::BeginCombo("Role - Ad Video", adVideo.isMainVideo ? "Main Video" : "Ad Video")) {
            if (ImGui::Selectable("Main Video", true)) {
                mainVideo.isMainVideo = false;
                adVideo.isMainVideo = true;
            }
            if (ImGui::Selectable("Ad Video", false)) {
                mainVideo.isMainVideo = true;
                adVideo.isMainVideo = false;
            }
            ImGui::EndCombo();
        }

        // Input field for insertion time in seconds
        ImGui::InputInt("Insert ad after (seconds)", &insertAfterSeconds);
        if (insertAfterSeconds < 0) {
            insertAfterSeconds = 0;  // Ensure the value is non-negative
        }

        // Input field for duration of the ad field
        ImGui::InputInt("Duration of the ad (seconds)", &durationOfAd);
        if (durationOfAd < 0) {
            durationOfAd = 0;  // Ensure the value is non-negative
        }

        // Error message for invalid configurations
        if (insertAdError) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: Please select two videos and set 'Insert after' to a value greater than 0.");
        }

        // Preview and Save Buttons
        if (ImGui::Button("Preview")) {
            if (validateInsertAd()) {
                previewInsertAd();
                insertAdError = false;
            }
            else {
                insertAdError = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            if (validateInsertAd()) {
                saveInsertAd();
                insertAdError = false;
            }
            else {
                insertAdError = true;
            }
        }
    }

    // Back Button
    if (ImGui::Button("Back")) {
        uiState = UIState::MainMenu;
        isMainVideoSet = false;
        isAdVideoSet = false;
        insertAfterSeconds = 0;
        durationOfAd = 0;
        insertAdError = false;
    }

    ImGui::End();
}

// Function to add a video (main or ad) using tinyfiledialogs
void addMainVideo(bool isMainVideo) {
    const char* filters[] = { "*.mp4", "*.avi", "*.mov" };
    const char* path = tinyfd_openFileDialog("Select a video file", "", 3, filters, "Video Files", 0);

    if (path) {
        if (isMainVideo) {
            mainVideo.path = path;
            mainVideo.isMainVideo = true;
            isMainVideoSet = true;
        }
        else {
            adVideo.path = path;
            adVideo.isMainVideo = false;
            isAdVideoSet = true;
        }
    }
}

// Function to validate that two videos are selected and insertion time is valid
bool validateInsertAd() {
    // Check if both videos are selected and if insertion time is greater than zero
    return isMainVideoSet && isAdVideoSet && insertAfterSeconds > 0 && durationOfAd > 0;
}

// Function to handle preview of merged ad (stub function)
void previewInsertAd() {
    std::cout << "Previewing merged video with main video: " << (mainVideo.isMainVideo ? mainVideo.path : adVideo.path)
        << ", ad video: " << (adVideo.isMainVideo ? adVideo.path : mainVideo.path)
        << ", insert after: " << insertAfterSeconds << " seconds.\n";
    // Add actual preview functionality here
}

// Function to handle saving of merged ad (stub function)
void saveInsertAd() {
    std::cout << "Saving merged video with main video: " << (mainVideo.isMainVideo ? mainVideo.path : adVideo.path)
        << ", ad video: " << (adVideo.isMainVideo ? adVideo.path : mainVideo.path)
        << ", insert after: " << insertAfterSeconds << " seconds.\n";
    // Add actual save functionality here
}