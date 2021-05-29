#include <web_server.h>
#include <charger.h>

/* Connectivity */
extern const char *sta_ssid;
extern const char *sta_password;
extern const char *ap_ssid;
extern const char *password;
extern const char *ota_hostname;
extern const char *password;
extern const char *dns_hostname;
extern const char *ap_ssid;
extern const char *password;

const char *PARAM_1 = "function";
const char *PARAM_2 = "command";

MDNSResponder mdns;
AsyncWebServer server(80);
IPAddress ap_ip(192, 168, 4, 22);
IPAddress gateway(192, 168, 4, 9);
IPAddress subnet(255, 255, 255, 0);

int available_networks = 0;
int wifi_timeout = 0;

void StartMdnsService()
{
  mdns.addService("http", "tcp", 80);

  if (!mdns.begin(dns_hostname))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  else
  {
    Serial.println((String) "mdns: http://" + dns_hostname + ".local");
  }
}

void ConfigWebServer()
{
  /* Webpages */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/index.html", FILE_READ); // read file from filesystem
    request->send(page, "/index.html", "text/html", false);
  });
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/update.html", FILE_READ); // read file from filesystem
    request->send(page, "/update", "text/html");
  });
  server.on("/parameters", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/parameters.html", FILE_READ);
    request->send(page, "/parameters.html", "text/html");
  });
  server.on("/serialport", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/serialport.html", FILE_READ);
    request->send(page, "/serialport.html", "text/html");
    String inputparam;
    // GET input1 value on fiorino.local/serialport?function=<>
    if (request->hasParam(PARAM_2))
    {
      inputparam = request->getParam(PARAM_2)->value();
      Serial.println(inputparam);
    }
  });
  server.on("/datalog", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/datalog.html", FILE_READ);
    request->send(page, "/datalog", "text/html", false);
    String inputparam;
    // GET input1 value on fiorino.local/datalog?function=<delete>
    if (request->hasParam(PARAM_1))
    {
      inputparam = request->getParam(PARAM_1)->value();
    }
    if (inputparam == "delete")
    {
      Serial.println("file deleted");
      dataLogger("clear");
      request->send(200);
    }
  });
  server.on("/logfile", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_READ);
    request->send(page, "/logfile", "text/plain", false);
  });

  /* Global files */
  server.on("/pureknob.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/js/pureknob.js", FILE_READ);
    request->send(page, "/pureknob.js", "text/javascript");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/css/style.css", FILE_READ);
    request->send(page, "/style.css", "text/css");
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/etc/favicon.ico", FILE_READ);
    request->send(page, "/favicon.ico", "image/vnd.microsoft.icon");
  });

  /* Updating variables */
  server.on("/vmin", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(str_vmin);
    request->send_P(200, "text/plain", str_vmin.c_str());
  });
  server.on("/vmax", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(str_vmax);
    request->send_P(200, "text/plain", str_vmax.c_str());
  });
  server.on("/vtot", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(str_vtot);
    request->send_P(200, "text/plain", str_vtot.c_str());
  });
  server.on("/temp", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(str_ctmp);
    request->send_P(200, "text/plain", str_ctmp.c_str());
  });
  server.on("/soc", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(str_soc);
    request->send_P(200, "text/plain", str_soc.c_str());
  });
  server.on("/amps", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(str_dc_amps);
    request->send_P(200, "text/plain", str_dc_amps.c_str());
  });

  server.begin();
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  //Handle upload
  Serial.println("----UPLOAD-----");
  Serial.print("FILENAME: ");
  Serial.println(filename);
  Serial.print("INDEX: ");
  Serial.println(index);
  Serial.print("LENGTH: ");
  Serial.println(len);
  AsyncWebHeader *header = request->getHeader("X-File-Size");
  Serial.print("File size: ");
  Serial.println((size_t)header->value().toFloat());
  if (!Update.isRunning())
  {
    Serial.print("Status Update.begin(): ");
    Serial.println(Update.begin((size_t)header->value().toFloat()));
    Serial.print("Update remaining: ");
    Serial.println(Update.remaining());
  }
  else
  {
    Serial.println("Status Update.begin(): RUNNING");
  }
  Serial.print("FLASH BYTES: ");
  Serial.println(Update.write(data, len));
  Serial.print("Update remaining: ");
  Serial.println(Update.remaining());

  if (final)
  {
    Update.end();
    Serial.print("----FINAL-----");
  }
}