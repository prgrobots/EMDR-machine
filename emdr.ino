#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <FastLED.h>

// Pin definitions for ESP8266 Motor Shield
// Motor A (left motor)
#define MOTOR_LEFT_PWM D1    // PWM to control speed
#define MOTOR_LEFT_IN1 D3    // Set HIGH
#define MOTOR_LEFT_IN2 D4    // Set HIGH

// Motor B (right motor)  
#define MOTOR_RIGHT_PWM D2   // PWM to control speed
#define MOTOR_RIGHT_IN3 D5   // Set HIGH
#define MOTOR_RIGHT_IN4 D6   // Set HIGH

// LED strip
#define LED_PIN D7
#define NUM_LEDS 20  // Reduced for ESP8266 memory

CRGB leds[NUM_LEDS];

// WiFi AP credentials
const char* ssid = "EMDR-Control";
const char* password = "emdr1234";

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// EMDR parameters
struct EMDRConfig {
  bool running = false;
  bool visualEnabled = true;
  bool audioEnabled = true;
  bool tactileEnabled = true;
  
  int speed = 1000;
  int cycles = 10;
  int brightness = 128;
  int colorR = 255;
  int colorG = 255;
  int colorB = 255;
  int beepFreq = 800;
  int vibeIntensity = 200;
  
  int currentCycle = 0;
  bool leftActive = true;
} config;

unsigned long lastToggle = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize LED strip
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(config.brightness);
  
  // Initialize motor pins
  pinMode(MOTOR_LEFT_PWM, OUTPUT);
  pinMode(MOTOR_LEFT_IN1, OUTPUT);
  pinMode(MOTOR_LEFT_IN2, OUTPUT);
  pinMode(MOTOR_RIGHT_PWM, OUTPUT);
  pinMode(MOTOR_RIGHT_IN3, OUTPUT);
  pinMode(MOTOR_RIGHT_IN4, OUTPUT);
  
  // Set direction pins HIGH (motors always "forward")
  digitalWrite(MOTOR_LEFT_IN1, HIGH);
  digitalWrite(MOTOR_LEFT_IN2, HIGH);
  digitalWrite(MOTOR_RIGHT_IN3, HIGH);
  digitalWrite(MOTOR_RIGHT_IN4, HIGH);
  
  // Motors off initially
  analogWrite(MOTOR_LEFT_PWM, 0);
  analogWrite(MOTOR_RIGHT_PWM, 0);
  
  // Start WiFi AP
  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP started");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Setup web server
  server.on("/", handleRoot);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
  
  // Start WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("EMDR Ready!");
}

void loop() {
  server.handleClient();
  webSocket.loop();
  
  // EMDR cycle logic
  if (config.running) {
    unsigned long now = millis();
    int halfCycle = config.speed / 2;
    
    if (now - lastToggle >= halfCycle) {
      lastToggle = now;
      config.leftActive = !config.leftActive;
      
      if (config.leftActive) {
        config.currentCycle++;
        if (config.currentCycle > config.cycles) {
          stopSession();
          sendState();
          return;
        }
      }
      
      updateOutputs();
      sendCycle();
    }
  }
  
  yield(); // Important for ESP8266
}

void updateOutputs() {
  // Update LEDs
  if (config.visualEnabled) {
    int halfLeds = NUM_LEDS / 2;
    CRGB color = CRGB(config.colorR, config.colorG, config.colorB);
    
    for (int i = 0; i < NUM_LEDS; i++) {
      if (config.leftActive && i < halfLeds) {
        leds[i] = color;
      } else if (!config.leftActive && i >= halfLeds) {
        leds[i] = color;
      } else {
        leds[i] = CRGB::Black;
      }
    }
    FastLED.setBrightness(config.brightness);
    FastLED.show();
  } else {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
  }
  
  // Update motors
  if (config.tactileEnabled) {
    if (config.leftActive) {
      analogWrite(MOTOR_LEFT_PWM, config.vibeIntensity);
      analogWrite(MOTOR_RIGHT_PWM, 0);
    } else {
      analogWrite(MOTOR_LEFT_PWM, 0);
      analogWrite(MOTOR_RIGHT_PWM, config.vibeIntensity);
    }
  } else {
    analogWrite(MOTOR_LEFT_PWM, 0);
    analogWrite(MOTOR_RIGHT_PWM, 0);
  }
  
  // Send audio trigger
  if (config.audioEnabled) {
    String msg = "{\"audio\":true,\"side\":\"";
    msg += config.leftActive ? "left" : "right";
    msg += "\",\"freq\":";
    msg += config.beepFreq;
    msg += "}";
    webSocket.broadcastTXT(msg);
  }
}

void stopSession() {
  config.running = false;
  config.currentCycle = 0;
  
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  analogWrite(MOTOR_LEFT_PWM, 0);
  analogWrite(MOTOR_RIGHT_PWM, 0);
}

void sendState() {
  String msg = "{\"type\":\"state\",\"running\":";
  msg += config.running ? "true" : "false";
  msg += ",\"visual\":";
  msg += config.visualEnabled ? "true" : "false";
  msg += ",\"audio\":";
  msg += config.audioEnabled ? "true" : "false";
  msg += ",\"tactile\":";
  msg += config.tactileEnabled ? "true" : "false";
  msg += ",\"speed\":";
  msg += config.speed;
  msg += ",\"cycles\":";
  msg += config.cycles;
  msg += ",\"currentCycle\":";
  msg += config.currentCycle;
  msg += ",\"brightness\":";
  msg += config.brightness;
  msg += ",\"colorR\":";
  msg += config.colorR;
  msg += ",\"colorG\":";
  msg += config.colorG;
  msg += ",\"colorB\":";
  msg += config.colorB;
  msg += ",\"beepFreq\":";
  msg += config.beepFreq;
  msg += ",\"vibeIntensity\":";
  msg += config.vibeIntensity;
  msg += "}";
  webSocket.broadcastTXT(msg);
}

void sendCycle() {
  String msg = "{\"type\":\"cycle\",\"current\":";
  msg += config.currentCycle;
  msg += ",\"total\":";
  msg += config.cycles;
  msg += "}";
  webSocket.broadcastTXT(msg);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload);
    
    if (msg.startsWith("start")) {
      config.running = true;
      config.currentCycle = 0;
      config.leftActive = true;
      lastToggle = millis();
      updateOutputs();
      sendState();
    }
    else if (msg.startsWith("stop")) {
      stopSession();
      sendState();
    }
    else if (msg.startsWith("speed:")) {
      config.speed = msg.substring(6).toInt();
    }
    else if (msg.startsWith("cycles:")) {
      config.cycles = msg.substring(7).toInt();
    }
    else if (msg.startsWith("brightness:")) {
      config.brightness = msg.substring(11).toInt();
    }
    else if (msg.startsWith("color:")) {
      sscanf(msg.substring(6).c_str(), "%d,%d,%d", 
             &config.colorR, &config.colorG, &config.colorB);
    }
    else if (msg.startsWith("beep:")) {
      config.beepFreq = msg.substring(5).toInt();
    }
    else if (msg.startsWith("vibe:")) {
      config.vibeIntensity = msg.substring(5).toInt();
    }
    else if (msg.startsWith("visual:")) {
      config.visualEnabled = msg.substring(7) == "1";
    }
    else if (msg.startsWith("audio:")) {
      config.audioEnabled = msg.substring(6) == "1";
    }
    else if (msg.startsWith("tactile:")) {
      config.tactileEnabled = msg.substring(8) == "1";
    }
    else if (msg == "getState") {
      sendState();
    }
  }
  else if (type == WStype_CONNECTED) {
    Serial.printf("Client connected\n");
    sendState();
  }
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

String getHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>EMDR</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 15px;
    }
    .container {
      max-width: 480px;
      margin: 0 auto;
      background: white;
      border-radius: 15px;
      padding: 20px;
      box-shadow: 0 8px 30px rgba(0,0,0,0.3);
    }
    h1 {
      text-align: center;
      color: #667eea;
      margin-bottom: 20px;
      font-size: 24px;
    }
    .status {
      text-align: center;
      padding: 10px;
      border-radius: 8px;
      font-weight: 600;
      margin-bottom: 15px;
      font-size: 16px;
    }
    .running { background: #d1fae5; color: #065f46; }
    .stopped { background: #fee2e2; color: #991b1b; }
    .progress {
      text-align: center;
      padding: 12px;
      background: #f0f4ff;
      border-radius: 8px;
      margin-bottom: 15px;
      font-size: 22px;
      font-weight: 700;
      color: #667eea;
    }
    .toggles {
      display: flex;
      gap: 8px;
      margin-bottom: 15px;
    }
    .toggle {
      flex: 1;
      padding: 10px;
      border: 2px solid #667eea;
      background: white;
      color: #667eea;
      border-radius: 8px;
      font-weight: 600;
      cursor: pointer;
      font-size: 14px;
    }
    .toggle.on {
      background: #667eea;
      color: white;
    }
    .group {
      margin-bottom: 15px;
      padding: 12px;
      background: #f8f9fa;
      border-radius: 8px;
    }
    label {
      display: block;
      font-weight: 600;
      margin-bottom: 6px;
      font-size: 13px;
      color: #333;
    }
    input[type="range"] {
      width: 100%;
      height: 5px;
      border-radius: 3px;
      background: #ddd;
      outline: none;
    }
    input[type="color"] {
      width: 100%;
      height: 45px;
      border: none;
      border-radius: 8px;
      cursor: pointer;
    }
    .val {
      text-align: right;
      color: #667eea;
      font-weight: 600;
      font-size: 13px;
      margin-top: 4px;
    }
    .btn {
      width: 100%;
      padding: 15px;
      border: none;
      border-radius: 10px;
      font-size: 16px;
      font-weight: 700;
      cursor: pointer;
      margin-bottom: 10px;
      text-transform: uppercase;
    }
    .start { background: #10b981; color: white; }
    .stop { background: #ef4444; color: white; }
  </style>
</head>
<body>
  <div class="container">
    <h1>EMDR Control</h1>
    
    <div class="status stopped" id="status">Stopped</div>
    <div class="progress" id="progress">0 / 10</div>
    
    <div class="toggles">
      <button class="toggle on" id="vis"> Visual</button>
      <button class="toggle on" id="aud"> Audio</button>
      <button class="toggle on" id="tac"> Tactile</button>
    </div>
    
    <div class="group">
      <label>Speed</label>
      <input type="range" id="spd" min="200" max="3000" value="1000" step="100">
      <div class="val" id="spdV">1000 ms</div>
    </div>
    
    <div class="group">
      <label>Cycles</label>
      <input type="range" id="cyc" min="1" max="50" value="10">
      <div class="val" id="cycV">10</div>
    </div>
    
    <div class="group">
      <label>Brightness</label>
      <input type="range" id="bri" min="10" max="255" value="128">
      <div class="val" id="briV">128</div>
    </div>
    
    <div class="group">
      <label>Color</label>
      <input type="color" id="col" value="#ffffff">
    </div>
    
    <div class="group">
      <label>Beep Frequency</label>
      <input type="range" id="bep" min="200" max="2000" value="800" step="50">
      <div class="val" id="bepV">800 Hz</div>
    </div>
    
    <div class="group">
      <label>Vibration</label>
      <input type="range" id="vib" min="0" max="255" value="200">
      <div class="val" id="vibV">200</div>
    </div>
    
    <button class="btn start" id="start">▶ Start</button>
    <button class="btn stop" id="stop">■ Stop</button>
  </div>

  <script>
    let ws;
    let ctx;
    
    function connect() {
      ws = new WebSocket('ws://' + location.hostname + ':81');
      
      ws.onopen = () => {
        console.log('Connected');
        ws.send('getState');
      };
      
      ws.onmessage = (e) => {
        const d = JSON.parse(e.data);
        
        if (d.type === 'state') {
          document.getElementById('status').textContent = d.running ? 'Running' : 'Stopped';
          document.getElementById('status').className = d.running ? 'status running' : 'status stopped';
          document.getElementById('progress').textContent = d.currentCycle + ' / ' + d.cycles;
          document.getElementById('vis').classList.toggle('on', d.visual);
          document.getElementById('aud').classList.toggle('on', d.audio);
          document.getElementById('tac').classList.toggle('on', d.tactile);
        } else if (d.type === 'cycle') {
          document.getElementById('progress').textContent = d.current + ' / ' + d.total;
        } else if (d.audio) {
          beep(d.side, d.freq);
        }
      };
      
      ws.onclose = () => {
        setTimeout(connect, 2000);
      };
    }
    
    function beep(side, freq) {
      if (!ctx) ctx = new (window.AudioContext || window.webkitAudioContext)();
      
      const osc = ctx.createOscillator();
      const gain = ctx.createGain();
      const pan = ctx.createStereoPanner();
      
      osc.connect(gain);
      gain.connect(pan);
      pan.connect(ctx.destination);
      
      osc.frequency.value = freq;
      pan.pan.value = side === 'left' ? -0.8 : 0.8;
      gain.gain.value = 0.3;
      gain.gain.exponentialRampToValueAtTime(0.01, ctx.currentTime + 0.1);
      
      osc.start();
      osc.stop(ctx.currentTime + 0.1);
    }
    
    document.getElementById('spd').oninput = (e) => {
      document.getElementById('spdV').textContent = e.target.value + ' ms';
      ws.send('speed:' + e.target.value);
    };
    
    document.getElementById('cyc').oninput = (e) => {
      document.getElementById('cycV').textContent = e.target.value;
      ws.send('cycles:' + e.target.value);
    };
    
    document.getElementById('bri').oninput = (e) => {
      document.getElementById('briV').textContent = e.target.value;
      ws.send('brightness:' + e.target.value);
    };
    
    document.getElementById('col').oninput = (e) => {
      const h = e.target.value;
      const r = parseInt(h.substr(1,2), 16);
      const g = parseInt(h.substr(3,2), 16);
      const b = parseInt(h.substr(5,2), 16);
      ws.send('color:' + r + ',' + g + ',' + b);
    };
    
    document.getElementById('bep').oninput = (e) => {
      document.getElementById('bepV').textContent = e.target.value + ' Hz';
      ws.send('beep:' + e.target.value);
    };
    
    document.getElementById('vib').oninput = (e) => {
      document.getElementById('vibV').textContent = e.target.value;
      ws.send('vibe:' + e.target.value);
    };
    
    document.getElementById('vis').onclick = (e) => {
      e.target.classList.toggle('on');
      ws.send('visual:' + (e.target.classList.contains('on') ? '1' : '0'));
    };
    
    document.getElementById('aud').onclick = (e) => {
      e.target.classList.toggle('on');
      ws.send('audio:' + (e.target.classList.contains('on') ? '1' : '0'));
    };
    
    document.getElementById('tac').onclick = (e) => {
      e.target.classList.toggle('on');
      ws.send('tactile:' + (e.target.classList.contains('on') ? '1' : '0'));
    };
    
    document.getElementById('start').onclick = () => {
      if (!ctx) ctx = new (window.AudioContext || window.webkitAudioContext)();
      ws.send('start');
    };
    
    document.getElementById('stop').onclick = () => {
      ws.send('stop');
    };
    
    connect();
  </script>
</body>
</html>
)rawliteral";
}
