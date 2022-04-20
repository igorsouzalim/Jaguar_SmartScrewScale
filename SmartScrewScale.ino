#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "HX711.h"
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <ThingerESP32.h>


#define totalParafusos 110 //editar conforme qtd utilizada
#define LED 36 //quantidade de leds na fita
//Thingerio
#define USERNAME "LaboratorioAberto"
#define DEVICE_ID "jaguarEsp32"
#define DEVICE_CREDENTIAL "7DkKmMQdhec%gYdC"

//Wi-Fi
#define SSID "note"
#define SSID_PASSWORD "eletronica"

//FreeRTOS
#define CORE_0 0 
#define CORE_1 1

//Fita de LED
#define PIN        4 // On Trinket or Gemma, suggest changing this to 1
#define NUMPIXELS 36 // Popular NeoPixel ring size
#define BRIGHTNESS 255 // Set BRIGHTNESS to about 1/5 (max = 255)

#define LOADCELL_DOUT_PIN 18
#define  LOADCELL_SCK_PIN 5

#define REEDSWITCH 19


bool initCalibration = 0, scaleTaskExecuting = 0;
int32_t pesoMin,pesoMax;

//Thingerio outputs
uint16_t percent = 50,estimated = 0, status_signal= 0, alarm_system = 0, chart1 = 0 ;

//Objetos
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);
Preferences preferences;
HX711 scale;

//Tasks FREERTOS
void vTaskCalibration(void *pvParameters); 


void setup() {

  Serial.begin(115200);
  Serial.println("SmartScrewScale Demo -- IST AUTOMACAO INDUSTRAL");

  pinMode (19, INPUT);
  pinMode(2, OUTPUT);

  //EEPROM begin
  preferences.begin("Storage", false); 

  pesoMin = preferences.getLong("pesoMin", 0);
  if (pesoMin == 0){
    Serial.println("No values saved for pesoMin");
  }
  else
  Serial.println("PesoMin = " + String(pesoMin));

  pesoMax = preferences.getLong("pesoMax", 0);
  if (pesoMax == 0){
    Serial.println("No values saved for pesoMax");
  }
  else
  Serial.println("pesoMax = " + String(pesoMax));



  //LoadCell begin
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
	
  //LED begin
  pixels.begin(); 

   pixels.clear();  // turn off LEDs


  for(int j=0; j<36; j++)  
  {
    pixels.setPixelColor(j, pixels.Color(50, 230, 50)); 
    pixels.show(); 
    delay(100);
  }
  for(int j=36; j>0; j--)  
  {
    pixels.setPixelColor(j, pixels.Color(150, 0, 150)); 
    pixels.show(); 
    delay(100);
  }

  
  //Inicialization effect
  for(int i=0 ; i<2; i++)
  startLed();

  //setup thingerio connection
  thing.add_wifi(SSID, SSID_PASSWORD);
  // Input values
  thing["button1"] << digitalPin(2);
  // Output values
  thing["estimated"] >> outputValue(estimated);
  thing["percent"] >> outputValue(percent);
  thing["status_signal"] >> outputValue(status_signal);
  thing["alarm"] >> outputValue(alarm_system);
  thing["chart1"] >> outputValue(chart1);

  //Freertos setup
  xTaskCreatePinnedToCore( vTaskCalibration, "Task calibration", configMINIMAL_STACK_SIZE*2, NULL, 1, NULL, CORE_1 ); 
  xTaskCreatePinnedToCore( vTaskThingerio, "Task Thingerio", configMINIMAL_STACK_SIZE*24, NULL, 2, NULL, CORE_1 ); 
     
}

void loop() {

int32_t readPeso,nLeds;

if(initCalibration == 0)
{
  
  scaleTaskExecuting = 1;
  
  readPeso = scale.read_average(10);

  if(readPeso < pesoMin)
  readPeso = pesoMin;
  else if(readPeso > pesoMax)
  readPeso = pesoMax;
  
  nLeds = map(readPeso, pesoMin, pesoMax, 0, LED);
  percent = map(readPeso, pesoMin, pesoMax, 0, 100);
  estimated = map(readPeso, pesoMin, pesoMax, 0, totalParafusos);
  chart1 = estimated;

  Serial.print("\t| Media bruta:\t");
  Serial.println(readPeso);
  Serial.print("\t| nLeds:\t");
  Serial.println(nLeds, 1);
  Serial.print("\t| estimated:\t");
  Serial.println(estimated, 1);
  Serial.print("\t| percent:\t");
  Serial.println(percent, 1);
  scaleTaskExecuting = 0;

  scale.power_down();			        // put the ADC in sleep mode
  vTaskDelay( 20/ portTICK_PERIOD_MS ); 
  scale.power_up();





if(nLeds==0)  // standby LED
{
  StandbyLedEffect();
}
else{           // displays the LED with peso value
  pixels.clear();  // turn off LEDs

  for(int j=0; j<nLeds; j++)  
  {
    pixels.setPixelColor(j, pixels.Color(50, 230, 50)); 
  }
  pixels.show(); 
}

}

}


void vTaskThingerio(void *pvParameters)
{
  uint16_t statusCount=0;
  for(;;)
  {
    if(scaleTaskExecuting == 0)
    thing.handle();
    //else
    //{Serial.println("scaleTaskExecuting -------------- waiting for the end of the task... ");}


    status_signal = 1;

    vTaskDelay( 10/ portTICK_PERIOD_MS ); 
  }
}


void vTaskCalibration(void *pvParameters)
{
  uint8_t reedSwitchCount = 0, calibrationState = 0; // 1 = prato vazo
                                                     // 2 = com carga de referencia
                                                     // 3 = carga maxima

  for(;;)
  {

    if(digitalRead(REEDSWITCH) == 1){
      reedSwitchCount++;

      pixels.setPixelColor(reedSwitchCount, pixels.Color(150, 0, 150));
      pixels.show();   

      if(reedSwitchCount > 100){
        reedSwitchCount = 0;
      }
    }
    else{
      if(reedSwitchCount>0){
      reedSwitchCount--;
      pixels.clear(); 
      pixels.show(); 
      initCalibration = 0;
      }
    }

    if(reedSwitchCount > 0)
    initCalibration = 1;
   


    if(reedSwitchCount == 36)  // Entra no processo de calibracao
    {

      Serial.println("Entra Calibracao");

      initCalibration = 1;

      for(int i = 0;i<LED;i++) pixels.setPixelColor(i, pixels.Color(0, 0, 0)); // apaga Leds
      pixels.show();   vTaskDelay( 300/ portTICK_PERIOD_MS ); 
			     
      for(int i = 0;i<LED;i++) pixels.setPixelColor(i, pixels.Color(150, 0, 0));
      pixels.show();    vTaskDelay( 300/ portTICK_PERIOD_MS ); 

      pesoMin = scale.read_average(50);  // Le o peso do prato

      Serial.print("Calibracao -- valor prato vazio (pesoMin): ");
      Serial.println(pesoMin); 

      preferences.putLong("pesoMin", pesoMin);  // Grava valor minimo (valor do prato)

      calibrationLedEffect_1(); // tempo de espera para colocar todos os parafusos na balança

      //Deve se colocar o peso maximo de parausos neste momento
      pesoMax = scale.read_average(50);  // Le o peso do prato com a maxima quantidade de parafusos desejada
      Serial.print("Calibracao -- valor prato cheio(pesoMax): ");
      Serial.println(pesoMax); 

      preferences.putLong("pesoMax", pesoMax);  // Grava valor minimo (valor do prato)

      

      reedSwitchCount = 0;

      calibrationLedEffect_2();

      pixels.clear(); 
      pixels.show(); 

      vTaskDelay( 1000/ portTICK_PERIOD_MS ); 

      initCalibration = 0;

      
    }




    vTaskDelay( 50/ portTICK_PERIOD_MS );
  }
}

void calibrationLedEffect_1()
{
  pixels.clear(); 

  delay(500);

  for(int i=0; i<=18; i++) { 
    
    pixels.setPixelColor(i, pixels.Color(150, 0, 150));
    pixels.setPixelColor(LED-i, pixels.Color(150, 0, 150));
    pixels.show();   

    delay(50); 
  }

  for(int i=18; i>=0; i--) { 
    
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
    pixels.setPixelColor(LED-i, pixels.Color(0, 0, 0));
    pixels.show();   

    delay(1200); 
  }
}

void startLed()
{
  pixels.clear(); 

  delay(500);

  for(int i=0; i<18; i++) { 
    
    pixels.setPixelColor(i, pixels.Color(150, 0, 150));
    pixels.setPixelColor(LED-i, pixels.Color(150, 0, 150));
    pixels.show();   

    delay(50); 
  }

  for(int i=17; i>=0; i--) { 
    
    pixels.setPixelColor(i, pixels.Color(0, 150, 0));
    pixels.setPixelColor(LED-i, pixels.Color(0, 150, 0));
    pixels.show();   

    delay(50); 
  }
}

void calibrationLedEffect_2()
{
  pixels.clear(); 
  pixels.show();

  for(int i = 0;i<LED;i++) pixels.setPixelColor(i, pixels.Color(150, 0, 150));
  pixels.show(); vTaskDelay( 300/ portTICK_PERIOD_MS ); 
  for(int i = 0;i<LED;i++) pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  pixels.show(); vTaskDelay( 300/ portTICK_PERIOD_MS ); 
  for(int i = 0;i<LED;i++) pixels.setPixelColor(i, pixels.Color(150, 0, 150));
  pixels.show();  vTaskDelay( 300/ portTICK_PERIOD_MS ); 
}


void StandbyLedEffect()
{
  pixels.clear(); 
      
 for(int i = 0;i<36;i++) pixels.setPixelColor(i, pixels.Color(200, 0, 0)); pixels.show();  // apaga Leds
 delay(200);
 for(int i = 0;i<36;i++) pixels.setPixelColor(i, pixels.Color(0, 0, 0)); pixels.show();  // apaga Leds
 delay(100);
 for(int i = 0;i<36;i++) pixels.setPixelColor(i, pixels.Color(200, 0, 0)); pixels.show();  // apaga Leds
 delay(200);
 for(int i = 0;i<36;i++) pixels.setPixelColor(i, pixels.Color(0, 0, 0)); pixels.show();  // apaga Leds
 delay(350);
 
 
}
