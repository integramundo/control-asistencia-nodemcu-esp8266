#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h> // Include ArduinoJson library

class Usuario
{
private:
  const char *nombre;
  const char *clave;

public:
  // Constructor
  Usuario(const char *nombre, const char *clave) : nombre(nombre), clave(clave) {}

  const char *obtenerNombre()
  {
    return nombre;
  }

  const char *obtenerClave()
  {
    return clave;
  }
};

/* const int D0 = 16; // GPIO16 - WAKE UP
const int D1 = 5;  // GPIO5
const int D2 = 4;  // GPIO4
const int D3 = 0;  // GPIO0
const int D4 = 2;  // GPIO2 - TXD1
const int D5 = 14; // GPIO14 - HSCLK
const int D6 = 12; // GPIO12 - HMISO
const int D7 = 13; // GPIO13 - HMOSI - RXD2
const int D8 = 15; // GPIO15 - HCS   - TXD2
const int RX = 3;  // GPIO3 - RXD0
const int TX = 1;  // GPIO1 - TXD0
const int S3 = 10;
const int S2 = 9; */

const int SS_PIN = D8;  // SDA RC522
const int RST_PIN = D0; // RST RC522
const int BUZZER = D1;
const int BOTON = D2;
const int LED_VERDE = D3;
const int LED_ROJO = D4;

// para debounce del boton
bool estadoAnteriorBoton = HIGH; // Estado inicial del botón (no presionado)
unsigned long ultimaPulsacion = 0;
unsigned long intervaloDebounce = 100;

// para incializar el scanner rfid
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;
// Init array that will store new NUID
byte nuidPICC[4];

// para almacenar el codigo de la tarjet rfid
String DatoHex;

// certificado raíz de onrender.com
const char IRG_Root_X1[] PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)CERT";
X509List cert(IRG_Root_X1);

const char *ssid = "";
const char *password = "";

String modo = "check-out";

String url = "https://flask-asistencia-odoo-integramundo.onrender.com/hello/integramundo";

// importante almacenar los usuarios como url enconded string
Usuario usuarios[] = {
    Usuario("Master Key", "D3649A0D"),
    Usuario("Master Card", "F3073CF6"),
    Usuario("User 1", "8493398F"),
    Usuario("User 2", "44130E8F"),
    Usuario("User 3", "742E2A8F"),
    Usuario("User 4", "142CEB8E"),
    Usuario("User 5", "A4B68D8F"),
    Usuario("User 6", "A4F2948F"),
    Usuario("User 7", "A4BFA98F"),
    Usuario("User 8", "54DF9A8F"),
    Usuario("User 9", "54A3648F"),
    Usuario("User 10", "B450438F"),
    Usuario("User 11", "7460DC8F"),
    Usuario("User 12", "A468798F"),
    Usuario("User 13", "44BC2E8F"),
    Usuario("User 14", "8477858F"),
    Usuario("User 15", "D6B06172"),
    Usuario("User 16", "9497928F"),
    Usuario("User 17", "7481648F"),
    Usuario("User 18", "74988C8F"),
    Usuario("User 19", "94350B8F"),
    Usuario("User 20", "17DA3F5F")};
int numUsuarios = sizeof(usuarios) / sizeof(usuarios[0]); // Calculate number of users

void parpadearLed(int pin, int ms)
{
  digitalWrite(pin, HIGH);
  delay(ms);
  digitalWrite(pin, LOW);
  delay(ms);
}

void pulsarBuzzer(int buzzer, int ms)
{
  // LOW PRENDE HIGH APAGA
  digitalWrite(buzzer, LOW);
  delay(ms);
  digitalWrite(buzzer, HIGH);
  delay(ms);
}

void imprimirUsuarios(Usuario usuarios[], int size)
{
  for (int i = 0; i < size; ++i)
  {
    Serial.print(usuarios[i].obtenerNombre());
    Serial.print(usuarios[i].obtenerClave());
  }
}

void registrarUsuario(const char *nombre)
{
  String nombreUsuario = nombre;
  Serial.print("Autenticando a ");
  Serial.println(nombreUsuario);

  WiFiClientSecure client;

  if (WiFi.status() == WL_CONNECTED)
  {
    client.setTrustAnchors(&cert); // Ensure you have the root certificate set up properly

    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");

    String url = "https://flask-asistencia-odoo-integramundo.onrender.com/post_attendance";

    // Prepare JSON data
    JsonDocument doc;
    doc["file_path"] = "credentials.txt";
    doc["action"] = (modo == "check-in") ? "check-in" : "check-out";
    doc["username"] = nombreUsuario;

    // Serialize JSON object
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);

    Serial.print("[HTTPS] POST...\n");
    if (https.begin(client, url))
    {                                                      // Initialize HTTPClient with the server URL
      https.addHeader("Content-Type", "application/json"); // Add the content type header

      int httpCode = https.POST(jsonBuffer); // Send the request with the JSON payload

      if (httpCode > 0)
      {
        Serial.printf("[HTTPS] POST... code: %d\n", httpCode);
        String payload = https.getString();
        Serial.println(payload);

        if (httpCode == HTTP_CODE_OK)
        {
          pulsarBuzzer(BUZZER, 150);
          pulsarBuzzer(BUZZER, 150);
        }
        else if (httpCode == HTTP_CODE_NOT_FOUND)
        {
          Serial.println("Usuario no existe en la base de datos.");
          pulsarBuzzer(BUZZER, 150);
          pulsarBuzzer(BUZZER, 150);
          pulsarBuzzer(BUZZER, 150);
        }
        else if (httpCode == HTTP_CODE_BAD_REQUEST)
        {
          Serial.println("Usuario ya registró su ingreso.");
          pulsarBuzzer(BUZZER, 300);
          pulsarBuzzer(BUZZER, 300);
          pulsarBuzzer(BUZZER, 300);
        }
      }
      else
      {
        Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
        pulsarBuzzer(BUZZER, 150);
        pulsarBuzzer(BUZZER, 150);
        pulsarBuzzer(BUZZER, 150);
      }
      https.end(); // Close the connection
    }
    else
    {
      Serial.println("[HTTPS] Unable to begin connection");
    }
  }
  else
  {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

void comprobarClave(String clave, Usuario usuarios[], int size)
{
  // importante convertir a char* porque en la clase Usuario usa char* para almacenar los strings
  const char *claveChar = clave.c_str(); // Convert String to const char*
  for (int i = 0; i < size; ++i)
  {
    if (strcmp(claveChar, usuarios[i].obtenerClave()) == 0)
    {
      // pasamos el nombre de usuario a la funcion que luego lo agregara como query param
      registrarUsuario(usuarios[i].obtenerNombre());
      return;
    }
  }

  // solo se imprime si no hay return anterior, o sea si no se encuentra la clave en el array
  Serial.println("NO ESTA REGISTRADO - PROHIBIDO EL INGRESO");
  pulsarBuzzer(BUZZER, 150);
  pulsarBuzzer(BUZZER, 150);
  pulsarBuzzer(BUZZER, 150);
}

void cambiarModo(int led1, int led2)
{
  if (modo == "check-in")
  {
    modo = "check-out";
    digitalWrite(led1, HIGH);
    digitalWrite(led2, LOW);
  }
  else if (modo == "check-out")
  {
    modo = "check-in";
    digitalWrite(led2, HIGH);
    digitalWrite(led1, LOW);
  }
}

// para extraer el numero contenido en la tarjeta rfid
String printHex(byte *buffer, byte bufferSize)
{
  String DatoHexAux = "";
  for (byte i = 0; i < bufferSize; i++)
  {
    if (buffer[i] < 0x10)
    {
      DatoHexAux = DatoHexAux + "0";
      DatoHexAux = DatoHexAux + String(buffer[i], HEX);
    }
    else
    {
      DatoHexAux = DatoHexAux + String(buffer[i], HEX);
    }
  }

  for (unsigned int i = 0; i < DatoHexAux.length(); i++)
  {
    DatoHexAux[i] = toupper(DatoHexAux[i]);
  }
  return DatoHexAux;
}

void setup()
{
  pinMode(BOTON, INPUT_PULLUP);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // partir con el buzzer apagado
  digitalWrite(BUZZER, HIGH);

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  SPI.begin();     // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  Serial.println();
  Serial.print(F("Reader :"));
  rfid.PCD_DumpVersionToSerial();
  for (byte i = 0; i < 6; i++)
  {
    key.keyByte[i] = 0xFF;
  }
  DatoHex = printHex(key.keyByte, MFRC522::MF_KEY_SIZE);
  Serial.println();
  Serial.println();
  Serial.println("Iniciando el Programa");

  Serial.println();
  Serial.println();
  Serial.println();

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }

  // Set time via NTP, as required for x.509 validation
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));

  // partimos en modo check-in
  cambiarModo(LED_VERDE, LED_ROJO);
}

void loop()
{
  bool estadoBoton = digitalRead(BOTON);
  unsigned long tiempoActual = millis();

  // Verifica si ha pasado suficiente tiempo desde la última pulsación
  if (tiempoActual - ultimaPulsacion >= intervaloDebounce)
  {
    // Verifica si el botón ha cambiado de no presionado a presionado
    if (estadoBoton == LOW && estadoAnteriorBoton == HIGH)
    {
      cambiarModo(LED_VERDE, LED_ROJO);
      ultimaPulsacion = tiempoActual;
    }
    // Actualiza el estado anterior del botón
    estadoAnteriorBoton = estadoBoton;
  }

  // Reset the loop if no new card present on the sensor/reader. This saves the
  // entire process when idle.
  if (!rfid.PICC_IsNewCardPresent())
  {
    return;
  }

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial())
  {
    return;
  }

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));
  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K)
  {
    Serial.println("Su Tarjeta no es del tipo MIFARE Classic.");
    return;
  }

  Serial.println("Se ha detectado una nueva tarjeta.");

  pulsarBuzzer(BUZZER, 150);

  DatoHex = printHex(rfid.uid.uidByte, rfid.uid.size);
  Serial.print("Codigo Tarjeta: ");
  Serial.println(DatoHex);

  // verificar si existe esta clave entre nuestros usuarios registrados
  comprobarClave(DatoHex, usuarios, numUsuarios);

  Serial.println();

  // Halt PICC
  rfid.PICC_HaltA();
  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}