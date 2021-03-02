#include <web_updater.h>

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

/* void handleUploadWebpage(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    request->_tempFile = SD.open(filename, "w");
  }
  if (request->_tempFile)
  {
    if (len)
    {
      request->_tempFile.write(data, len);
    }
    if (final)
    {
      request->_tempFile.close();
    }
  }
  File page = SD.open("/html/update.html", "r"); // read file from filesystem
  request->send(page, "/update", "text/html");
}
*/

void handleUploadWebpage(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    Serial.printf("UploadStart: %s\n", filename.c_str());
  }
  for (size_t i = 0; i < len; i++)
  {
    Serial.write(data[i]);
  }
  if (final)
  {
    Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
  }
}
