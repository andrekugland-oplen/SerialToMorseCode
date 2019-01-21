/*
   Serial to Morse Code

   Converts the input received from the serial interface to Morse code,
   driving a pin with the resulting code.

   It uses two circular buffers, one for the characters read from the serial
   interface and another one for the timed signals (millis, HIGH/LOW) to drive
   the pin.

   modified 20 Jan 2019
   by Andre Kugland

   This example code is in the public domain.
*/


#include <CircularBuffer.h>


/* You can change the following two definitions to your liking. */

#define OUTPUT_PIN          LED_BUILTIN   /* Output pin */
#define MORSE_TIME_UNIT     150           /* Base time unit of the output code */



/* Table containing the ASCII code of the character and a descriptor byte with size
   and binary-encoded Morse code (size occupies the 3 most-significant bits, while
   the code occupies the least-significant 5 bits). */

/* Some pre-processor macros to help create the aforementioned table. */
#define DOT  1
#define DASH 0
#define PACK_MORSE_N(ch, sz, mask)        { ch, (sz<<5) | (mask) }
#define PACK_MORSE_1(ch, a)               PACK_MORSE_N(ch, 1, a)
#define PACK_MORSE_2(ch, a, b)            PACK_MORSE_N(ch, 2, a|(b<<1))
#define PACK_MORSE_3(ch, a, b, c)         PACK_MORSE_N(ch, 3, a|(b<<1)|(c<<2))
#define PACK_MORSE_4(ch, a, b, c, d)      PACK_MORSE_N(ch, 4, a|(b<<1)|(c<<2)|(d<<3))
#define PACK_MORSE_5(ch, a, b, c, d, e)   PACK_MORSE_N(ch, 5, a|(b<<1)|(c<<2)|(d<<3)|(e<<4))



/* A nice 16-bit struct for the table. */
typedef struct {
  char idx_ch;                 /* Character */
  byte data;                   /* Corresponding Morse code */
} morse_code_t;


/* A struct to hold timed digital signals (HIGHs and LOWs). */
typedef struct {
  unsigned long startMillis;   /* Send signal when millis() >= starMillis */
  byte value;                  /* LOW or HIGH */
} signal_t;



/* The actual table, marked as PROGMEM to spare precious RAM. */
const morse_code_t morse_code_tbl[36] PROGMEM = {
  PACK_MORSE_1('E', DOT),
  PACK_MORSE_1('T', DASH),
  PACK_MORSE_2('A', DOT,  DASH),
  PACK_MORSE_2('I', DOT,  DOT),
  PACK_MORSE_2('M', DASH, DASH),
  PACK_MORSE_2('N', DASH, DOT),
  PACK_MORSE_3('D', DASH, DOT,  DOT),
  PACK_MORSE_3('G', DASH, DASH, DOT),
  PACK_MORSE_3('K', DASH, DOT,  DASH),
  PACK_MORSE_3('O', DASH, DASH, DASH),
  PACK_MORSE_3('R', DOT,  DASH, DOT),
  PACK_MORSE_3('S', DOT,  DOT,  DOT),
  PACK_MORSE_3('U', DOT,  DOT,  DASH),
  PACK_MORSE_3('W', DOT,  DASH, DASH),
  PACK_MORSE_4('B', DASH, DOT,  DOT,  DOT),
  PACK_MORSE_4('C', DASH, DOT,  DASH, DOT),
  PACK_MORSE_4('F', DOT,  DOT,  DASH, DOT),
  PACK_MORSE_4('H', DOT,  DOT,  DOT,  DOT),
  PACK_MORSE_4('J', DOT,  DASH, DASH, DASH),
  PACK_MORSE_4('L', DOT,  DASH, DOT,  DOT),
  PACK_MORSE_4('P', DOT,  DASH, DASH, DOT),
  PACK_MORSE_4('Q', DASH, DASH, DOT,  DASH),
  PACK_MORSE_4('V', DOT,  DOT,  DOT,  DASH),
  PACK_MORSE_4('X', DASH, DOT,  DOT,  DASH),
  PACK_MORSE_4('Y', DASH, DOT,  DASH, DASH),
  PACK_MORSE_4('Z', DASH, DASH, DOT,  DOT),
  PACK_MORSE_5('0', DASH, DASH, DASH, DASH, DASH),
  PACK_MORSE_5('1', DOT,  DASH, DASH, DASH, DASH),
  PACK_MORSE_5('2', DOT,  DOT,  DASH, DASH, DASH),
  PACK_MORSE_5('3', DOT,  DOT,  DOT,  DASH, DASH),
  PACK_MORSE_5('4', DOT,  DOT,  DOT,  DOT,  DASH),
  PACK_MORSE_5('5', DOT,  DOT,  DOT,  DOT,  DOT),
  PACK_MORSE_5('6', DASH, DOT,  DOT,  DOT,  DOT),
  PACK_MORSE_5('7', DASH, DASH, DOT,  DOT,  DOT),
  PACK_MORSE_5('8', DASH, DASH, DASH, DOT,  DOT),
  PACK_MORSE_5('9', DASH, DASH, DASH, DASH, DOT)
};



/* A circular buffer to store characters. */
CircularBuffer<char, 1536> charBuffer;

/* Circular buffer used to store signals. 11 is the max number
   of signals within a character. */
CircularBuffer<signal_t, 11> signalBuffer;



/*
   Reads bytes from serial and enqueue them in the character buffer.
*/
void enqueue_char_from_serial()
{
  if (!charBuffer.isFull() && Serial.available())
    charBuffer.push(Serial.read());

  /* If, however, our buffer is full, we don't have to worry, since
     the UART has 64 bytes of buffer, and after this buffer is full,
     the UART will signal the client it is not ready. The client,
     hopefully, is not an Arduino, and will have plenty of space
     to hold the characters. */
}



/*
   Enqueues the first character in the char buffer as signals
   timed according to the Morse code table.
*/
void enqueue_signals_from_char()
{
  /* If there are still signals in the signal buffer, we must wait.
     If there are no chars in the char buffer, we have nothing to do. */
  if (!signalBuffer.isEmpty() || charBuffer.isEmpty()) {
    return;
  }

  char ch = charBuffer.shift(); /* Get the next char. */

  /* Convert lowercase to uppercase. */
  if (ch >= 'a' && ch <= 'z')
    ch -= 32;

  if (ch == ' ' || ch == '\t' || ch == '\n') {
    /* Seven time units of silence between words. But why only
       four here? There are already one after each dot or dash,
       and two more after each letter. */
    signalBuffer.push(signal_t{millis() + 4 * MORSE_TIME_UNIT, LOW});
    return;
  }

  for (int i = 0; i < 36; i++) {
    /* Load both idx and data from program memory. */
    char idx_ch = pgm_read_byte(&(morse_code_tbl[i].idx_ch));

    if (ch == idx_ch) {
      unsigned long startMillis = millis();
      byte          data = pgm_read_byte(&(morse_code_tbl[i].data));
      unsigned      size = (data >> 5) & 7;    /* 3 bits for size. */
      data &= 31;                              /* 5 bits for dots and dashes. */

      for (int j = 0; j < size; j++) {
        /* First enqueue a high signal. */
        signalBuffer.push(signal_t{startMillis, HIGH});
        /* One time unit for dots, three for dashes. */
        startMillis += (((data & 1) == DOT) ? 1 : 3) * MORSE_TIME_UNIT;
        /* Then enqueue a low signal. */
        signalBuffer.push(signal_t{startMillis, LOW});
        /* One time unit of silence after each dot or dash.
           This silence need not be enqueued, we must only
           keep track of its timing, for the next signal. */
        startMillis += MORSE_TIME_UNIT;
        data >>= 1;
      }
      /* Three time units of silence after letter. (The third
         is the one after the dot or dash.) Now this must be
         enqueued, since we will need this timing in the tail
         of the queue when the next character comes. */
      signalBuffer.push(signal_t{startMillis + 2 * MORSE_TIME_UNIT, LOW});
      break;
    }
  }
}



/*
   Checks if there is a signal already due to be written to the pin,
   and, if there is one, write it and remove from the queue.
*/
void drive_pin_from_signal()
{
  if (!signalBuffer.isEmpty()) {
    if (signalBuffer.first().startMillis <= millis()) {
      digitalWrite(OUTPUT_PIN, signalBuffer.first().value);
      signalBuffer.shift();
    }
  }
}



/*
  Finally, the setup() ...
*/
void setup()
{
  Serial.begin(115200);         /* Initializes serial interface. */
  while (!Serial);              /* Needed for USB serial. */
  pinMode(OUTPUT_PIN, OUTPUT);  /* Output pin is for output. */

  /* A welcoming message. */
  Serial.print(F("Serial to Morse code running on pin "));
  Serial.println(OUTPUT_PIN);
  Serial.println(F("Ready."));
}



/*
  ... and loop() functions.
*/
void loop()
{
  enqueue_char_from_serial();
  enqueue_signals_from_char();
  drive_pin_from_signal();
}
