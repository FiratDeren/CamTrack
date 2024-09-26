#include "esp_camera.h"

// Define the appropriate pin mappings based on the camera model
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void setup() {
  // Start the serial communication for debugging purposes
  Serial.begin(115200);

  // Initialize the camera with the specified configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; // Set the XCLK frequency to 20 MHz
  config.pixel_format = PIXFORMAT_GRAYSCALE; // Set the pixel format to grayscale
  config.frame_size = FRAMESIZE_CIF; // Set frame size to CIF (400x296 resolution)
  config.jpeg_quality = 12; // Set JPEG quality (lower number = higher quality)
  config.fb_count = 1; // Use only one frame buffer

  // Attempt to initialize the camera with the configuration
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) { // Check if camera initialization failed
    Serial.printf("Camera initialization failed with error: 0x%x", err);
    return;
  }
}

// Function to calculate the average grayscale value for each vertical segment
// This function divides the image into segments and calculates the average
// grayscale value for each segment.
void calculate_greyscale(int width, int height, int num_of_segments, camera_fb_t * fb, float *average_per_segment){
  int segment_width = width / num_of_segments; // Calculate the width of each segment

  for (int segment = 0; segment < num_of_segments; segment++) {
    unsigned long sum = 0;

    // Loop through each pixel in the segment and calculate the sum of pixel values
    for (int x = segment * segment_width; x < (segment + 1) * segment_width; x++) {
      for (int y = 0; y < height; y++) {
        int index = y * width + x; // Calculate the pixel index in the frame buffer
        sum += fb->buf[index]; // Add the pixel value to the sum
      }
    }

    // Calculate and store the average grayscale value for the segment
    average_per_segment[segment] = sum / (segment_width * height);
  }
}

// Function to process the captured image and detect segment changes
void image_processing(int number, const int num_of_segments, const float acceptable_diff, float *previous_frame_segments, float *current_frame_segments, int *segment_changes) {

  // Capture an image from the camera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { // Check if the capture failed
    Serial.println("Camera capture failed");
    return;
  }

  int width = fb->width; // Get the image width
  int height = fb->height; // Get the image height

  // Calculate the initial grayscale values for the previous frame segments
  calculate_greyscale(width, height, num_of_segments, fb, previous_frame_segments);
  esp_camera_fb_return(fb); // Return the frame buffer to free memory

  // Process the current frame and compare with the previous frame
  for(int i = 0; i < num_of_segments - 2; i++) {
    // Capture the next image for the current frame
    fb = esp_camera_fb_get();
    if (!fb) { // Check if the capture failed
      Serial.println("Camera capture failed");
      continue;
    }

    // Calculate the grayscale values for the current frame segments
    calculate_greyscale(width, height, num_of_segments, fb, current_frame_segments);
    esp_camera_fb_return(fb); // Return the frame buffer to free memory

    // Compare the current frame segments with the previous frame segments
    for (int segment = 0; segment < num_of_segments; segment++) {
      float diff = abs(current_frame_segments[segment] - previous_frame_segments[segment]);
      if (diff > acceptable_diff) { // If the difference is greater than the acceptable threshold
        segment_changes[i] = segment; // Store the segment index where the change occurred
      }
    }

    // Copy the current frame segments to the previous frame segments for the next comparison
    memcpy(previous_frame_segments, current_frame_segments, num_of_segments * sizeof(float));
  }

  // Check the direction of the segment changes and adjust the number accordingly
  if(segment_changes[2] < segment_changes[1] && segment_changes[1] < segment_changes[0]) {
    Serial.println("Move Left."); // Detected movement to the left
    number++;
  }
  else if(segment_changes[2] > segment_changes[1] && segment_changes[1] > segment_changes[0]) {
    Serial.println("Move Right."); // Detected movement to the right
    number--;
  }

  // Print the segment changes
  for (int j = 0; j < num_of_segments - 2; j++) {
   Serial.print(segment_changes[j]);
   Serial.print(" ");
  }
  Serial.print("\n\n");
}

void loop() {
  int number = 0; // Initialize a counter variable
  const int num_of_segments = 5; // Define the number of vertical segments to divide the image
  const float acceptable_diff = 7.0; // Define the threshold for segment change detection
  float previous_frame_segments[num_of_segments] = {0}; // Array to store previous frame segment averages
  float current_frame_segments[num_of_segments] = {0}; // Array to store current frame segment averages
  int segment_changes[num_of_segments - 2] = {0}; // Array to store the indices of segment changes

  // Call the image processing function to detect movement
  image_processing(number, num_of_segments, acceptable_diff, previous_frame_segments, current_frame_segments, segment_changes);
}
