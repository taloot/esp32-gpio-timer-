#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <time.h>

// WiFi credentials
char ssid[32] = "ur ssid";
char pass[64] = "passwod";

// Server and configuration variables
WebServer server(80); 
uint8_t gpioCount = 0;
uint8_t gmtOffset = 3; // GMT offset in hours

struct GPIO {
    uint8_t pin;
    struct Schedule {
        uint8_t startHour;
        uint8_t startMinute;
        uint8_t endHour;
        uint8_t endMinute;
    } schedule;
};

GPIO gpios[10]; // Array to hold up to 10 GPIOs

// Function declarations
void saveConfig();
void loadConfig();
void calculateCPULoad();
inline void toggleGPIO();
void setGPIO();
void addGPIO();
void removeGPIO();
void setSchedule();
String generateHTML();
String generateOptions(int min, int max, int selected);
void checkSchedules();
void getTime();
void getUptime();
void cpuLoad();
void formatSPIFFS();
void setGMT();
void getStatus();
void scanNetworks();
void handleNetworkConnect();
void handleGetNetworks();

void setup() {
  Serial.begin(115200); 
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  if (!SPIFFS.begin(true)) return; 
  loadConfig(); 
  configTime(gmtOffset * 3600, 0, "pool.ntp.org");
  for (uint8_t i = 0; i < gpioCount; i++) { 
    pinMode(gpios[i].pin, OUTPUT); 
    digitalWrite(gpios[i].pin, LOW); 
  }
  server.on("/", handleRoot); 
  server.on("/t", toggleGPIO); 
  server.on("/sg", setGPIO); 
  server.on("/ag", addGPIO); 
  server.on("/rg", removeGPIO); 
  server.on("/st", setSchedule); 
  server.on("/gt", getTime); 
  server.on("/uptime", getUptime); 
  server.on("/cpuload", cpuLoad); 
  server.on("/fs", formatSPIFFS); 
  server.on("/sgmt", setGMT); 
  server.on("/status", getStatus); 
  server.on("/scan", scanNetworks);
  server.on("/connect", handleNetworkConnect);
  server.on("/getNetworks", handleGetNetworks);
  server.begin();
}

void loop() { 
  server.handleClient(); 
  checkSchedules(); 
  calculateCPULoad();
}

void handleRoot() {
  String page = "<html><head><title>ESP32</title><link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css'></head><body class='container'><h1 class='mt-4'>GPIO Control</h1>";
  page += "<div class='alert alert-info p-2'><span id='time'>Time...</span> | <span id='uptime'>Uptime...</span> | <span id='cpuLoad'>CPU Load...</span></div>" + generateHTML();
  page += "<div class='form-group'><label for='gmt'>GMT Offset:</label><input type='number' id='gmt' class='form-control form-control-sm' value='" + String(gmtOffset) + "'><button class='btn btn-primary btn-sm mt-1' onclick=\"setGMT()\">Set GMT</button></div>";
  page += "<script>function updateTime() { fetch('/gt').then(response => response.text()).then(data => { document.getElementById('time').innerHTML = data; }); } setInterval(updateTime, 1000); updateTime(); function setGMT() { location.href='/sgmt?o=' + document.getElementById('gmt').value; } function updateUptime() { fetch('/uptime').then(response => response.text()).then(data => { document.getElementById('uptime').innerHTML = data; }); } setInterval(updateUptime, 1000); updateUptime(); function updateCpuLoad() { fetch('/cpuload').then(response => response.text()).then(data => { document.getElementById('cpuLoad').innerHTML = data; }); } setInterval(updateCpuLoad, 1000); updateCpuLoad();</script>";
  page += "<script>function updateStatus() { fetch('/status').then(response => response.json()).then(data => { for (var i = 0; i < data.length; i++) { document.getElementById('led' + i).className = data[i] ? 'badge badge-success' : 'badge badge-danger'; document.getElementById('led' + i).innerHTML = data[i] ? 'ON' : 'OFF'; } }); } setInterval(updateStatus, 1000); updateStatus();</script>";
  page += "<div class='form-group'><label for='networks'>Available Networks:</label><select id='networks' class='form-control form-control-sm'></select></div><div class='form-group'><label for='ssid'>SSID:</label><input type='text' id='ssid' class='form-control form-control-sm'><label for='password'>Password:</label><input type='password' id='password' class='form-control form-control-sm'><button class='btn btn-primary btn-sm mt-1' onclick=\"connectToNetwork()\">Connect</button></div>";
  page += "<script>function scanNetworks() { fetch('/scan').then(response => response.json()).then(data => { let networksSelect = document.getElementById('networks'); networksSelect.innerHTML = ''; data.forEach(network => { let option = document.createElement('option'); option.value = network; option.text = network; networksSelect.add(option); }); }); } scanNetworks(); function connectToNetwork() { let ssid = document.getElementById('ssid').value; let password = document.getElementById('password').value; fetch('/connect?ssid=' + ssid + '&password=' + password).then(response => response.text()).then(data => { alert(data); }); }</script>";
  page += "<button class='btn btn-danger btn-sm mt-1' onclick=\"location.href='/fs'\">Format SPIFFS</button></body></html>"; 
  server.send(200, "text/html", page);
}
inline void toggleGPIO() { 
  int index = server.arg("i").toInt(); 
  digitalWrite(gpios[index].pin, !digitalRead(gpios[index].pin)); 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void setGPIO() { 
  int index = server.arg("i").toInt();
  uint8_t pin = server.arg("g").toInt(); 
  gpios[index].pin = pin; 
  pinMode(pin, OUTPUT); 
  digitalWrite(pin, LOW); 
  saveConfig(); 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void addGPIO() { 
  if (gpioCount < 10) { 
    uint8_t pin = server.arg("g").toInt(); 
    gpios[gpioCount++].pin = pin; 
    pinMode(pin, OUTPUT); 
    digitalWrite(pin, LOW); 
    saveConfig(); 
  } 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void removeGPIO() { 
  int index = server.arg("i").toInt(); 
  if (index < gpioCount) { 
    for (int j = index; j < gpioCount - 1; j++) { 
      gpios[j] = gpios[j + 1]; 
    } 
    gpioCount--; 
    saveConfig(); 
  } 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void setSchedule() { 
  int index = server.arg("i").toInt(); 
  gpios[index].schedule = {
    static_cast<uint8_t>(server.arg("sH").toInt()),
    static_cast<uint8_t>(server.arg("sM").toInt()),
    static_cast<uint8_t>(server.arg("eH").toInt()),
    static_cast<uint8_t>(server.arg("eM").toInt())
  }; 
  saveConfig(); 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

String generateHTML() {
  String content;
  for (int i = 0; i < gpioCount; i++) {
    content += "<div class='card mt-2 p-2 shadow-sm'><div class='card-body p-2'><h6 class='card-title text-primary'>GPIO " + String(gpios[i].pin) + " - <span id='led" + String(i) + "' class='badge " + (digitalRead(gpios[i].pin) ? "badge-success" : "badge-danger") + "'>" + (digitalRead(gpios[i].pin) ? "ON" : "OFF") + "</span></h6>";
    content += "<button class='btn btn-secondary btn-sm' onclick=\"location.href='/t?i=" + String(i) + "'\">Toggle</button>";
    content += "<div class='form-group mt-1'><label for='gpio" + String(i) + "'>Set GPIO:</label><input type='number' value='" + String(gpios[i].pin) + "' id='gpio" + String(i) + "' class='form-control form-control-sm'><button class='btn btn-primary btn-sm mt-1' onclick=\"location.href='/sg?i=" + String(i) + "&g=' + document.getElementById('gpio" + String(i) + "').value\">Set</button></div>";
    content += "<button class='btn btn-danger btn-sm' onclick=\"location.href='/rg?i=" + String(i) + "'\">Remove</button>";
    content += "<div class='form-group mt-1'><label>Schedule:</label><div class='form-row'><div class='col'><select id='sH" + String(i) + "' class='form-control form-control-sm'>" + generateOptions(0, 23, gpios[i].schedule.startHour) + "</select></div><div class='col'><select id='sM" + String(i) + "' class='form-control form-control-sm'>" + generateOptions(0, 59, gpios[i].schedule.startMinute) + "</select></div></div>";
    content += "<div class='form-row mt-1'><div class='col'><select id='eH" + String(i) + "' class='form-control form-control-sm'>" + generateOptions(0, 23, gpios[i].schedule.endHour) + "</select></div><div class='col'><select id='eM" + String(i) + "' class='form-control form-control-sm'>" + generateOptions(0, 59, gpios[i].schedule.endMinute) + "</select></div></div>";
    content += "<button class='btn btn-primary btn-sm mt-1' onclick=\"location.href='/st?i=" + String(i) + "&sH=' + document.getElementById('sH" + String(i) + "').value + '&sM=' + document.getElementById('sM" + String(i) + "').value + '&eH=' + document.getElementById('eH" + String(i) + "').value + '&eM=' + document.getElementById('eM" + String(i) + "').value\">Set Times</button></div></div></div>";
  }
  content += "<div class='form-group mt-2'><label for='newGpio'>New GPIO:</label><input type='number' id='newGpio' class='form-control form-control-sm'><button class='btn btn-primary btn-sm mt-1' onclick=\"location.href='/ag?g=' + document.getElementById('newGpio').value\">Add</button></div>";
  return content;
}

String generateOptions(int min, int max, int selected) {
  String options = "";
  for (int i = min; i <= max; i++) {
    options += "<option value='" + String(i) + "'" + (i == selected ? " selected" : "") + ">" + (i < 10 ? "0" : "") + String(i) + "</option>";
  }
  return options;
}
void checkSchedules() {
  struct tm timeinfo; 
  if (!getLocalTime(&timeinfo)) return;

  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  for (uint8_t i = 0; i < gpioCount; i++) {
    int startMinutes = gpios[i].schedule.startHour * 60 + gpios[i].schedule.startMinute;
    int endMinutes = gpios[i].schedule.endHour * 60 + gpios[i].schedule.endMinute;
    digitalWrite(gpios[i].pin, (currentMinutes >= startMinutes && currentMinutes < endMinutes) ? HIGH : LOW);
  }
}

void saveConfig() {
  File file = SPIFFS.open("/config.txt", FILE_WRITE);
  if (!file) return;

  file.print(gpioCount);
  for (uint8_t i = 0; i < gpioCount; i++) {
    file.printf(",%d,%d,%d,%d,%d", gpios[i].pin, gpios[i].schedule.startHour, gpios[i].schedule.startMinute, gpios[i].schedule.endHour, gpios[i].schedule.endMinute);
  }
  file.printf(",%d,%s,%s", gmtOffset, ssid, pass);
  file.close();
}

void loadConfig() {
  File file = SPIFFS.open("/config.txt", FILE_READ);
  if (!file) return;

  String configData = file.readString(); 
  file.close();
  if (configData.length() > 0) {
    int start = 0, end = configData.indexOf(',');
    gpioCount = configData.substring(start, end).toInt(); 
    start = end + 1;
    
    for (uint8_t i = 0; i < gpioCount; i++) {
      end = configData.indexOf(',', start); gpios[i].pin = configData.substring(start, end).toInt(); start = end + 1;
      end = configData.indexOf(',', start); gpios[i].schedule.startHour = configData.substring(start, end).toInt(); start = end + 1;
      end = configData.indexOf(',', start); gpios[i].schedule.startMinute = configData.substring(start, end).toInt(); start = end + 1;
      end = configData.indexOf(',', start); gpios[i].schedule.endHour = configData.substring(start, end).toInt(); start = end + 1;
      end = configData.indexOf(',', start); gpios[i].schedule.endMinute = configData.substring(start, end).toInt(); start = end + 1;
    }
    end = configData.indexOf(',', start);
    gmtOffset = configData.substring(start, end).toInt();
    start = end + 1;
    end = configData.indexOf(',', start);
    String ssidStr = configData.substring(start, end);
    strncpy(ssid, ssidStr.c_str(), sizeof(ssid));
    start = end + 1;
    String passStr = configData.substring(start);
    strncpy(pass, passStr.c_str(), sizeof(pass));
  }
}

void getTime() {
  struct tm timeinfo; 
  if (!getLocalTime(&timeinfo)) { 
    server.send(200, "text/plain", "Failed"); 
    return; 
  }
  char timeString[64]; 
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo); 
  server.send(200, "text/plain", timeString);
}

void getUptime() {
  long seconds = millis() / 1000;
  long minutes = seconds / 60;
  long hours = minutes / 60;
  long days = hours / 24;
  seconds = seconds % 60;
  minutes = minutes % 60;
  hours = hours % 24;
  char uptimeString[64];
  snprintf(uptimeString, sizeof(uptimeString), "%ldd %02ld:%02ld:%02ld", days, hours, minutes, seconds);
  server.send(200, "text/plain", uptimeString);
}

void calculateCPULoad() {
  // Placeholder function to simulate CPU load calculation
}

void cpuLoad() {
  static unsigned long lastIdleTime = 0;
  static unsigned long lastTotalTime = 0;
  
  unsigned long idleTime = millis() - lastIdleTime;
  lastIdleTime = millis();
  
  unsigned long totalTime = millis() - lastTotalTime;
  lastTotalTime = millis();
  
  float load = 100.0 * (1.0 - ((float)idleTime / (float)totalTime));
  char loadString[16];
  snprintf(loadString, sizeof(loadString), "%.2f%%", load);
  server.send(200, "text/plain", loadString);
}
void formatSPIFFS() { 
  SPIFFS.format(); 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void setGMT() { 
  gmtOffset = server.arg("o").toInt(); 
  configTime(gmtOffset * 3600, 0, "pool.ntp.org"); 
  saveConfig(); 
  server.sendHeader("Location", "/"); 
  server.send(303); 
}

void getStatus() {
  String status = "[";
  for (uint8_t i = 0; i < gpioCount; i++) {
    if (i > 0) status += ",";
    status += digitalRead(gpios[i].pin) ? "true" : "false";
  }
  status += "]";
  server.send(200, "application/json", status);
}

void scanNetworks() {
  int n = WiFi.scanNetworks();
  String networks = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) networks += ",";
    networks += "\"" + WiFi.SSID(i) + "\"";
  }
  networks += "]";
  server.send(200, "application/json", networks);
}

void handleNetworkConnect() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    attempt++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    strncpy(::ssid, ssid.c_str(), sizeof(::ssid));
    strncpy(::pass, password.c_str(), sizeof(::pass));
    saveConfig();
    server.send(200, "text/plain", "Connected");
  } else {
    server.send(200, "text/plain", "Failed to Connect");
  }
}

void handleGetNetworks() {
  String networks = "";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    networks += (WiFi.SSID(i) + "\n");
  }
  server.send(200, "text/plain", networks);
}
