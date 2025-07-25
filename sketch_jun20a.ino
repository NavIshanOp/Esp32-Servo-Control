#include <WiFi.h>
#include <ESP32Servo.h>
#include <vector>

// Define Servo objects
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;  // Servo object for the fourth servo

// GPIO the servos are attached to
static const int servoPin1 = 13;
static const int servoPin2 = 12;
static const int servoPin3 = 14;
static const int servoPin4 = 27;  // Choose another available GPIO pin for the fourth servo

// Replace with your hotspot credentials
const char* ssid     = "<YOUR_SSID>";
const char* password = "<PASSWORD>";

// Set static IP address
IPAddress local_IP(192, 168, 1, 230);  // Set your desired static IP
IPAddress gateway(192, 168, 1, 1);     // Set your network gateway
IPAddress subnet(255, 255, 255, 0);    // Set your network subnet mask

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Variables to decode HTTP GET values for servo positions
String valueString1 = String(0);
String valueString2 = String(0);
String valueString3 = String(90);
String valueString4 = String(0);
int pos1 = 0;
int pos2 = 0;
int pos3 = 90;
int pos4 = 0;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// Recording and Playback
bool isRecording = false;
std::vector<std::vector<String>> recordedSessions; // Store multiple sessions
std::vector<String> currentRecording;
unsigned long recordingStartTime;

// Function to convert degree to microseconds
int degreeToMicroseconds(int degree) {
  return map(degree, 0, 360, 1000, 2000);
}

void setup() {
  Serial.begin(115200);

  // Attach the servos on the respective GPIO pins
  servo1.attach(servoPin1);
  servo2.attach(servoPin2);
  servo3.attach(servoPin3);
  servo4.attach(servoPin4);  // Attach the fourth servo

  // Set initial positions to stop for MG90S
  servo1.writeMicroseconds(1500);
  servo2.writeMicroseconds(1500);
  servo3.write(90);  // Set initial position for the third servo
  servo4.writeMicroseconds(1500);  // Set initial position for the fourth servo

  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop() {
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // Check if the request is for recording or playing
            if (header.indexOf("GET /?record=start") >= 0) {
              isRecording = true;
              recordingStartTime = millis();
              currentRecording.clear();  // Clear any previous recordings
              Serial.println("Recording started.");
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Recording started");
            } 
            else if (header.indexOf("GET /?record=stop") >= 0) {
              isRecording = false;
              recordedSessions.push_back(currentRecording); // Save current recording to sessions
              String recordingSession = "Recorded " + String(currentRecording.size()) + " movements at " + String(millis());
              Serial.println("Recording stopped. " + recordingSession);
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/plain");
              client.println("Connection: close");
              client.println();
              client.println(recordingSession);  // Send recording session info back to client
            } 
            else if (header.indexOf("GET /?play=") >= 0) {
              int sessionIndex = header.substring(header.indexOf('=') + 1).toInt();
              if (sessionIndex < recordedSessions.size()) {
                Serial.println("Playback started for session " + String(sessionIndex));
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/plain");
                client.println("Connection: close");
                client.println();
                client.println("Playback started for session " + String(sessionIndex));

                // Move servos to stop before playback
                servo1.writeMicroseconds(1500);
                servo2.writeMicroseconds(1500);
                servo3.write(90);
                servo4.writeMicroseconds(1500);
                delay(1000); // Give some time to settle

                unsigned long startTime = millis();
                for (size_t i = 0; i < recordedSessions[sessionIndex].size(); i++) {
                  String movement = recordedSessions[sessionIndex][i];
                  int delimiter1 = movement.indexOf(',');
                  int delimiter2 = movement.indexOf(',', delimiter1 + 1);
                  unsigned long timestamp = movement.substring(0, delimiter1).toInt();
                  String servo = movement.substring(delimiter1 + 1, delimiter2);
                  int position = movement.substring(delimiter2 + 1).toInt();

                  // Wait until the recorded time
                  while (millis() - startTime < timestamp) { delay(1); }

                  if (servo == "servo1") {
                    servo1.writeMicroseconds(degreeToMicroseconds(position));
                    Serial.println("Playing back Servo 1 to " + String(position));
                  } else if (servo == "servo2") {
                    servo2.writeMicroseconds(degreeToMicroseconds(position));
                    Serial.println("Playing back Servo 2 to " + String(position));
                  } else if (servo == "servo3") {
                    servo3.write(position);
                    Serial.println("Playing back Servo 3 to " + String(position));
                  } else if (servo == "servo4") {
                    servo4.writeMicroseconds(degreeToMicroseconds(position));
                    Serial.println("Playing back Servo 4 to " + String(position));
                  }
                }

                // Move servos to stop after playback
                servo1.writeMicroseconds(1500);
                servo2.writeMicroseconds(1500);
                servo3.write(90);
                servo4.writeMicroseconds(1500);
                delay(1000); // Give some time to settle

                Serial.println("Playback finished for session " + String(sessionIndex));
              } else {
                Serial.println("Invalid session index: " + String(sessionIndex));
              }
            }
            else if (header.indexOf("GET /?servo1=") >= 0) {
              int pos = header.substring(header.indexOf('=') + 1, header.indexOf('&')).toInt();
              pos1 = pos; // Update the servo 1 position
              servo1.writeMicroseconds(degreeToMicroseconds(pos));  // Move servo 1
              Serial.println("Servo 1 moved to " + String(pos));
              
              // Record the movement if recording is ON
              if (isRecording) {
                unsigned long elapsedTime = millis() - recordingStartTime;
                currentRecording.push_back(String(elapsedTime) + ",servo1," + String(pos));
              }
            }
            else if (header.indexOf("GET /?servo2=") >= 0) {
              int pos = header.substring(header.indexOf('=') + 1, header.indexOf('&')).toInt();
              pos2 = pos; // Update the servo 2 position
              servo2.writeMicroseconds(degreeToMicroseconds(pos));  // Move servo 2
              Serial.println("Servo 2 moved to " + String(pos));

              // Record the movement if recording is ON
              if (isRecording) {
                unsigned long elapsedTime = millis() - recordingStartTime;
                currentRecording.push_back(String(elapsedTime) + ",servo2," + String(pos));
              }
            }
            else if (header.indexOf("GET /?servo3=") >= 0) {
              int pos = header.substring(header.indexOf('=') + 1, header.indexOf('&')).toInt();
              pos3 = pos; // Update the servo 3 position
              servo3.write(pos);  // Move servo 3
              Serial.println("Servo 3 moved to " + String(pos));

              // Record the movement if recording is ON
              if (isRecording) {
                unsigned long elapsedTime = millis() - recordingStartTime;
                currentRecording.push_back(String(elapsedTime) + ",servo3," + String(pos));
              }
            }
            else if (header.indexOf("GET /?servo4=") >= 0) {
              int pos = header.substring(header.indexOf('=') + 1, header.indexOf('&')).toInt();
              pos4 = pos; // Update the servo 4 position
              servo4.writeMicroseconds(degreeToMicroseconds(pos));  // Move servo 4
              Serial.println("Servo 4 moved to " + String(pos));

              // Record the movement if recording is ON
              if (isRecording) {
                unsigned long elapsedTime = millis() - recordingStartTime;
                currentRecording.push_back(String(elapsedTime) + ",servo4," + String(pos));
              }
            }
            else if (header.indexOf("GET /?servo1stop") >= 0) {
              servo1.writeMicroseconds(1500);
              Serial.println("Servo 1 stopped at 1500 microseconds");
            }
            else if (header.indexOf("GET /?servo2stop") >= 0) {
              servo2.writeMicroseconds(1500);
              Serial.println("Servo 2 stopped at 1500 microseconds");
            }
            else if (header.indexOf("GET /?servo3stop") >= 0) {
              servo3.write(90);
              Serial.println("Servo 3 stopped at 90 degrees");
            }
            else if (header.indexOf("GET /?servo4stop") >= 0) {
              servo4.writeMicroseconds(1500);
              Serial.println("Servo 4 stopped at 1500 microseconds");
            }
            else {
              // Default: Serve the main page
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();

              // Display the HTML web page
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              // CSS to style the on/off buttons 
              client.println("<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial; margin-left:auto; margin-right:auto;}");
              client.println(".slider { width: 300px; }</style>");
              client.println("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js\"></script>");
              client.println("</head><body><h1>ESP32 with Quad Servo Control</h1>");
              
              // Servo 1 Control
              client.println("<p>Servo 1 Position: <span id=\"servoPos1\"></span></p>");
              client.println("<button onclick=\"servo1front()\">Front</button>");
              client.println("<button onclick=\"servo1back()\">Back</button>");
              client.println("<button onclick=\"servo1stop()\">Stop</button><br>");
              client.println("<input type=\"range\" min=\"0\" max=\"360\" class=\"slider\" id=\"servoSlider1\" onchange=\"servo1(this.value)\" value=\"" + valueString1 + "\"/>");
              
              // Servo 2 Control
              client.println("<p>Servo 2 Position: <span id=\"servoPos2\"></span></p>");
              client.println("<button onclick=\"servo2front()\">Front</button>");
              client.println("<button onclick=\"servo2back()\">Back</button>");
              client.println("<button onclick=\"servo2stop()\">Stop</button><br>");
              client.println("<input type=\"range\" min=\"0\" max=\"360\" class=\"slider\" id=\"servoSlider2\" onchange=\"servo2(this.value)\" value=\"" + valueString2 + "\"/>");

              // Servo 3 Control
              client.println("<p>Servo 3 Position: <span id=\"servoPos3\"></span></p>");
              client.println("<button onclick=\"servo3front()\">Front</button>");
              client.println("<button onclick=\"servo3back()\">Back</button>");
              client.println("<button onclick=\"servo3stop()\">Stop</button><br>");
              client.println("<input type=\"range\" min=\"0\" max=\"180\" class=\"slider\" id=\"servoSlider3\" onchange=\"servo3(this.value)\" value=\"" + valueString3 + "\"/>");
              
              // Servo 4 Control
              client.println("<p>Servo 4 Position: <span id=\"servoPos4\"></span></p>");
              client.println("<button onclick=\"servo4front()\">Front</button>");
              client.println("<button onclick=\"servo4back()\">Back</button>");
              client.println("<button onclick=\"servo4stop()\">Stop</button><br>");
              client.println("<input type=\"range\" min=\"0\" max=\"360\" class=\"slider\" id=\"servoSlider4\" onchange=\"servo4(this.value)\" value=\"" + valueString4 + "\"/>");

              // Recording Control
              client.println("<p>Recording: <span id=\"recordStatus\">OFF</span></p>");
              client.println("<button onclick=\"startRecording()\">Start Recording</button>");
              client.println("<button onclick=\"stopRecording()\">Stop Recording</button>");
              client.println("<h2>Recorded Sessions:</h2>");
              client.println("<ul id=\"recordedSessions\"></ul>");
              
              // JavaScript functions
              client.println("<script>");
              client.println("var slider1 = document.getElementById(\"servoSlider1\");");
              client.println("var servoP1 = document.getElementById(\"servoPos1\"); servoP1.innerHTML = slider1.value;");
              client.println("slider1.oninput = function() { servoP1.innerHTML = this.value; };");
              client.println("function servo1(pos) { $.get(\"/?servo1=\" + pos + \"&\"); }");
              client.println("function servo1front() { servo1(360); }");
              client.println("function servo1back() { servo1(0); }");
              client.println("function servo1stop() { $.get(\"/?servo1stop\"); }");
              
              client.println("var slider2 = document.getElementById(\"servoSlider2\");");
              client.println("var servoP2 = document.getElementById(\"servoPos2\"); servoP2.innerHTML = slider2.value;");
              client.println("slider2.oninput = function() { servoP2.innerHTML = this.value; };");
              client.println("function servo2(pos) { $.get(\"/?servo2=\" + pos + \"&\"); }");
              client.println("function servo2front() { servo2(360); }");
              client.println("function servo2back() { servo2(0); }");
              client.println("function servo2stop() { $.get(\"/?servo2stop\"); }");

              client.println("var slider3 = document.getElementById(\"servoSlider3\");");
              client.println("var servoP3 = document.getElementById(\"servoPos3\"); servoP3.innerHTML = slider3.value;");
              client.println("slider3.oninput = function() { servoP3.innerHTML = this.value; };");
              client.println("function servo3(pos) { $.get(\"/?servo3=\" + pos + \"&\"); }");
              client.println("function servo3front() { servo3(180); }");
              client.println("function servo3back() { servo3(0); }");
              client.println("function servo3stop() { $.get(\"/?servo3stop\"); }");

              client.println("var slider4 = document.getElementById(\"servoSlider4\");");
              client.println("var servoP4 = document.getElementById(\"servoPos4\"); servoP4.innerHTML = slider4.value;");
              client.println("slider4.oninput = function() { servoP4.innerHTML = this.value; };");
              client.println("function servo4(pos) { $.get(\"/?servo4=\" + pos + \"&\"); }");
              client.println("function servo4front() { servo4(360); }");
              client.println("function servo4back() { servo4(0); }");
              client.println("function servo4stop() { $.get(\"/?servo4stop\"); }");
              
              client.println("function startRecording() { $.get(\"/?record=start\"); document.getElementById('recordStatus').innerHTML = 'ON'; }");
              client.println("function stopRecording() { $.get(\"/?record=stop\", function(data) { updateSessions(data); document.getElementById('recordStatus').innerHTML = 'OFF'; }); }");
              
              client.println("function updateSessions(sessionInfo) {");
              client.println("  var sessionList = document.getElementById(\"recordedSessions\");");
              client.println("  var sessionIndex = sessionList.childElementCount;");
              client.println("  var sessionItem = document.createElement(\"li\");");
              client.println("  sessionItem.innerHTML = sessionInfo + ' <button onclick=\"playSession(' + sessionIndex + ')\">Play</button>';");
              client.println("  sessionList.appendChild(sessionItem);");
              client.println("}");

              client.println("function playSession(index) { $.get(\"/?play=\" + index); }");
              
              client.println("</script>");
             
              client.println("</body></html>");
            }
            // The HTTP response ends with another blank line
            client.println();
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}