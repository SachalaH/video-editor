#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <sstream>

// Function to get video duration using FFprobe (returns duration in seconds as double)
// This function writes FFprobe output to a temporary file and reads it
double get_video_duration(const std::string& video_path) {
    std::string temp_file = "duration.txt";
    std::string command = "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"" + video_path + "\" > " + temp_file;

    // Execute command and write output to temp file
    int result = std::system(command.c_str());
    if (result != 0) {
        std::cerr << "Error executing FFprobe to get video duration.\n";
        return -1;
    }

    // Read duration from temp file
    std::ifstream infile(temp_file);
    double duration = -1;
    if (infile.is_open()) {
        infile >> duration;
        infile.close();
    }
    // Clean up temporary file
    std::remove(temp_file.c_str());
    return duration;
}

// Function to generate FFmpeg filter complex command to insert ads in the main video
std::string generate_ffmpeg_command(const std::string& main_video, const std::string& ad_video, double X, double Y, int num_insertions) {
    std::ostringstream filter_complex;
    filter_complex << "-filter_complex \"";

    // Initial input main video segment up to the first ad insertion
    filter_complex << "[0:v]trim=0:" << X << ",setpts=PTS-STARTPTS[v0]; ";
    filter_complex << "[0:a]atrim=0:" << X << ",asetpts=PTS-STARTPTS[a0]; ";

    for (int i = 0; i < num_insertions; ++i) {
        double start_main = X * (i + 1); // start time of the next main video part
        double end_main = X * (i + 2);   // end time of the next main video part

        // Add the ad segment
        filter_complex << "[1:v]trim=0:" << Y << ",setpts=PTS-STARTPTS[v" << (i + 1) << "]; ";
        filter_complex << "[1:a]atrim=0:" << Y << ",asetpts=PTS-STARTPTS[a" << (i + 1) << "]; ";

        // Add the next part of the main video after the ad
        filter_complex << "[0:v]trim=" << start_main << ":" << end_main << ",setpts=PTS-STARTPTS[v" << (i + 2) << "]; ";
        filter_complex << "[0:a]atrim=" << start_main << ":" << end_main << ",asetpts=PTS-STARTPTS[a" << (i + 2) << "]; ";
    }

    // Concatenate all parts
    filter_complex << "concat=n=" << (2 * num_insertions + 1) << ":v=1:a=1[v][a]\" -map \"[v]\" -map \"[a]\" output.mp4";
    return "ffmpeg -i \"" + main_video + "\" -i \"" + ad_video + "\" " + filter_complex.str();
}

int main() {
    // User input for file paths and interval parameters
    std::string main_video, ad_video;
    double X, Y;

    std::cout << "Enter the path for the main video: ";
    std::getline(std::cin, main_video);

    std::cout << "Enter the path for the ad video: ";
    std::getline(std::cin, ad_video);

    std::cout << "Enter the interval (X) in seconds after which the ad should be played: ";
    std::cin >> X;

    std::cout << "Enter the duration (Y) in seconds for each ad segment (or enter -1 to use full ad length): ";
    std::cin >> Y;

    // Calculate video durations
    double main_duration = get_video_duration(main_video);
    double ad_duration = get_video_duration(ad_video);

    if (main_duration < 0 || ad_duration < 0) {
        std::cerr << "Error: Could not retrieve video duration.\n";
        return 1;
    }

    // Use full length of ad if Y is not set by user
    if (Y < 0) Y = ad_duration;

    // Calculate how many times the ad will be inserted
    int num_insertions = static_cast<int>(main_duration / X);

    // Generate FFmpeg command
    std::string command = generate_ffmpeg_command(main_video, ad_video, X, Y, num_insertions);

    // Run FFmpeg command
    std::cout << "Executing command:\n" << command << std::endl;
    int result = std::system(command.c_str());
    if (result == 0) {
        std::cout << "Video processing completed successfully.\n";
    }
    else {
        std::cerr << "Error during video processing.\n";
    }

    return 0;
}
