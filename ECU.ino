#include <PID_v1.h>

#define TOOTH_OFFSET 240

const int missingTooth = 2;      // the pin that the pushbutton is attached to
unsigned long lastTooth = 0;          // Point in time that the last tooth occured
unsigned toothTime = 32000;          // The time between teeth
unsigned crank_angle;            // The current crank angle
unsigned RPM = 0;                // The current engine RPM
long testTimer = 0;

// Throttle
const int throttlePin = 0;       // the pin that the throttle position sensor is on

// Lambda
const int lambdaPin = 1;         // the pin that the lambda sensor reading is on
double lambda = 512;              // The value read from the lambda analog input pin 512 should be lambda = 1
char lambdaDeadband = 20;        // The deadback for lambda feedback, don't adjust the output within this region from lambda = 512
double lambdaSetpoint = 512;
String fuelControl = "PID";  // Whether to use lambda feedback or the engine map

// Fuel
const int fuelPin = 12;          // the pin that the LED is attached to
unsigned fuelTime = calcTime(1000,64);// The fuel pulse timing delay, default value at this timer's prescaler
double fuelDuration = calcTime(4000,64); // The fuel pulse duration, default value at this timer's prescaler

// Ignition
const int ignPin = 13;           // the pin that the ignition is attached to
char ignAngle = 18; // The ignition delay time @TODO modify to be crank angle based
unsigned ignDuration = calcTime(1500,64); // The time that the ignition coil charges for

// For PID
PID lambdaPID(&lambda,&fuelDuration,&lambdaSetpoint,1,1,0.1,DIRECT);

void setup() {
  // initialize the crank sensor as a input:
  pinMode(missingTooth, INPUT);
  // initialize the fuel as an output:
  pinMode(fuelPin, OUTPUT);
  
  // Turn the fuel off!
  digitalWrite(fuelPin,LOW);
  
  analogReference(EXTERNAL);
  //pinMode(throttlePin, INPUT);
  
  // initialize serial communication:
  Serial.begin(115200);
  
  attachInterrupt(0, missingToothISR, CHANGE);
  
  // PID
  lambdaPID.SetMode(AUTOMATIC);
  lambdaPID.SetOutputLimits(1, 65535);
  
   // initialize timer1 
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;               // Control register for waveform generation
  TCCR1B = 0;               // Turn off noise cancelling, turn off edge select, waveform gen mode 0, no clock source
  TCCR1B |= (1 << ICNC1);   // Turn on noise cancelling (samples 4 times before considered a change)
  TCNT1  = 0;               // Reset timer counter to 1
  
  OCR1A = fuelTime; // compare match register 16MHz/256/2Hz
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS11);    // Load prescaler
  TCCR1B |= (1 << CS10);    // Load prescaler
  
  // initialise timer2
  TCCR3A = 0;               // Control register for waveform generation (off
  TCCR3B = 0;               // Turn off noise cancelling, turn off edge select, waveform gen mode 0, no clock source
  TCNT3  = 0;               // Reset timer counter to 1
  
  //OCR3A = ignTime;  // Load the compare match register
  TCCR3B |= (1 << WGM32);   // CTC mode
  TCCR3B |= (1 << CS31);    // Load prescaler
  TCCR3B |= (1 << CS30);    // Load prescaler
  //TIMSK3 |= (1 << OCIE3A);  // enable timer compare interrupt
  
  interrupts();             // enable all interrupts
}

void loop() {
  //delay(50);
  
  //lambda = temp2*temp2;
  
  //lambda = (float)0.9*lambda + (float)analogRead(lambdaPin)*0.1; // low pass filter
  lambda = analogRead(lambdaPin);  // no filter
  //lambda = map(analogRead(lambdaPin),0,865,0,1023);    // Remap for max vals
  if(fuelControl == "ONOFF" && millis()%50 >45){
    if(lambda > 512 + lambdaDeadband && fuelDuration < 65534){
       fuelDuration += 1; 
    }else if(lambda < 512 - lambdaDeadband && fuelDuration > 1){
       fuelDuration -= 1; 
    }
  }
  
  
  // Use the map if lambda is turned off
  if(fuelControl == "PID"){
      // Look in the map to see if the data point is available
      // If it is, then load it
      // If it's not, then interpolate
      lambdaPID.Compute();
  }
  
  
  if(millis()%500 >495){
    RPM = 500000/toothTime;
    Serial.print(RPM);
    Serial.print("\t");
    Serial.print(fuelDuration);
    Serial.print("\t\t");
    Serial.print(fuelTime);
    Serial.print("\t\t");
    Serial.print(ignDuration);
    Serial.print("\t\t");
    Serial.print(ignAngle);
    Serial.print("\t");
    Serial.print(lambda);
    Serial.print("\t"); 
    Serial.print(lambdaSetpoint);
    Serial.print("\t");
    Serial.println(analogRead(throttlePin)/10.23);
    if(millis()%5000 >4995){
      Serial.println(fuelControl);
      Serial.print("RPM");
      Serial.print("\t");
      Serial.print("fuelDuration");
      Serial.print("\t");
      Serial.print("fuelTime");
      Serial.print("\t");
      Serial.print("ignDuration");
      Serial.print("\t");
      Serial.print("ignTime");
      Serial.print("\t");
      Serial.print("lambda");
      Serial.print("\t");
      Serial.print("lambdaSetpoint");
      Serial.print("\t");
      Serial.println("throttle");
    }
  }
  
}

void serialEvent() {
  char temp = Serial.read();
  switch(temp){
    case 'f':
      fuelDuration = calcTime(Serial.parseInt(),64);
    break;
    case 'd':
      fuelTime = calcTime(Serial.parseInt(),64);
    break;
    case 'i':
    Serial.println(ignDuration);
      ignDuration = calcTime(Serial.parseInt(),64);
          Serial.println(ignDuration);
    break;
    case 't':
      ignAngle = Serial.parseInt();
    break;
    case 'l':
      char next = Serial.read();
      if(next == 'd'){
        lambdaDeadband = Serial.parseInt();
        fuelControl = "ONOFF";
      }else if(next == 's'){
        fuelControl = "PID";
        lambdaSetpoint = Serial.parseInt();
      }else{
        // Turn lambda feedback off, and use the map instead
        fuelControl = "manual"; 
      }
    break;
  }
}

void missingToothISR(){
  long time = micros();
  long temp = time - lastTooth;
  

  if(temp > (5*toothTime) || crank_angle > 360){
    // Missing tooth detected
    
    // Sort out fuel timer
    TIFR1 |= 1 << OCF1A;        // Write a 1 to the interrupt flag to clear it
    TCNT1 = 0;                  // Reset the timer count to 0
    TIMSK1 |= (1 << OCIE1A);    // enable timer compare interrupt
    OCR1A = fuelTime;           // Load the compare match register
    
    // Start the ignition delay timer
    TIFR3 |= 1 << OCF3A;        // Write a 1 to the interrupt flag to clear it
    TCNT3 = 0;                  // Reset the timer count to 0
    TIMSK3 |= (1 << OCIE3A);    // enable timer compare interrupt
    
    //OCR3A = ignTime;            // Load the compare match register
    
    // Work out the timings for the fuel and ignition events based on the length of time the last tooth took
    // 1 tooth is 3 degrees, so toothTime = 3 degrees worth of microseconds
    // If the ignition pulse should finish 18 degrees before TDC then it has to start 18 degrees + duration of charge before TDC
    // Convert 18 degrees into microseconds: (18/3)*toothTime
    // Plus the time for charging: (18/3)*toothTime + ignDuration  = A
    // Time after this missing tooth is TOOTH_OFFSET in microseconds minus A
    // (TOOTH_OFFSET/3) * toothTime - ((18/3)*toothTime +ignDuration)
    // (TOOTH_OFFSET/3) * toothTime -(18/3)*toothTime -ignDuration
    // (TOOTH_OFFSET-18)*toothTime/3 - ignDuration
    // This method requires careful prescaler choice since the timer is fully utilised at the RPM limits 64 seems to work ok
    // The calculations also need to be cast to floats to avoid strange results
    OCR3A = calcTime((float)(TOOTH_OFFSET - ignAngle)*toothTime/3.0 - (ignDuration << 2) ,64);            // Load the compare match register
    //OCR3A = ignTime;            // Load the compare match register
    
    //Serial.print(time);Serial.print("\t");
    //Serial.print(lastTooth);Serial.print("\t");
    //Serial.println(temp);
    
    crank_angle = 0;            // Reset the crank angle
    temp = temp/6.0;              // This is how long a tooth would have taken and allows for correct calculation of engine RPM

  }else{    
    crank_angle+=3;
  }
  lastTooth = time;
   toothTime = temp; 
}

// Deal with turning the fuel on and off
ISR(TIMER1_COMPA_vect)          // timer compare interrupt service routine
{
   if(digitalRead(fuelPin) == LOW){
     // If fuel is off, then turn it on and set timer to turn it off
     digitalWrite(fuelPin,HIGH);
     TIFR1 |= 1 << OCF1A;        // Write a 1 to the interrupt flag to clear it
     TCNT1 = 0;                  // Reset the timer count to 0
     OCR1A = (int)fuelDuration;        // Load the compare match register with the coil charge time
     
   }else{
     // If fuel was on, then turn it off
     digitalWrite(fuelPin,LOW);
     TIMSK1 &= ~(1 << OCIE1A);  // disable timer compare interrupt
   }
}

// Deal with turning the spark on and off
ISR(TIMER3_COMPA_vect)          // timer compare interrupt service routine
{
   
   if(digitalRead(ignPin) == LOW){
     // If ignition off, then turn it on and set timer to turn it off again 
     digitalWrite(ignPin,HIGH);
     TIFR3 |= 1 << OCF3A;        // Write a 1 to the interrupt flag to clear it
     TCNT3 = 0;                  // Reset the timer count to 0
     //Serial.println(ignDuration);
     OCR3A = ignDuration;        // Load the compare match register with the coil charge time
     testTimer = micros();
   }else{
     //Serial.println(micros()- testTimer);
     
     // If ignition on, turn it off and disable timer until next missing tooth
     digitalWrite(ignPin,LOW);
     TIMSK3 &= ~(1 << OCIE3A);  // disable timer compare interrupt 
   }
}

// Do a short pulse on the given pin
// Can be used to show events as a pin output
void outputMarker(unsigned pin){
   digitalWrite(pin,LOW);
   delayMicroseconds(10);
   digitalWrite(pin,HIGH);
   delayMicroseconds(100);
   digitalWrite(pin,LOW); 
   delayMicroseconds(10);
}

//  Calculate the number of timer counts required for the given time with the given prescaler
//  Time is in nanoseconds.
//  Returns the integer value to load into the timer counter register
unsigned calcTime(unsigned long time, unsigned prescaler){
  return (int)(time/(prescaler >> 4));
}
