#include "Arduino.h"

#include "TaskScheduler.h"

// 
// Генератор импульсов для тестирования счетчика импульсов
// 
// Настройка:
// диапазон длины импульса 
// диапазон длины паузы 
// 2 выхода
// 
// В случайном порядке импульсы будут поданы на выходы
// 

#define OUTPUT1_PIN 11 
#define OUTPUT2_PIN 12


#define RETRIES 10000

#define IMPULSE_MS_MIN 250+25
#define IMPULSE_MS_MAX 3000

#define WAIT_MS_MIN 750+75
#define WAIT_MS_MAX 3000
#define WAIT_BAD 120  //паузу меньше будем считать не импульсом (так считает ватериус)

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
float    total_hours = 0.0;

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
	digitalWrite(LED_BUILTIN, LOW);
}

void ImpulseStop1()
{
	TurnOff(OUTPUT1_PIN);

	digitalWrite(LED_BUILTIN, HIGH);
	if (random(1000) % 2)
	{
		if (count1 < RETRIES)
		{
			tImpulseOn1.restartDelayed(random(WAIT_BAD));
		}
	}
	else
	{
		if (++count1 < RETRIES)
		{
			tImpulseOn1.restartDelayed(WAIT_MS_MIN + random(WAIT_MS_MAX-WAIT_MS_MIN));
		}
	}
}

void ImpulseStart2()
{
	TurnOn(OUTPUT2_PIN);
	tImpulseOff2.restartDelayed(IMPULSE_MS_MIN + random(IMPULSE_MS_MAX-IMPULSE_MS_MIN));
	digitalWrite(LED_BUILTIN, LOW);
}

void ImpulseStop2()
{
	TurnOff(OUTPUT2_PIN);
	if (++count2 < RETRIES)
	{
		digitalWrite(LED_BUILTIN, HIGH);
		tImpulseOn2.restartDelayed(WAIT_MS_MIN + random(WAIT_MS_MAX-WAIT_MS_MIN));
	}
}

void TurnOn(uint8_t pin)
{
	Serial.print(millis());
	Serial.print(" : ");
	Serial.print("ON ");
	Serial.println(pin);

	pinMode(pin, OUTPUT);
	digitalWrite(pin, LOW);
}

void TurnOff(uint8_t pin)
{	
	Serial.print(millis());
	Serial.print(" : ");
	Serial.print("OFF ");
	Serial.println(pin);

	digitalWrite(pin, HIGH);
	pinMode(pin, INPUT);
}

void setup() 
{
	Serial.begin(115200);
	
	total_hours = (IMPULSE_MS_MAX - IMPULSE_MS_MIN + WAIT_MS_MAX - WAIT_MS_MIN) / 2.0 / 1000.0 * RETRIES / 60.0 / 60.0;
	Serial.print("Total time ~: ");
	if (total_hours >= 1.0) 
	{
		Serial.print(total_hours);
		Serial.println(" hours");
	}
	else
	{
		Serial.print(total_hours * 60.0);
		Serial.println(" minutes");
	}
	
	pinMode(OUTPUT1_PIN, INPUT);
	pinMode(OUTPUT2_PIN, INPUT);
	randomSeed(analogRead(0) + millis());
}

void loop() 
{
	pinMode(LED_BUILTIN, OUTPUT);
	ts.execute();
}
