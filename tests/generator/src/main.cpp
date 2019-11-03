#include "Arduino.h"

#include "TaskScheduler.h"

#define OUTPUT1_PIN LED_BUILTIN  // 13
#define OUTPUT2_PIN 12

#define RETRIES 100

#define IMPULSE_MS_MIN 500
#define IMPULSE_MS_MAX 5000

#define WAIT_MS_MIN 1500
#define WAIT_MS_MAX 5000

void WrapperStart();
void ImpulseStart1();
void ImpulseStop1();
void ImpulseStart2();
void ImpulseStop2();
void TurnOn(uint8_t pin);
void TurnOff(uint8_t pin);

Scheduler ts;

uint32_t count1 = 0;
uint32_t count2 = 0;

Task tStart(0, TASK_ONCE, &WrapperStart, &ts, true);
Task tImpulseOn1(0, TASK_ONCE, &ImpulseStart1, &ts, false);
Task tImpulseOff1(0, TASK_ONCE, &ImpulseStop1, &ts, false);
Task tImpulseOn2(0, TASK_ONCE, &ImpulseStart2, &ts, false);
Task tImpulseOff2(0, TASK_ONCE, &ImpulseStop2, &ts, false);

void WrapperStart()
{	
	Serial.print(millis());
	Serial.print(" : ");
	Serial.println("Wrapper");
	tImpulseOn1.restartDelayed(WAIT_MS_MIN + random(WAIT_MS_MAX-WAIT_MS_MIN));
	tImpulseOn2.restartDelayed(WAIT_MS_MIN + random(WAIT_MS_MAX-WAIT_MS_MIN));
}

void ImpulseStart1()
{
	TurnOn(OUTPUT1_PIN);
	tImpulseOff1.restartDelayed(IMPULSE_MS_MIN + random(IMPULSE_MS_MAX-IMPULSE_MS_MIN));
}

void ImpulseStop1()
{
	TurnOff(OUTPUT1_PIN);
	if (++count1 < RETRIES)
	{
		tImpulseOn1.restartDelayed(WAIT_MS_MIN + random(WAIT_MS_MAX-WAIT_MS_MIN));
	}
}

void ImpulseStart2()
{
	TurnOn(OUTPUT2_PIN);
	tImpulseOff2.restartDelayed(IMPULSE_MS_MIN + random(IMPULSE_MS_MAX-IMPULSE_MS_MIN));
}

void ImpulseStop2()
{
	TurnOff(OUTPUT2_PIN);
	if (++count2 < RETRIES)
	{
		tImpulseOn2.restartDelayed(WAIT_MS_MIN + random(WAIT_MS_MAX-WAIT_MS_MIN));
	}
}

void TurnOn(uint8_t pin)
{
	Serial.print(millis());
	Serial.print(" : ");
	Serial.print("ON ");
	Serial.println(pin);
	digitalWrite(pin, HIGH);
}

void TurnOff(uint8_t pin)
{
	Serial.print(millis());
	Serial.print(" : ");
	Serial.print("OFF ");
	Serial.println(pin);
	digitalWrite(pin, LOW);
}

void setup() 
{
	Serial.begin(115200);

	Serial.print("Total time ~: ");
	Serial.print((IMPULSE_MS_MAX - IMPULSE_MS_MIN + WAIT_MS_MAX - WAIT_MS_MIN) / 1000.0 * RETRIES / 60.0 / 60.0);
	Serial.println(" hours");
	



	pinMode(OUTPUT1_PIN, OUTPUT);
	pinMode(OUTPUT2_PIN, OUTPUT);
	randomSeed(analogRead(0) + millis());
}

void loop() 
{
	ts.execute();
}
