/*
 * Project: Monitor the mail box activities
 * Description: This project will has two major functionalities.
 * -- 1. Send a text message using ifttt whenever the mail box door is opened.
 * -- 2. User can take a picture inside the mail box by sending a text (#Click) to ifttt
 * -- Also history of mailbox activities will recorded on thingspeaks chart. (Public view : https://thingspeak.com/channels/641416)
 * - This project is developed and implemented by Final Project Group 7 - Internet of Things - Fall 2018 - NWMSU
 * Team members: Vineeth Agarwal, Vishal Chilka, Nilantha Dambadeni Kalu Achchillage, Simi Maithani
 * References : https://www.hackster.io/mailmods/automatic-mailbox-notification-unit-728b98
 *            : https://www.hackster.io/middleca/sending-photos-and-video-over-the-internet-db583f
 * Special thanks to Dr.Michel Rogers for all the advices, guides and sponsoring for the items.
 * Author: Final Project Group 7
 * Date: 12/11/2018
 */

#include "ArduCAM.h"
#include "memorysaver.h"
SYSTEM_THREAD(ENABLED);

#define VERSION_SLUG "7n"

TCPClient client;

#define SERVER_ADDRESS "10.10.110.107" //"192.168.1.121"//"10.31.105.222"//"192.168.1.121"//"10.10.126.51"
#define SERVER_TCP_PORT 5550

#define TX_BUFFER_MAX 1024
uint8_t buffer[TX_BUFFER_MAX + 1];
int tx_buffer_index = 0;




//#define SD_CS D2

// set pin A2 as the slave select for the ArduCAM shield
const int SPI_CS = A2;

ArduCAM myCAM(OV5642, SPI_CS);
int unixTime = 0;
int isDoorOpened = 0;
int doorStat = 0;
int doorStatOLD = 0;
int rawAnalogStrength = 0;
int LED = D2;
int flash = D3;
int door = WKP;

void setup()
{
  // Put initialization like pinMode and begin functions here.
  pinMode(LED, OUTPUT);
  pinMode(door, INPUT);
  pinMode(flash, OUTPUT);
  Particle.variable("isCloased", doorStat);
  Particle.function("Picture", getPicture);
  Particle.publish("status", "Good morning, Version: " + String(VERSION_SLUG));
  delay(1000);

  uint8_t vid,pid;
  uint8_t temp;

  Wire.setSpeed(CLOCK_SPEED_100KHZ);
  Wire.begin();

  Serial.begin(115200);
  Serial.println("ArduCAM Start!");

  // set the SPI_CS as an output:
  pinMode(SPI_CS, OUTPUT);

  // initialize SPI:
  SPI.begin();
  //SPI.begin(SPI_MODE_MASTER);
  //SPI.begin(SPI_MODE_SLAVE, SPI_CS);


  while(1) {
    Particle.publish("status", "checking for camera");
    Serial.println("Checking for camera...");

    //Check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if(temp != 0x55)
    {
      Serial.println("SPI interface Error!");
      Serial.println("myCam.read_reg said " + String(temp));
      delay(5000);
    }
    else {
      break;
    }
    Particle.process();
  }
  Particle.publish("status", "Camera found.");


  while(1){
  //Check if the camera module type is OV5642
    myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
    if ((vid != 0x56) || (pid != 0x42)){
      Serial.println(F("Can't find OV5642 module!"));
      Particle.publish("status", "Not found, camera says " + String::format("%d:%d", vid, pid));
      delay(5000);
      continue;
    }
    else {
      Serial.println(F("OV5642 detected."));
      Particle.publish("status", "OV5642 detected: " + String::format("%d:%d", vid, pid));
      break;
    }
  }
  Serial.println("Camera found, initializing...");
  //Change MCU mode
  myCAM.set_format(JPEG);
  delay(100);

  myCAM.InitCAM();
  delay(100);
  myCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  delay(100);

  myCAM.clear_fifo_flag();
  delay(100);

  myCAM.write_reg(ARDUCHIP_FRAMES,0x00);
  delay(100);

  myCAM.OV5642_set_JPEG_size(OV5642_320x240);
  //myCAM.OV5640_set_JPEG_size(OV5642_1600x1200);
  //myCAM.OV5640_set_JPEG_size(OV5642_640x480);
  //delay(100);

  // wait a sec`
  delay(1000);
  client.connect(SERVER_ADDRESS, SERVER_TCP_PORT);
}


void loop()
{
  // The core of your code will likely live here.
  rawAnalogStrength = analogRead(door);
  // if the door read is greater than 400, consider the door is closed.
  if (rawAnalogStrength < 400 && isDoorOpened == 1) {
      isDoorOpened = 0;
      doorStat = 1; //closed
      digitalWrite(LED, LOW);    // Turn OFF the LED pins
      delay(100);
    }
  if(rawAnalogStrength >=400 && isDoorOpened == 0) {
      isDoorOpened = 1;
      doorStat = 0; //open
      digitalWrite(LED, HIGH);   // Turn ON the LED pins
      delay(100);

      Particle.publish("doorStat",String(doorStat)); // Publishes to particle events
      delay(500);
  }
}

// Function to take the pictures.
int getPicture(String command){
    if (!client.connected()) {
        //client.stop();
        Particle.publish("status", "Attempting to reconnect to TCP Server...");
        if (!client.connect(SERVER_ADDRESS, SERVER_TCP_PORT)) {
            delay(1000);
            return 0;
        }
    }
    digitalWrite(flash, HIGH);
    Particle.publish("status", "Taking a picture..."); //publish the event to particle.io
    Serial.println("Taking a picture..."); // print the status on serial output

    myCAM.OV5642_set_JPEG_size(OV5642_2592x1944); //works
    delay(100);
    myCAM.flush_fifo();
    delay(100);
    myCAM.clear_fifo_flag();
    delay(100);
    myCAM.start_capture();
    delay(100);

    //myCAM.start_capture();
    unsigned long start_time = millis(),
                  last_publish = millis();

//  wait for the photo to be done........ ......

    while(!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK)) {
        Particle.process();
        delay(10);

        unsigned long now = millis();
        if ((now - last_publish) > 1000) {
            Particle.publish("status", "waiting for photo " + String(now-start_time));
            last_publish = now;
        }

        if ((now-start_time) > 30000) {
            Particle.publish("status", "bailing...");
            break;
        }
    }
    delay(100);

    int length = myCAM.read_fifo_length();
    Particle.publish("status", "Image size is " + String(length));
    Serial.println("Image size is " + String(length));

    uint8_t temp = 0xff, temp_last = 0;
    int bytesRead = 0;

    if(myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
    {
        delay(100);
        Serial.println(F("ACK CMD CAM Capture Done."));
        Particle.publish("status", "Capture done");
        myCAM.set_fifo_burst();

        tx_buffer_index = 0;
        temp = 0;

        //while (bytesRead < length)
        while( (temp != 0xD9) | (temp_last != 0xFF) )
        {
            temp_last = temp;
            temp = myCAM.read_fifo();
            bytesRead++;


            buffer[tx_buffer_index++] = temp;

            if (tx_buffer_index >= TX_BUFFER_MAX) {
                client.write(buffer, tx_buffer_index);

                tx_buffer_index = 0;
                Particle.process();
            }

            if (bytesRead > 2048000) {
                // failsafe
                break;
            }
        }


        if (tx_buffer_index != 0) {
            client.write(buffer, tx_buffer_index);
        }

        //Clear the capture done flag
        //myCAM.CS_HIGH();
        myCAM.clear_fifo_flag();

        Serial.println(F("End of Photo"));
    }
    digitalWrite(flash, LOW); // Switch off the flash light.
    return 0;
}
