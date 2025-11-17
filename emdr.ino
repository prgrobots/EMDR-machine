#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <FastLED.h>

// Pin definitions for ESP8266 Motor Shield
#define MOTOR_LEFT_PWM D1
#define MOTOR_LEFT_IN1 D3
#define MOTOR_LEFT_IN2 D4
#define MOTOR_RIGHT_PWM D2
#define MOTOR_RIGHT_IN3 D5
#define MOTOR_RIGHT_IN4 D6
#define LED_PIN D7
#define NUM_LEDS 20

CRGB leds[NUM_LEDS];

const char* ssid = "EMDR-Control";
const char* password = "emdr1234";

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

struct EMDRConfig {
  bool running = false;
  bool visualEnabled = true;
  bool audioEnabled = true;
  bool tactileEnabled = true;
  
  int speed = 1000;
  int cycleDuration = 30;
  int brightness = 128;
  int colorR = 255;
  int colorG = 255;
  int colorB = 255;
  int beepFreq = 800;
  int vibeIntensity = 200;
  
  int desensThreshold = 5;
  int beliefThreshold = 5;
  
  unsigned long phaseStartTime = 0;
  int currentPos = 0;
  bool movingRight = true;
} config;

unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(config.brightness);
  
  pinMode(MOTOR_LEFT_PWM, OUTPUT);
  pinMode(MOTOR_LEFT_IN1, OUTPUT);
  pinMode(MOTOR_LEFT_IN2, OUTPUT);
  pinMode(MOTOR_RIGHT_PWM, OUTPUT);
  pinMode(MOTOR_RIGHT_IN3, OUTPUT);
  pinMode(MOTOR_RIGHT_IN4, OUTPUT);
  
  digitalWrite(MOTOR_LEFT_IN1, HIGH);
  digitalWrite(MOTOR_LEFT_IN2, HIGH);
  digitalWrite(MOTOR_RIGHT_IN3, HIGH);
  digitalWrite(MOTOR_RIGHT_IN4, HIGH);
  
  analogWrite(MOTOR_LEFT_PWM, 0);
  analogWrite(MOTOR_RIGHT_PWM, 0);
  
  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP started");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  
  server.on("/", handleRoot);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
  
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("EMDR Ready!");
}

void loop() {
  server.handleClient();
  webSocket.loop();
  
  if (config.running) {
    unsigned long elapsed = (millis() - config.phaseStartTime) / 1000;
    if (elapsed >= config.cycleDuration) {
      stopSession();
      sendComplete();
      return;
    }
    
    unsigned long now = millis();
    int stepDelay = config.speed / (NUM_LEDS - 1);
    
    if (now - lastUpdate >= stepDelay) {
      lastUpdate = now;
      
      if (config.movingRight) {
        config.currentPos++;
        if (config.currentPos >= NUM_LEDS - 1) {
          config.currentPos = NUM_LEDS - 1;
          config.movingRight = false;
          triggerBeepAndVibe(false);
        }
      } else {
        config.currentPos--;
        if (config.currentPos <= 0) {
          config.currentPos = 0;
          config.movingRight = true;
          triggerBeepAndVibe(true);
        }
      }
      
      updateOutputs();
      sendProgress();
    }
  }
  
  yield();
}

void updateOutputs() {
  if (config.visualEnabled) {
    CRGB color = CRGB(config.colorR, config.colorG, config.colorB);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    leds[config.currentPos] = color;
    if (config.currentPos > 0) {
      leds[config.currentPos - 1] = color;
      leds[config.currentPos - 1].fadeToBlackBy(180);
    }
    if (config.currentPos < NUM_LEDS - 1) {
      leds[config.currentPos + 1] = color;
      leds[config.currentPos + 1].fadeToBlackBy(180);
    }
    
    FastLED.setBrightness(config.brightness);
    FastLED.show();
  } else {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
  }
}

void triggerBeepAndVibe(bool isLeft) {
  if (config.tactileEnabled) {
    if (isLeft) {
      analogWrite(MOTOR_LEFT_PWM, config.vibeIntensity);
      analogWrite(MOTOR_RIGHT_PWM, 0);
      delay(100);
      analogWrite(MOTOR_LEFT_PWM, 0);
    } else {
      analogWrite(MOTOR_LEFT_PWM, 0);
      analogWrite(MOTOR_RIGHT_PWM, config.vibeIntensity);
      delay(100);
      analogWrite(MOTOR_RIGHT_PWM, 0);
    }
  }
  
  if (config.audioEnabled) {
    String msg = "{\"audio\":true,\"side\":\"";
    msg += isLeft ? "left" : "right";
    msg += "\",\"freq\":";
    msg += config.beepFreq;
    msg += "}";
    webSocket.broadcastTXT(msg);
  }
}

void stopSession() {
  config.running = false;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  analogWrite(MOTOR_LEFT_PWM, 0);
  analogWrite(MOTOR_RIGHT_PWM, 0);
}

void sendComplete() {
  String msg = "{\"type\":\"complete\"}";
  webSocket.broadcastTXT(msg);
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
  msg += ",\"cycleDuration\":";
  msg += config.cycleDuration;
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
  msg += ",\"desensThreshold\":";
  msg += config.desensThreshold;
  msg += ",\"beliefThreshold\":";
  msg += config.beliefThreshold;
  msg += "}";
  webSocket.broadcastTXT(msg);
}

void sendProgress() {
  unsigned long elapsed = (millis() - config.phaseStartTime) / 1000;
  String msg = "{\"type\":\"progress\",\"elapsed\":";
  msg += elapsed;
  msg += ",\"total\":";
  msg += config.cycleDuration;
  msg += "}";
  webSocket.broadcastTXT(msg);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload);
    
    if (msg.startsWith("start")) {
      config.running = true;
      config.currentPos = 0;
      config.movingRight = true;
      config.phaseStartTime = millis();
      lastUpdate = millis();
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
    else if (msg.startsWith("cycleDuration:")) {
      config.cycleDuration = msg.substring(14).toInt();
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
    else if (msg.startsWith("desensThreshold:")) {
      config.desensThreshold = msg.substring(16).toInt();
    }
    else if (msg.startsWith("beliefThreshold:")) {
      config.beliefThreshold = msg.substring(16).toInt();
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
  <title>EMDR Control</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      background: #1a1a1a;
      min-height: 100vh;
      padding: 15px;
      color: #e0e0e0;
    }
    .container {
      max-width: 480px;
      margin: 0 auto;
      background: #2a2a2a;
      border-radius: 15px;
      padding: 20px;
      box-shadow: 0 8px 30px rgba(0,0,0,0.5);
    }
    h1 {
      text-align: center;
      color: #4a9eff;
      margin-bottom: 20px;
      font-size: 24px;
    }
    h2 {
      color: #4a9eff;
      margin-bottom: 15px;
      font-size: 18px;
    }
    .mode-select {
      display: flex;
      gap: 10px;
      margin-bottom: 20px;
    }
    .mode-btn {
      flex: 1;
      padding: 15px;
      border: 2px solid #4a9eff;
      background: #2a2a2a;
      color: #4a9eff;
      border-radius: 8px;
      font-weight: 600;
      cursor: pointer;
      font-size: 16px;
    }
    .mode-btn.active {
      background: #4a9eff;
      color: #1a1a1a;
    }
    .screen {
      display: none;
    }
    .screen.active {
      display: block;
    }
    .input-group {
      margin-bottom: 15px;
    }
    label {
      display: block;
      font-weight: 600;
      margin-bottom: 8px;
      color: #b0b0b0;
      font-size: 14px;
    }
    input[type="number"] {
      width: 100%;
      padding: 10px;
      background: #1a1a1a;
      border: 2px solid #444;
      border-radius: 8px;
      color: #e0e0e0;
      font-size: 16px;
    }
    .status {
      text-align: center;
      padding: 10px;
      border-radius: 8px;
      font-weight: 600;
      margin-bottom: 15px;
      font-size: 16px;
    }
    .running { background: #1e4620; color: #4ade80; }
    .stopped { background: #4a1e1e; color: #f87171; }
    .progress {
      text-align: center;
      padding: 12px;
      background: #1e293b;
      border-radius: 8px;
      margin-bottom: 15px;
      font-size: 22px;
      font-weight: 700;
      color: #4a9eff;
    }
    .toggles {
      display: flex;
      gap: 8px;
      margin-bottom: 15px;
    }
    .toggle {
      flex: 1;
      padding: 10px;
      border: 2px solid #4a9eff;
      background: #2a2a2a;
      color: #4a9eff;
      border-radius: 8px;
      font-weight: 600;
      cursor: pointer;
      font-size: 14px;
    }
    .toggle.on {
      background: #4a9eff;
      color: #1a1a1a;
    }
    .group {
      margin-bottom: 15px;
      padding: 12px;
      background: #1e1e1e;
      border-radius: 8px;
    }
    input[type="range"] {
      width: 100%;
      height: 5px;
      border-radius: 3px;
      background: #444;
      outline: none;
    }
    input[type="color"] {
      width: 100%;
      height: 45px;
      border: 2px solid #444;
      border-radius: 8px;
      cursor: pointer;
      background: #1a1a1a;
    }
    .val {
      text-align: right;
      color: #4a9eff;
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
    .next { background: #4a9eff; color: white; }
    .reset { background: #f59e0b; color: white; }
    .prompt {
      background: #1e293b;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 15px;
      text-align: center;
      font-size: 16px;
      line-height: 1.5;
    }
    .scale-input {
      display: flex;
      align-items: center;
      gap: 10px;
      margin-top: 10px;
    }
    .scale-input input {
      flex: 1;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>EMDR Control</h1>
    
    <div class="mode-select">
      <button class="mode-btn active" id="guidedBtn">Guided Session</button>
      <button class="mode-btn" id="manualBtn">Manual Session</button>
    </div>

    <!-- Guided Session Screens -->
    <div id="guidedSetup" class="screen active">
      <h2>Session Setup</h2>
      
      <div class="input-group">
        <label>Cycle Duration (seconds)</label>
        <input type="number" id="setupDuration" value="30" min="10" max="300">
      </div>
      
      <div class="input-group">
        <label>Desensitization Continue Threshold (0-10)</label>
        <input type="number" id="setupDesens" value="5" min="0" max="10">
      </div>
      
      <div class="input-group">
        <label>Belief Stop Threshold (0-7)</label>
        <input type="number" id="setupBelief" value="5" min="0" max="7">
      </div>
      
      <button class="btn next" id="setupNext">Begin Session</button>
    </div>

    <div id="guidedDesens" class="screen">
      <h2>Desensitization Phase</h2>
      <div class="prompt">Focus on the target event</div>
      
      <div class="status stopped" id="statusDesens">Stopped</div>
      <div class="progress" id="progressDesens">0:00 / 0:30</div>
      
      <div id="desensControls">
        <div class="toggles">
          <button class="toggle on" id="visDesens">Visual</button>
          <button class="toggle on" id="audDesens">Audio</button>
          <button class="toggle on" id="tacDesens">Tactile</button>
        </div>
        
        <div class="group">
          <label>Speed</label>
          <input type="range" id="spdDesens" min="200" max="3000" value="1000" step="100">
          <div class="val" id="spdDesensV">1000 ms</div>
        </div>
        
        <div class="group">
          <label>Brightness</label>
          <input type="range" id="briDesens" min="10" max="255" value="128">
          <div class="val" id="briDesensV">128</div>
        </div>
        
        <div class="group">
          <label>Color</label>
          <input type="color" id="colDesens" value="#ffffff">
        </div>
        
        <div class="group">
          <label>Beep Frequency</label>
          <input type="range" id="bepDesens" min="200" max="2000" value="800" step="50">
          <div class="val" id="bepDesensV">800 Hz</div>
        </div>
        
        <div class="group">
          <label>Vibration</label>
          <input type="range" id="vibDesens" min="0" max="255" value="200">
          <div class="val" id="vibDesensV">200</div>
        </div>
      </div>
      
      <button class="btn start" id="startDesens">Start Cycle</button>
      <button class="btn stop" id="stopDesens" style="display:none;">Stop</button>
    </div>

    <div id="guidedDesensScore" class="screen">
      <h2>Desensitization Phase</h2>
      <div class="prompt">Rate your current level of disturbance (0-10)<br>0 = No disturbance, 10 = Highest disturbance</div>
      
      <div class="scale-input">
        <label>Score:</label>
        <input type="number" id="desensScore" value="5" min="0" max="10">
      </div>
      
      <button class="btn next" id="desensScoreNext">Continue</button>
      <button class="btn reset" id="resetFromDesens">Reset Session</button>
    </div>

    <div id="guidedReprocess" class="screen">
      <h2>Reprocessing Phase</h2>
      <div class="prompt">Focus on the positive belief</div>
      
      <div class="status stopped" id="statusReprocess">Stopped</div>
      <div class="progress" id="progressReprocess">0:00 / 0:30</div>
      
      <div id="reprocessControls">
        <div class="toggles">
          <button class="toggle on" id="visReprocess">Visual</button>
          <button class="toggle on" id="audReprocess">Audio</button>
          <button class="toggle on" id="tacReprocess">Tactile</button>
        </div>
        
        <div class="group">
          <label>Speed</label>
          <input type="range" id="spdReprocess" min="200" max="3000" value="1000" step="100">
          <div class="val" id="spdReprocessV">1000 ms</div>
        </div>
        
        <div class="group">
          <label>Brightness</label>
          <input type="range" id="briReprocess" min="10" max="255" value="128">
          <div class="val" id="briReprocessV">128</div>
        </div>
        
        <div class="group">
          <label>Color</label>
          <input type="color" id="colReprocess" value="#ffffff">
        </div>
        
        <div class="group">
          <label>Beep Frequency</label>
          <input type="range" id="bepReprocess" min="200" max="2000" value="800" step="50">
          <div class="val" id="bepReprocessV">800 Hz</div>
        </div>
        
        <div class="group">
          <label>Vibration</label>
          <input type="range" id="vibReprocess" min="0" max="255" value="200">
          <div class="val" id="vibReprocessV">200</div>
        </div>
      </div>
      
      <button class="btn start" id="startReprocess">Start Cycle</button>
      <button class="btn stop" id="stopReprocess" style="display:none;">Stop</button>
    </div>

    <div id="guidedBeliefScore" class="screen">
      <h2>Reprocessing Phase</h2>
      <div class="prompt">Rate your belief in the positive statement (0-7)<br>0 = Completely false, 7 = Completely true</div>
      
      <div class="scale-input">
        <label>Score:</label>
        <input type="number" id="beliefScore" value="3" min="0" max="7">
      </div>
      
      <button class="btn next" id="beliefScoreNext">Continue</button>
      <button class="btn reset" id="resetFromBelief">Reset Session</button>
    </div>

    <div id="guidedComplete" class="screen">
      <h2>Session Complete</h2>
      <div class="prompt">Your EMDR session is complete</div>
      <button class="btn reset" id="resetComplete">Start New Session</button>
    </div>

    <!-- Manual Session Screen -->
    <div id="manualSession" class="screen">
      <h2>Manual Session</h2>
      
      <div class="status stopped" id="statusManual">Stopped</div>
      <div class="progress" id="progressManual">0:00 / 0:30</div>
      
      <div class="toggles">
        <button class="toggle on" id="visManual">Visual</button>
        <button class="toggle on" id="audManual">Audio</button>
        <button class="toggle on" id="tacManual">Tactile</button>
      </div>
      
      <div class="group">
        <label>Duration</label>
        <input type="range" id="durManual" min="10" max="300" value="30" step="10">
        <div class="val" id="durManualV">30 seconds</div>
      </div>
      
      <div class="group">
        <label>Speed</label>
        <input type="range" id="spdManual" min="200" max="3000" value="1000" step="100">
        <div class="val" id="spdManualV">1000 ms</div>
      </div>
      
      <div class="group">
        <label>Brightness</label>
        <input type="range" id="briManual" min="10" max="255" value="128">
        <div class="val" id="briManualV">128</div>
      </div>
      
      <div class="group">
        <label>Color</label>
        <input type="color" id="colManual" value="#ffffff">
      </div>
      
      <div class="group">
        <label>Beep Frequency</label>
        <input type="range" id="bepManual" min="200" max="2000" value="800" step="50">
        <div class="val" id="bepManualV">800 Hz</div>
      </div>
      
      <div class="group">
        <label>Vibration</label>
        <input type="range" id="vibManual" min="0" max="255" value="200">
        <div class="val" id="vibManualV">200</div>
      </div>
      
      <button class="btn start" id="startManual">Start</button>
      <button class="btn stop" id="stopManual">Stop</button>
    </div>
  </div>

  <script>
    let ws;
    let ctx;
    let isGuided = true;
    let currentPhase = 'setup';
    let desensThreshold = 5;
    let beliefThreshold = 5;
    
    function connect() {
      ws = new WebSocket('ws://' + location.hostname + ':81');
      
      ws.onopen = () => {
        console.log('Connected');
        ws.send('getState');
      };
      
      ws.onmessage = (e) => {
        const d = JSON.parse(e.data);
        
        if (d.type === 'state') {
          updateStatusAll(d.running);
        } else if (d.type === 'progress') {
          updateProgressAll(d.elapsed, d.total);
        } else if (d.type === 'complete') {
          handleCycleComplete();
        } else if (d.audio) {
          beep(d.side, d.freq);
        }
      };
      
      ws.onclose = () => {
        setTimeout(connect, 2000);
      };
    }
    
    function updateStatusAll(running) {
      const status = running ? 'Running' : 'Stopped';
      const className = running ? 'status running' : 'status stopped';
      document.getElementById('statusDesens').textContent = status;
      document.getElementById('statusDesens').className = className;
      document.getElementById('statusReprocess').textContent = status;
      document.getElementById('statusReprocess').className = className;
      document.getElementById('statusManual').textContent = status;
      document.getElementById('statusManual').className = className;
      
      if (running) {
        document.getElementById('startDesens').style.display = 'none';
        document.getElementById('stopDesens').style.display = 'block';
        document.getElementById('startReprocess').style.display = 'none';
        document.getElementById('stopReprocess').style.display = 'block';
      } else {
        document.getElementById('startDesens').style.display = 'block';
        document.getElementById('stopDesens').style.display = 'none';
        document.getElementById('startReprocess').style.display = 'block';
        document.getElementById('stopReprocess').style.display = 'none';
      }
    }
    
    function updateProgressAll(elapsed, total) {
      const eMin = Math.floor(elapsed / 60);
      const eSec = elapsed % 60;
      const tMin = Math.floor(total / 60);
      const tSec = total % 60;
      const text = eMin + ':' + (eSec < 10 ? '0' : '') + eSec + ' / ' +
                   tMin + ':' + (tSec < 10 ? '0' : '') + tSec;
      document.getElementById('progressDesens').textContent = text;
      document.getElementById('progressReprocess').textContent = text;
      document.getElementById('progressManual').textContent = text;
    }
    
    function handleCycleComplete() {
      if (isGuided) {
        if (currentPhase === 'desens') {
          showScreen('guidedDesensScore');
        } else if (currentPhase === 'reprocess') {
          showScreen('guidedBeliefScore');
        }
      }
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
    
    function showScreen(screenId) {
      document.querySelectorAll('.screen').forEach(s => s.classList.remove('active'));
      document.getElementById(screenId).classList.add('active');
    }
    
    function setupControls(prefix) {
      document.getElementById('spd' + prefix).oninput = (e) => {
        document.getElementById('spd' + prefix + 'V').textContent = e.target.value + ' ms';
        ws.send('speed:' + e.target.value);
      };
      
      document.getElementById('bri' + prefix).oninput = (e) => {
        document.getElementById('bri' + prefix + 'V').textContent = e.target.value;
        ws.send('brightness:' + e.target.value);
      };
      
      document.getElementById('col' + prefix).oninput = (e) => {
        const h = e.target.value;
        const r = parseInt(h.substr(1,2), 16);
        const g = parseInt(h.substr(3,2), 16);
        const b = parseInt(h.substr(5,2), 16);
        ws.send('color:' + r + ',' + g + ',' + b);
      };
      
      document.getElementById('bep' + prefix).oninput = (e) => {
        document.getElementById('bep' + prefix + 'V').textContent = e.target.value + ' Hz';
        ws.send('beep:' + e.target.value);
      };
      
      document.getElementById('vib' + prefix).oninput = (e) => {
        document.getElementById('vib' + prefix + 'V').textContent = e.target.value;
        ws.send('vibe:' + e.target.value);
      };
      
      document.getElementById('vis' + prefix).onclick = (e) => {
        e.target.classList.toggle('on');
        ws.send('visual:' + (e.target.classList.contains('on') ? '1' : '0'));
      };
      
      document.getElementById('aud' + prefix).onclick = (e) => {
        e.target.classList.toggle('on');
        ws.send('audio:' + (e.target.classList.contains('on') ? '1' : '0'));
      };
      
      document.getElementById('tac' + prefix).onclick = (e) => {
        e.target.classList.toggle('on');
        ws.send('tactile:' + (e.target.classList.contains('on') ? '1' : '0'));
      };
    }
    
    // Mode selection
    document.getElementById('guidedBtn').onclick = () => {
      isGuided = true;
      document.getElementById('guidedBtn').classList.add('active');
      document.getElementById('manualBtn').classList.remove('active');
      showScreen('guidedSetup');
    };
    
    document.getElementById('manualBtn').onclick = () => {
      isGuided = false;
      document.getElementById('manualBtn').classList.add('active');
      document.getElementById('guidedBtn').classList.remove('active');
      showScreen('manualSession');
    };
    
    // Guided session setup
    document.getElementById('setupNext').onclick = () => {
      const duration = parseInt(document.getElementById('setupDuration').value);
      desensThreshold = parseInt(document.getElementById('setupDesens').value);
      beliefThreshold = parseInt(document.getElementById('setupBelief').value);
      
      ws.send('cycleDuration:' + duration);
      ws.send('desensThreshold:' + desensThreshold);
      ws.send('beliefThreshold:' + beliefThreshold);
      
      currentPhase = 'desens';
      showScreen('guidedDesens');
    };
    
    // Desensitization phase
    document.getElementById('startDesens').onclick = () => {
      if (!ctx) ctx = new (window.AudioContext || window.webkitAudioContext)();
      ws.send('start');
    };
    
    document.getElementById('stopDesens').onclick = () => {
      ws.send('stop');
    };
    
    document.getElementById('desensScoreNext').onclick = () => {
      const score = parseInt(document.getElementById('desensScore').value);
      if (score > desensThreshold) {
        showScreen('guidedDesens');
      } else {
        currentPhase = 'reprocess';
        showScreen('guidedReprocess');
      }
    };
    
    // Reprocessing phase
    document.getElementById('startReprocess').onclick = () => {
      if (!ctx) ctx = new (window.AudioContext || window.webkitAudioContext)();
      ws.send('start');
    };
    
    document.getElementById('stopReprocess').onclick = () => {
      ws.send('stop');
    };
    
    document.getElementById('beliefScoreNext').onclick = () => {
      const score = parseInt(document.getElementById('beliefScore').value);
      if (score < beliefThreshold) {
        showScreen('guidedReprocess');
      } else {
        showScreen('guidedComplete');
      }
    };
    
    // Reset buttons
    document.getElementById('resetFromDesens').onclick = () => {
      showScreen('guidedSetup');
      ws.send('stop');
    };
    
    document.getElementById('resetFromBelief').onclick = () => {
      showScreen('guidedSetup');
      ws.send('stop');
    };
    
    document.getElementById('resetComplete').onclick = () => {
      showScreen('guidedSetup');
    };
    
    // Manual session
    document.getElementById('durManual').oninput = (e) => {
      document.getElementById('durManualV').textContent = e.target.value + ' seconds';
      ws.send('cycleDuration:' + e.target.value);
    };
    
    document.getElementById('startManual').onclick = () => {
      if (!ctx) ctx = new (window.AudioContext || window.webkitAudioContext)();
      ws.send('start');
    };
    
    document.getElementById('stopManual').onclick = () => {
      ws.send('stop');
    };
    
    // Setup all control handlers
    setupControls('Desens');
    setupControls('Reprocess');
    setupControls('Manual');
    
    connect();
  </script>
</body>
</html>
)rawliteral";
}
