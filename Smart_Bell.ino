da #define WIFI_NAME "Wifi SSID"
#define WIFI_PASSWORD "Wifi Password"
#define DEVICE_ID 5 //Device ID
#define DEVICE_NAME "Smart bell"
#define TOKEN "Place for token here"

#include "esp_camera.h"
#include <RemoteMe.h>
#include <RemoteMeSocketConnector.h>
#include <WiFi.h>
#include <NTPClient.h>

#define FLASH_PIN 4 // GPIO4 for flash LED
#define LOCK 12 // GPIO12 for Lock relay
#define BTN_PIN 13 // GPIO13 for button input 
#define BTN_LED_PIN 14 // GPIO14 for button click signal

#define repeatingSendTimeoutSeconds 30
#define secondsWhileUnlocked 5

RemoteMe& remoteMe = RemoteMe::getInstance(TOKEN, DEVICE_ID);

boolean locked = true;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

// Select camera model
#define CAMERA_MODEL_AI_THINKER

//other suported modules
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
//#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

void initCamera()
{
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
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;

#if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
#endif

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
    else {
        Serial.println("Camera init success");
    }

    sensor_t* s = esp_camera_sensor_get();
    //initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1); //flip it back
        s->set_brightness(s, 1); //up the blightness just a bit
        s->set_saturation(s, -2); //lower the saturation
    }

#if defined(CAMERA_MODEL_M5STACK_WIDE)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#endif

    Serial.println("Camera end");
}

inline void setLock(boolean b) {remoteMe.getVariables()->setBoolean("lock", b); }

void unlock(boolean b) {
  locked = !b;
  Serial.printf("Lock b: %d\n",!b);
}

void setup()
{
    pinMode(BTN_PIN, INPUT);
    pinMode(BTN_LED_PIN, OUTPUT);
    pinMode(FLASH_PIN, OUTPUT);
    pinMode(LOCK, OUTPUT);
    Serial.begin(115200);

    initCamera();

    WiFi.begin(WIFI_NAME, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
    }

    remoteMe.getVariables()->observeBoolean("lock" ,unlock);
    remoteMe.setConnector(new RemoteMeSocketConnector());
    remoteMe.sendRegisterDeviceMessage(DEVICE_NAME);
    remoteMe.setUserMessageListener(onUserMessage);

    timeClient.begin();
    timeClient.setTimeOffset(10800);
}

void takePhoto(boolean flash)
{
    //acquire a frame
    if (flash) digitalWrite(FLASH_PIN, HIGH);//flash on
    camera_fb_t* fb = esp_camera_fb_get();
    if (flash) digitalWrite(FLASH_PIN, LOW);//flash off
    
    if (!fb) {
        Serial.println("Camera Capture Failed");
        return;
    }

    //return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);
    Serial.println("photo took");

    remoteMe.setFileContent(6, "photos/photo.jpg", fb->len, fb->buf);
    Serial.printf(" buff size %d \n", fb->len);
    Serial.println("sent");
}

void onUserMessage(uint16_t senderDeviceId, uint16_t dataSize, uint8_t* data)
{
    if (!data[0]) { //without flash
      takePhoto(false);
    } else { //with flash
      takePhoto(true); 
    }
}

void loop()
{
    static long lastSend = 0;
    static long unlockTime = 0;
    static bool btnLed = false;
    remoteMe.loop();

    //checking for button input and time since last input
    if (digitalRead(BTN_PIN) && ((lastSend == 0) || (lastSend + repeatingSendTimeoutSeconds * 1000 < millis()))) { 
        takePhoto(false);
        lastSend = millis();
        
        if(!timeClient.update()) {
          timeClient.forceUpdate();
        }
        
        formattedDate = timeClient.getFormattedDate();
        Serial.println(formattedDate);
        
        int splitT = formattedDate.indexOf("T");
        dayStamp = formattedDate.substring(0, splitT);
        timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-4);
        
        remoteMe.sendPushNotificationMessage(6, "Ring", "Ring on " + dayStamp + " at " + timeStamp, "badge.png", "icon192.png", "photos/photo.jpg");
        digitalWrite(BTN_LED_PIN, HIGH);
        btnLed = true;
    }

    if (btnLed && lastSend + 3000 < millis()) {
      digitalWrite(BTN_LED_PIN, LOW);
      btnLed = false;
    }

    //unlocking
    if (!locked && unlockTime == 0) {
      digitalWrite(LOCK, HIGH);
      unlockTime = millis();
    
    //reseting lock timer and changing input in remoteme server
    } else if (!locked && unlockTime + secondsWhileUnlocked * 1000 < millis()) {
      setLock(false);
      unlockTime = 0;

    //locking after defined seconds
    } else if (locked) {
      digitalWrite(LOCK, LOW);
      unlockTime = 0;
    }
}
