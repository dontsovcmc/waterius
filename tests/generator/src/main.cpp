#include "Arduino.h"

#include "TaskScheduler.h"

#define OUTPUT1_PIN LED_BUILTIN
//#define OUTPUT2_PIN 2

#define RETRIES 5

#define IMPULSE_MS_MIN 100
#define IMPULSE_MS_MAX 500

#define WAIT_MS_MIN 999
#define WAIT_MS_MAX 1001

void WrapperStart();
void ImpulseStart();
void ImpulseStop();

Scheduler ts;

Task tStart(0, TASK_ONCE, &WrapperStart, &ts, true);
Task tImpulseOn(0, TASK_ONCE, &ImpulseStart, &ts, false);
Task tImpulseOff(0, TASK_ONCE, &ImpulseStop, &ts, false);

void WrapperStart()
{	
	Serial.print(millis());
	Serial.print(" : ");
	Serial.println("Wrapper");
	tImpulseOn.restartDelayed(WAIT_MS_MIN + random(WAIT_MS_MAX-WAIT_MS_MIN));
}

void ImpulseStart()
{
	Serial.print(millis());
	Serial.print(" : ");
	Serial.println("ImpulseStart");
	digitalWrite(OUTPUT1_PIN, HIGH);
	tImpulseOff.restartDelayed(IMPULSE_MS_MIN + random(IMPULSE_MS_MAX-IMPULSE_MS_MIN));
}

void ImpulseStop()
{
	Serial.print(millis());
	Serial.print(" : ");
	Serial.println("ImpulseStop");
	digitalWrite(OUTPUT1_PIN, LOW);
	tImpulseOn.restartDelayed(WAIT_MS_MIN + random(WAIT_MS_MAX-WAIT_MS_MIN));
}

void setup() 
{
	Serial.begin(115200);
	pinMode(OUTPUT1_PIN, OUTPUT);
	randomSeed(analogRead(0) + millis());
}

void loop() 
{
	ts.execute();
}
