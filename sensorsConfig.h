const int maxHeat = 25; //set hot parameter
const int minHeat = 23; //set cold parameter
const int minMoist = 500; //set moisture threshold

int calibrationTime = 20;
long unsigned int lowIn;         

//the amount of milliseconds the sensor has to be low 
//before we assume all motion has stopped
long unsigned int pause = 1000;  

boolean lockLow = true;
boolean takeLowTime;  

int pirPin = A0;    //the digital pin connected to the PIR sensor's output
int pir = 0;  // initial pir sensor set to off

// Variables to store gyroscope data along the x, y, and z axes
float gX, gY, gZ;

// PIR sensor setup
void PIRSensorSetup(){
  // Serial.begin(9600);
  // pir sensor
  pinMode(pirPin, INPUT);
  digitalWrite(pirPin, LOW);

  Serial.print("calibrating pir sensor ");
    for(int i = 0; i < calibrationTime; i++){
      Serial.print(".");
      delay(1000);
      }
    Serial.println(" done");
    Serial.println("MOTION SENSOR ACTIVE");
    delay(50);
}
  
////////////////////////////
//PIR sensor LOOP
void pirSensorLoop(){

     if(digitalRead(pirPin) == HIGH){
       if(lockLow){  
         //makes sure we wait for a transition to LOW before any further output is made:
         lockLow = false;            
         Serial.println("---");
         Serial.print("motion detected at ");
         int pir = millis()/1000;
         Serial.print(pir);
         Serial.println(" sec"); 
         delay(50);
         }         
         takeLowTime = true;
       }

     if(digitalRead(pirPin) == LOW){    
       if(takeLowTime){
        lowIn = millis();          //save the time of the transition from high to LOW
        takeLowTime = false;       //make sure this is only done at the start of a LOW phase
        }
       //if the sensor is low for more than the given pause, 
       //we assume that no more motion is going to happen
       if(!lockLow && millis() - lowIn > pause){  
           //makes sure this block of code is only executed again after 
           //a new motion sequence has been detected
           lockLow = true;                        
           Serial.print("motion ended at ");      //output
           Serial.print((millis() - pause)/1000);
           Serial.println(" sec");
           delay(50);
           }
       }
}

// thermostat control conditions
void heaterController(float tempValue) {
  if (tempValue > maxHeat){
    digitalWrite(2, LOW);
    Serial.println("Heater Switched Off");
   }
  else {
    digitalWrite(2, HIGH);
    Serial.println("Heater Switched On");
  }
}

// set current moisture value to text
void getMoistureText(int moistureValue) {
  if (moistureValue > minMoist){
    Serial.println("The kennel is DRY: ");
  }
  else{
    Serial.println("The kennel is WET: ");
  }
  Serial.println(moistureValue);
}

// get gyroscope value in percentage
void getShakeSensorValue(float gyroScope) {

    gyroScope;
    // Read the gyroscope data along the x, y, and z axes
    // Calculate the magnitude of the gyroscope vector (motion intensity)
    float gyroMagnitude = sqrt(gX * gX + gY * gY + gZ * gZ);
    // Calculate the percentage of motion relative to the maximum possible motion range (e.g., 100 deg/s for gyroscope)
    int shakePercentage = map(gyroMagnitude, 0, 100, 0, 100);
    // Print the shake value percentage to the serial monitor
    Serial.print("Shake Percentage: ");
    Serial.print(shakePercentage);
    Serial.println("%");
}


