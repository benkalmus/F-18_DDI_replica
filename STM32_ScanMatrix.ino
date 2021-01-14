
/*
Name:       STM32_ScanMatrix.ino
Created:  16/06/2018 13:42:34
Author:     Beniamin Kalmus - TRIARII
*/

#include <USBComposite.h>
#include <USBHID.h>

//#define USE_SERIAL

#define HOW_LONG_HELD_MS	2000

#define ENC_A1	PB14
#define ENC_A2	PB15
#define ENC_B1	PB13
#define ENC_B2	PB12

#define LEDSTRIP	PA8

#define ENC_A_STEP 2
#define ENC_B_STEP 2

#define OSD_BTNS	22

#define ROT_SWITCH_JOY_0		1+OSD_BTNS
#define ROT_SWITCH_JOY_1		2+OSD_BTNS
#define ROT_SWITCH_JOY_2		3+OSD_BTNS

#define ENC_SWITCH_JOY_A_CW		5+OSD_BTNS
#define ENC_SWITCH_JOY_A_CCW	4+OSD_BTNS
#define ENC_SWITCH_JOY_B_CW		7+OSD_BTNS
#define ENC_SWITCH_JOY_B_CCW	6+OSD_BTNS

#define ENC_A_PUSH				21
#define ENC_B_PUSH				22

//Driven outputs	//{ PA0, PA1, PA2, PA3, PA4 };       {PA1 ,PA4, PA3, PA2,PA0, }
uint16_t columnPins[] = { PA0, PA1, PA2, PA3, PA4 };
//uint16_t columnPins[] = { PA1 ,PA4, PA3, PA2,PA0, };


//Input PullDowns		 { PB11, PB10, PB0, PA7, PA6 };		 { PB10 ,PA7 ,  PB11, PB0, PA6 }
//uint16_t rowPins[] = { PA6, PA7, PB0, PB10, PB11 };
uint16_t rowPins[] = { PB11, PB10, PB0, PA7, PA6 };


int num_cols = sizeof(columnPins) / sizeof(uint16_t);
int num_rows= sizeof(rowPins) / sizeof(uint16_t);

uint16_t LED_Brightness = 65535;

int8 encA_steps = 0, encB_steps =0 ;
int16_t encoderPosA = 0, encoderPosB = 0;
uint8_t moveAmount = (uint8_t)(1024/40);
uint32_t joySend = 0, joyInterval = 10000;

int32_t encA_time = 0, encB_time = 0;
uint16_t push_Time = 300; //hold time in ms
uint8_t encA_state = 0, prevStateA = 0;
uint32_t hold_encA_time = 0;

uint8_t encB_state = 0, prevStateB = 0;

static uint8_t old_A = 0;
static uint8_t old_B = 0;

void interruptEncoderA()
{
	int8 val = read_encoder(ENC_A1, ENC_A2, & old_A);
	if (val == 1)	// if output is not 0, valid
	{

		encA_steps++;
		if (encA_steps >= ENC_A_STEP) 
		{
			encA_steps = 0;
			//only change the joystick value if the led brightness is not being changed
			encoderPosA += moveAmount;
			if (encoderPosA > 1023) encoderPosA = 1023;
#if !defined(USE_SERIAL)
			if (!encA_state) Joystick.button(ENC_SWITCH_JOY_A_CW, 1);
#endif
		}
	}
	else if (val == -1)
	{
		encA_steps--;
		if (encA_steps <= (-1 * ENC_A_STEP))
		{
			encA_steps = 0;
			//only change the joystick value if the led brightness is not being changed
			encoderPosA -= moveAmount;
			if (encoderPosA < 0) encoderPosA = 0;
#if !defined(USE_SERIAL)
			if (!encA_state) Joystick.button( ENC_SWITCH_JOY_A_CCW, 1);
#endif
		}
	}
	if (!encA_state)	Joystick.Xrotate((uint16_t)encoderPosA);
	else
	{
		LED_Brightness = encoderPosA;
		hold_encA_time = millis();
	}
}

void interruptEncoderB()
{
	int8 val = read_encoder(ENC_B1, ENC_B2, & old_B);
	if (val == 1)	// if output is not 0, valid
	{

		encB_steps++;
		if (encB_steps >= ENC_B_STEP)
		{
			encB_steps = 0;
			encoderPosB += moveAmount;
			if (encoderPosB > 1023) encoderPosB = 1023;
#if !defined(USE_SERIAL)
			Joystick.button(ENC_SWITCH_JOY_B_CW, 1);
#endif
		}
	}
	else if (val == -1)
	{
		encB_steps--;
		if (encB_steps <= (-1 * ENC_B_STEP))
		{
			encB_steps = 0;
			encoderPosB -= moveAmount;
			if (encoderPosB < 0) encoderPosB = 0;
#if !defined(USE_SERIAL)
			Joystick.button(ENC_SWITCH_JOY_B_CCW, 1);
#endif
		}
	}
	Joystick.Yrotate((uint16_t)encoderPosB);

}

int8_t read_encoder(uint8 pin1, uint8 pin0, uint8_t *prev_state)
{
	static int8_t enc_states[] = { 0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0 };
	*prev_state <<= 2;	//remember previous state
	//							possible states: 0b00, 0b01, 0b10, 0b11
	uint8_t encoder_combined = digitalRead(pin1) << 1 | (digitalRead(pin0));
	*prev_state |= (encoder_combined & 0b00000011);		//adds current states of pins a1, a2 to the least signif. bits, and leaving the rest 0 so that it can be ORed with ol_AB
	
	return (enc_states[(*prev_state & 0x0f)]);
}


// The setup() function runs once each time the micro-controller starts
void setup()
{
#if !defined(USE_SERIAL)
	USBHID.begin(HID_JOYSTICK,41,42,"Triarii Electronics", "Hornet DDI");
#endif
	pinMode(33, OUTPUT);	//default inbuilt led
	digitalWrite(33, 0);	// set to low
	for (int i = 0; i < num_cols; i++)
	{
		pinMode(columnPins[i], OUTPUT); //using push pull resistors
	}
	for (int i = 0; i < num_rows; i++)
	{
		pinMode(rowPins[i], INPUT_PULLDOWN);  //rows need to be pulled down to read a digital 0 when off.
	}

	pinMode(LEDSTRIP, PWM);
	//digitalWrite(LEDSTRIP, 1);
	//encoder
	pinMode(ENC_A1, INPUT_PULLUP);
	pinMode(ENC_A2, INPUT_PULLUP);
	pinMode(ENC_B1, INPUT_PULLUP);
	pinMode(ENC_B2, INPUT_PULLUP);
	attachInterrupt(ENC_A1, interruptEncoderA, CHANGE);
	attachInterrupt(ENC_A2, interruptEncoderA, CHANGE);
	attachInterrupt(ENC_B1, interruptEncoderB, CHANGE);
	attachInterrupt(ENC_B2, interruptEncoderB, CHANGE);

#if defined (USE_SERIAL)
	Serial.begin(115200);
	while (!Serial);    //wait for seria
	Serial.println("Connected");

	Serial.print("Rows ");
	Serial.println(num_rows);
	Serial.print("Cols ");
	Serial.println(num_cols);
#endif
#if !defined(USE_SERIAL)
	Joystick.setManualReportMode(true); // aggregate data before sending
	delay(1000);
	//reset all axes
	Joystick.Xrotate(0);
	Joystick.Yrotate(0);
	for (int i = 0; i < 32; i++)
	{
		Joystick.button(i, 0);
	}
	Joystick.send();
#endif
}

// Add the main program code into the continuous loop() function
void loop()
{
	delay(5); //crude debounce

	uint32 //ledFadeValue = map(encoderPosB, 0, 1023, 0, 65535);		//linear mapping - bad
	ledFadeValue = (uint32_t)pow(2.718, (float)LED_Brightness / 92.11f);		//using e to get a better response curve.
	//sigmoid
	//ledFadeValue =( 65535 / (1 + (pow(2.718, -(encoderPosA-512)/80))));

	if (ledFadeValue > 65535) ledFadeValue = 65535;

	if (encA_state == 1)	pwmWrite(LEDSTRIP, ledFadeValue);

	if (millis() - hold_encA_time >= HOW_LONG_HELD_MS)
	{
		//hold_encA_time = millis();
		//stops changing the led brightness
		encA_state = 0;	//reset state back to 0, in case user forgets to push the encoder btn down again.
	}
	
	
	digitalWrite(columnPins[4], 1);		//set last column to 1
	for (int j = 0; j < num_rows; j++)
	{
#if !defined(USE_SERIAL)
		if (digitalRead(rowPins[0]))
		{
			Joystick.button( ROT_SWITCH_JOY_1, 1);
			Joystick.button( ROT_SWITCH_JOY_0, 0);
			Joystick.button( ROT_SWITCH_JOY_2, 0);
		}
		else if (digitalRead(rowPins[1]))
		{
			Joystick.button( ROT_SWITCH_JOY_2, 1);
			Joystick.button( ROT_SWITCH_JOY_0, 0);
			Joystick.button( ROT_SWITCH_JOY_1, 0);
		}
		else
		{
			Joystick.button( ROT_SWITCH_JOY_0, 1);
			Joystick.button(ROT_SWITCH_JOY_1, 0);
			Joystick.button(ROT_SWITCH_JOY_2, 0);
		}
#endif
		
	
		if (!digitalRead(rowPins[3]))
		{
			prevStateA = 0;
			Joystick.button(ENC_A_PUSH, 0);
		}
		else	Joystick.button(ENC_A_PUSH, 1 );

		if (!digitalRead(rowPins[4]))
		{
			prevStateB = 0;
			Joystick.button(ENC_B_PUSH, 0);
		}
		else	Joystick.button(ENC_B_PUSH, 1);

		//check if encoder push down buttons are pressed or held
		if (digitalRead(rowPins[3]) && prevStateA == 0)
		{
			encA_time = millis();			//note the time pressed
			prevStateA = 1;
		}
		else if (digitalRead(rowPins[3]) && prevStateA == 1 && ((int32_t)millis() - encA_time > push_Time))
		{
			encA_state ^= 1;
			if (encA_state == 1) 
			{
				hold_encA_time = millis();
			}
			encA_time = millis() + 100000;		//prevents the if statement from being executed repeatedly.
		}
	}
	digitalWrite(columnPins[4], 0);
	delay(5);

	//SCAN MATRIX for the OSD BUTTONS
	for (int i = 0; i < 4; i++)
	{
		digitalWrite(columnPins[i], 1);
		for (int j = 0; j < num_rows; j++)
		{
			//check if current pin is HIGH
			if (digitalRead(rowPins[j]))
			{
				//buttons start at 1...
				//to get button id: column number *(rows connected per column) + row number + 1
#if !defined(USE_SERIAL)
				Joystick.button((i*5) + j+1, 1);
#endif
			}
			else
			{
#if !defined(USE_SERIAL)
				Joystick.button((i * 5) + j + 1, 0);
#endif
			}
		}
		//switch the pin off for the next loop
		digitalWrite(columnPins[i], 0);
		delay(1);		//1ms delay to wait for the pin to be brought down fully
	}

#if !defined(USE_SERIAL)
	if (micros() - joySend > joyInterval)	//sends data at specific intervals
	{
		joySend = micros();
		Joystick.send();		//sends data buffer
		Joystick.button(ENC_SWITCH_JOY_A_CW, 0);	//switch off rotary encoder buttons
		Joystick.button( ENC_SWITCH_JOY_A_CCW, 0);
		Joystick.button( ENC_SWITCH_JOY_B_CW, 0);
		Joystick.button( ENC_SWITCH_JOY_B_CCW, 0);
	}
#endif
}

