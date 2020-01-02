/*V_Meter_State
 * 
 * Adalogger Feather M0
 * 
 * Read input voltage
 * Control Voltage Divider 
 * Analog Switches depending on ADC reading
 * 
 * Analog Switch Control  ( MAX 4618CPE+ )
 * 
 * EN(A3)   B(A2)   A(A1)    Sw On    Divider       LEDs Active Low  
 *                                    Connection    FEATHER Pins     
 * 
 * H         x        x      All off    
 * L         L        L      50V      13-12         12               
 * L         L        H      20V      13-14         11               
 * L         H        L      10V      13-15         10               
 * L         H        H       5V      13-11          9                 
 * 
 * 
 *   Ideas to try
 *     
 *     10 Meg input resistor
 *     No global variables
 *     Clean up proto board wiring
 *     Document with KiCAD
 *     Write up Lessons Learned
 *     Upload to GitHub
 */
 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # 4 (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// *** Global definitions begin

#define NumReadings       5   // Read ADC several times, throw away the first few readings 
#define FiftyOver      1023    
#define FiftyUnder      385   // unstable at 390
#define TwentyOver     1023   // was 1023
#define TwentyUnder     500   // was 500
#define TenOver        1023   // was 1023
#define TenUnder        500   // was 500
#define FiveOver       1023   // was 1023
#define FiveUnder         0

// Feather Adalogger Pins
#define AnalogInput      A0
#define AnalogSwitchA    A1
#define AnalogSwitchB    A2
#define AnalogSwitchEN   A3
#define LED_5V            9
#define LED_10V          10
#define LED_20V          11
#define LED_50V          12
#define SLC              21
#define SDA              20 

// Voltage divider input ranges
typedef enum {  
             FIFTY  =   50, 
             TWENTY =   20, 
             TEN    =   10, 
             FIVE   =    5 
             } 
             VoltageRange ;         

// Initial settings
VoltageRange VRange =   FIFTY;    // enum instance
int Reading         =   1023;     // Averaged ADC reading set to max at start
int Passes          =      0;        // Passes thru the loop

float Cal50 = 0.003005; // "volts per ADC count" for 50V scale
float Cal20 = 0.003145;
float Cal10 = 0.003080;
float Cal5  = 0.003060;

// *** Global definitions end

// *** Function Prototypes begin

int read_ADC();
void display_voltage(int VRange, int Reading);
void state_machine_run();
void debugDisplay();
void waitForSerial();
void setupVRange(int VRange);

// *** Function Prototypes end

// *** setup() begins

void setup() 
  {
  Serial.begin(9600);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
    { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
    }

  display.setRotation(2);

  // Set up Analog Switch control pins
  pinMode(AnalogSwitchA,       OUTPUT);    
  pinMode(AnalogSwitchB,       OUTPUT);    
  pinMode(AnalogSwitchEN,      OUTPUT);    
  digitalWrite(AnalogSwitchA,  LOW);  // All off = 50V range selected
  digitalWrite(AnalogSwitchB,  LOW);  
  digitalWrite(AnalogSwitchEN, LOW);  

  // Set up LED pins
  pinMode(LED_5V,  OUTPUT);
  pinMode(LED_10V, OUTPUT);
  pinMode(LED_20V, OUTPUT);
  pinMode(LED_50V, OUTPUT);

  // Turn off the LEDs
  digitalWrite(LED_5V,  HIGH);
  digitalWrite(LED_10V, HIGH);
  digitalWrite(LED_20V, HIGH);
  digitalWrite(LED_50V, HIGH);

  // while (!Serial) ;  // wait for serial port to connect up
   
  // Test LEDs
  digitalWrite(LED_5V,  LOW);
  delay(100);  
  digitalWrite(LED_5V,  HIGH);
  digitalWrite(LED_10V, LOW);
  delay(100);
  digitalWrite(LED_10V, HIGH);
  digitalWrite(LED_20V, LOW);
  delay(100);  
  digitalWrite(LED_20V, HIGH);
  digitalWrite(LED_50V, LOW);    // Only 50V LED on at startup
  }  
  
// *** setup() ends

// *** Main loop() function begins

void loop() 
  {
  state_machine_run();

delay(500);  // milliSeconds
  
  }  


// *** Main loop() function ends


// *** Function definitions begin

void state_machine_run()
  {
   display_voltage(VRange, Reading);
   Passes++;
   switch(VRange) // begin switch
     {
     case FIFTY:  
       setupVRange(VRange);
       read_ADC();  
       if (Reading >= FiftyOver) { Serial.println(" OVER-RANGE "); }
       else if (Reading <= FiftyUnder) 
         {
           VRange = TWENTY;       
         }
       else 
         {
           VRange = FIFTY;
         }
       break;
  
     case TWENTY:   
       setupVRange(VRange);
       read_ADC();         
       if(Reading >= TwentyOver)
         {
         VRange = FIFTY;           
         }
       else if(Reading <= TwentyUnder)
         {
         VRange = TEN; 
         }
       else 
         {
           VRange = TWENTY;
         }
       break;

     case TEN:        
       setupVRange(VRange);
       read_ADC(); 
       if(Reading >= TenOver)
         {
         VRange = TWENTY;                              
         }
       else if(Reading <= TenUnder)
         {
         VRange = FIVE;        
         }
       else 
         {
           VRange = TEN;
         }
       break;

       case FIVE:        
         setupVRange(VRange);
         read_ADC();           
         if(Reading >= FiveOver)
           {
           VRange = TEN;   
           }
         else 
         {
           VRange = FIVE;
         }
         break;

       default:
         {
         Serial.println("This message should never appear.");
         }
         break;      
       } // end switch
   } 

int read_ADC()
  {
  // Read ADC several times, throw away the first few readings
  // Return final ADC reading
  int i = 0; 
  //int numberOfReadings = 5;
  for ( i = 0; i < NumReadings; i++ )
    {
    Reading = analogRead(AnalogInput);
    }
  return Reading;  
  }

void display_voltage(int VRange, int Reading)
  {
  
  // Serial Monitor
  Serial.print("VRange: ");
  Serial.print(VRange);
  Serial.print(" | Reading: ");
  Serial.print(Reading);
  Serial.print(" | Calculated Volts: ");

  // Oled Display
  display.clearDisplay();
  display.setTextColor(WHITE);        // Draw white text
  display.setTextSize(1);             // Small text
  
  display.setCursor(1,1);             // Start at top-left corner
  display.print("ADC: ");
  display.print(Reading);
  
  display.setCursor(55,1);             // Start at top-left corner
  display.print(" Scale: ");
  display.print(VRange);
  display.setTextSize(2);             // Large Voltage text
  display.setCursor(20,15);           // Start at top-left corner

  
  // Calculated Volts  =  ADC Reading * scale factor       *     " volts per ADC count "
  //
  //    Cal50 "volts per ADC count" for 50V scale
  //    Cal20
  //    Cal10
  //    Cal5  
  //
  //                                    50V / 3.3V  = 15.152     3.3V / 1023 counts = 0.003226
  //                                    20V / 3.3V  =  6.061
  //                                    10V / 3.3V  =  3.030
  //                                     5V / 3.3V  =  1.515
  //  Note: " volts per ADC count " is adjusted to account for voltage divider resistor tolerances and
  //        Analog Switch losses
  //
  if (VRange == 50)
    {
    Serial.println(Reading * 15.152 * Cal50);
    display.print(Reading * 15.152 * Cal50);
    display.println(F(" V"));
    display.display();
    }
  else if(VRange == 20)
    {
    Serial.println(Reading * 6.061 * Cal20);
    display.print(Reading * 6.061 * Cal20);
    display.println(F(" V"));
    display.display();    
    }
  else if(VRange == 10)
    {
    Serial.println(Reading * 3.030 * Cal10);
    display.print(Reading * 3.030 * Cal10);
    display.println(F(" V"));
    display.display();    
    }
  else if(VRange == 5)
    {
    Serial.println(Reading * 1.515 * Cal5);
    display.print(Reading * 1.515 * Cal5);
    display.println(F(" V"));
    display.display();    
    }
  else
    {
    Serial.println("Whoops ! Program error !");
    } 
  Serial.println();  
}

void debugDisplay()
  {
    Serial.print("DEBUGGING DISPLAY  | ");
    Serial.print("ADC Reading: ");
    Serial.print(Reading);
    Serial.print(" | Vrange: ");
    Serial.print(VRange);
    Serial.println();
    Serial.println("Hit any key to continue");
    waitForSerial();
  }

void waitForSerial()
  {
  while (!Serial.available()) 
    {
    }
  Serial.println(Serial.read());
  }

void setupVRange(int VRange)
  {
    switch(VRange)
    {
      case  FIFTY:
        {
        // Leds
        digitalWrite(LED_5V, HIGH); 
        digitalWrite(LED_10V, HIGH);
        digitalWrite(LED_20V, HIGH);
        digitalWrite(LED_50V, LOW);  // Turn on 50 V LED
        // Analog Switch
        digitalWrite(AnalogSwitchA, LOW);
        digitalWrite(AnalogSwitchB, LOW);  
        digitalWrite(AnalogSwitchEN, LOW);  
        }
        break;
      case TWENTY:
        {
        // Leds
        digitalWrite(LED_5V, HIGH);
        digitalWrite(LED_10V, HIGH);
        digitalWrite(LED_20V, LOW);    // Turn on 20 V LED
        digitalWrite(LED_50V, HIGH); 
        // Analog Switch
        digitalWrite(AnalogSwitchA, HIGH);
        digitalWrite(AnalogSwitchB, LOW);
        digitalWrite(AnalogSwitchEN, LOW); 
        }
        break;
      case TEN:
        {
        // Leds
        digitalWrite(LED_5V, HIGH);
        digitalWrite(LED_10V, LOW);    // Turn on 10 V LED
        digitalWrite(LED_20V, HIGH);
        digitalWrite(LED_50V, HIGH); 
        // Analog Switch
        digitalWrite(AnalogSwitchA, LOW);
        digitalWrite(AnalogSwitchB, HIGH);
        digitalWrite(AnalogSwitchEN, LOW);
        }
        break;
      case FIVE:
        {
        // Leds
        digitalWrite(LED_5V, LOW);     // Turn on 5 V LED
        digitalWrite(LED_10V, HIGH);
        digitalWrite(LED_20V, HIGH);
        digitalWrite(LED_50V, HIGH);
        // Analog Switch
        digitalWrite(AnalogSwitchA, HIGH);
        digitalWrite(AnalogSwitchB, HIGH);
        digitalWrite(AnalogSwitchEN, LOW);
        } 
        break;
      default:
        {
        Serial.println("This message should never appear.");
          }
        break;
    }
    delay(100);  // Allow Analog Switch time to change settings
  }

// *** Function definitions end
