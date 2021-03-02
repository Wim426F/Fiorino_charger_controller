#include <web_server.h>

/* Connectivity */
extern const char *sta_ssid;
extern const char *sta_password;
extern const char *ap_ssid;
extern const char *ap_password;
extern const char *ota_hostname;
extern const char *ota_password;
extern const char *dns_hostname;
extern const char *http_username;
extern const char *http_password;

const char *PARAM_INPUT_1 = "function";

MDNSResponder mdns;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocketClient *globalClient = NULL;

IPAddress local_ip(192, 168, 1, 173);
IPAddress ap_ip(192, 168, 4, 22);
IPAddress gateway(192, 168, 4, 9);
IPAddress subnet(255, 255, 255, 0);

int ssid_hidden = 0;
int max_connection = 3;
int available_networks = 0;
int wifi_timeout = 0;

void StartMdnsService()
{
  //set hostname
  if (!MDNS.begin(dns_hostname))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  MDNS.addService("http", "tcp", 80);
}

void StartWebServer()
{
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/index.html", "r"); // read file from filesystem
    request->send(page, "/index.html", "text/html", false, processor);
  });
  server
      .on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        File page = SD.open("/html/update.html", "r"); // read file from filesystem
        request->send(page, "/update", "text/html");
      })
      .setAuthentication(http_username, http_password);
  server.on("/parameters", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/parameters.html", "r");
    request->send(page, "/parameters.html", "text/html");
  });
  server.on("/datalog", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/datalog.html", "r");
    File logfile;
    request->send(page, "/datalog", "text/html", false, processor);
    String inputparam;
    // GET input1 value on fiorino.local/datalog?function=<delete>
    if (request->hasParam(PARAM_INPUT_1))
    {
      inputparam = request->getParam(PARAM_INPUT_1)->value();
    }
    if (inputparam == "delete")
    {
      logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", "w");
      logfile.print("");
      logfile.close();
    }
  });
  server.on("/logdownload", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", "r");
    request->send(page, "/logfile", "text/plain", true);
  });
  server.on("/logfile", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", "r");
    request->send(page, "/logfile", "text/plain", false);
  });
  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/test.html", "r");
    request->send(page, "/test", "text/html", false, processor);
  });
  server.on("/pureknob.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/js/pureknob.js", "r");
    request->send(page, "/pureknob.js", "text/javascript");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/css/style.css", "r");
    request->send(page, "/style.css", "text/css");
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/etc/favicon.ico", "r");
    request->send(page, "/favicon.ico", "image/vnd.microsoft.icon");
  });
  server.begin();
}

void SendSocketData()
{
  String vmin_json = "{\"vmin\":";
  vmin_json += vmin;
  vmin_json += "}";

  String vmax_json = "{\"vmax\":";
  vmax_json += vmax;
  vmax_json += "}";

  String celltemp_json = "{\"celltemp\":";
  celltemp_json += celltemp;
  celltemp_json += "}";
}

String outputState(int output)
{
  if (evse_on)
  {
    return "checked";
  }
  else
  {
    return "";
  }
}

String processor(const String &var)
{
  //Serial.println(var);
  if (var == "BUTTONPLACEHOLDER")
  {
    String buttons = "";
    buttons += "<p class=\"textstyle\">Temperature: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)celltemp;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Battery Current: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)lemsensor;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Battery Voltage: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)vtot;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Vmin: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)vmin;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Vmax: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)vmax;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Charger Speed: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)charger_duty;
    buttons += "</b>";

    //buttons += "<label class=\"button\"><input value=\"Download\" type=\"button\" oncick=\"editlog(this)\" id=\"download\" " + outputState(2) + "></label>";
    buttons += "<label class=\"button\"><input value=\"Delete\" type=\"button\" onclick=\"editlog(this)\" id=\"delete\" " + outputState(4) + "></label>";
    return buttons;
  }
  return String();
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.println("WebSocket Client connected!");
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.println("Websocket Client disconnected!");
  }
}