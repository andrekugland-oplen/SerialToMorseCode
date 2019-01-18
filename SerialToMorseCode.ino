/*
   Serial to Morse Code

   Converts the input received from the serial interface to Morse code,
   driving a pin with the resulting code.

   modified 15 Jan 2019
   by Andre Kugland

   This example code is in the public domain.
*/

#include <CircularBuffer.h>


/* You can change the following two definitions to your liking. */

#define OUTPUT_PIN LED_BUILTIN            /* Output pin */
#define MORSE_TIME_UNIT 200               /* Base time unit of the output code */


/* Table containing the ASCII code of the character and a descriptor byte with size
   and binary-encoded Morse code (size occupies the 3 most-significant bits, while
   the code occupies the least-significant 5 bits). */

/* Some pre-processor macros to help create the aforementioned table. */
#define DOT  1
#define DASH 0
#define __ENCODE_MORSE(ch, sz, mask)      { ch, (sz<<5) | (mask) }
#define ENCODE_MORSE1(ch, a)              __ENCODE_MORSE(ch, 1, a)
#define ENCODE_MORSE2(ch, a, b)           __ENCODE_MORSE(ch, 2, a|(b<<1))
#define ENCODE_MORSE3(ch, a, b, c)        __ENCODE_MORSE(ch, 3, a|(b<<1)|(c<<2))
#define ENCODE_MORSE4(ch, a, b, c, d)     __ENCODE_MORSE(ch, 4, a|(b<<1)|(c<<2)|(d<<3))
#define ENCODE_MORSE5(ch, a, b, c, d, e)  __ENCODE_MORSE(ch, 5, a|(b<<1)|(c<<2)|(d<<3)|(e<<4))

/* A nice 16-bit struct for the table. */
typedef struct {
  char ch;
  byte data;
} morse_code_t;

/* The actual table, marked as PROGMEM to spare precious RAM. */
const morse_code_t morse_code_tbl[36] PROGMEM = {
  ENCODE_MORSE1('E', DOT),
  ENCODE_MORSE1('T', DASH),
  ENCODE_MORSE2('A', DOT, DASH),
  ENCODE_MORSE2('I', DOT, DOT),
  ENCODE_MORSE2('M', DASH, DASH),
  ENCODE_MORSE2('N', DASH, DOT),
  ENCODE_MORSE3('D', DASH, DOT, DOT),
  ENCODE_MORSE3('G', DASH, DASH, DOT),
  ENCODE_MORSE3('K', DASH, DOT, DASH),
  ENCODE_MORSE3('O', DASH, DASH, DASH),
  ENCODE_MORSE3('R', DOT, DASH, DOT),
  ENCODE_MORSE3('S', DOT, DOT, DOT),
  ENCODE_MORSE3('U', DOT, DOT, DASH),
  ENCODE_MORSE3('W', DOT, DASH, DASH),
  ENCODE_MORSE4('B', DASH, DOT, DOT, DOT),
  ENCODE_MORSE4('C', DASH, DOT, DASH, DOT),
  ENCODE_MORSE4('F', DOT, DOT, DASH, DOT),
  ENCODE_MORSE4('H', DOT, DOT, DOT, DOT),
  ENCODE_MORSE4('J', DOT, DASH, DASH, DASH),
  ENCODE_MORSE4('L', DOT, DASH, DOT, DOT),
  ENCODE_MORSE4('P', DOT, DASH, DASH, DOT),
  ENCODE_MORSE4('Q', DASH, DASH, DOT, DASH),
  ENCODE_MORSE4('V', DOT, DOT, DOT, DASH),
  ENCODE_MORSE4('X', DASH, DOT, DOT, DASH),
  ENCODE_MORSE4('Y', DASH, DOT, DASH, DASH),
  ENCODE_MORSE4('Z', DASH, DASH, DOT, DOT),
  ENCODE_MORSE5('0', DASH, DASH, DASH, DASH, DASH),
  ENCODE_MORSE5('1', DOT, DASH, DASH, DASH, DASH),
  ENCODE_MORSE5('2', DOT, DOT, DASH, DASH, DASH),
  ENCODE_MORSE5('3', DOT, DOT, DOT, DASH, DASH),
  ENCODE_MORSE5('4', DOT, DOT, DOT, DOT, DASH),
  ENCODE_MORSE5('5', DOT, DOT, DOT, DOT, DOT),
  ENCODE_MORSE5('6', DASH, DOT, DOT, DOT, DOT),
  ENCODE_MORSE5('7', DASH, DASH, DOT, DOT, DOT),
  ENCODE_MORSE5('8', DASH, DASH, DASH, DOT, DOT),
  ENCODE_MORSE5('9', DASH, DASH, DASH, DASH, DOT)
};

/*
   Encodes a letter to morse code, using '.' for dots, '-' for dashes,
   ' ' for silence between letters and '/' for silence between words.
*/
int encode_morse(char letter, char *buffer) {
  int i, j, size = 0;
  morse_code_t table_item;

  if (letter >= 'a' && letter <= 'z') {
    letter -= 32;
  }

  if (letter == ' ' || letter == '\t' || letter == '\n') {
    *buffer = '/';
    return 1;
  }

  for (i = 0; i < 36; i++) {
    memcpy_P(&table_item, &(morse_code_tbl[i]), sizeof(morse_code_t));
    if (letter == table_item.ch) {
      char data = table_item.data;
      size = (data >> 5) & 7;
      data = data & 31;
      for (j = 0; j < size; j++) {
        *(buffer++) = (data & 1) ? '.' : '-';
        data >>= 1;
      }
      *buffer = ' ';
      return size + 1;
    }
  }
  return 0;
}

/* A struct to hold timing and value of signals. */
typedef struct {
  unsigned long afterMillis;   /* Send signal when millis() >= this */
  byte value;                  /* 0 = LOW, 1 = HIGH, duh. */
} signal_t;


/* CircularBuffer to store signals. 192 items is a reasonable size
   in Arduino Uno */
CircularBuffer<signal_t, 192> signalBuffer;

/* Read bytes from serial, encode them as Morse code and enqueue
   the signals (and silences) in the circular buffer above. */
void readSerialAndEnqueueSignals() {
  while (Serial.available()) {
    int morse_size;
    char buffer[8];
    byte incomingByte;
    unsigned long currentMillis;

    incomingByte = Serial.read();

    /* If there are already enqueued signals, enqueue the next signal
       after the last one, otherwise, enqueue after current millis(). */
    if (!signalBuffer.isEmpty()) {
      currentMillis = signalBuffer.last().afterMillis;
    } else {
      currentMillis = millis();
    }

    morse_size = encode_morse(incomingByte, buffer);
    for (int i = 0; i < morse_size; i++) {
      if (buffer[i] != ' ' && buffer[i] != '/') {
        signalBuffer.push(signal_t{currentMillis, 1});
        switch (buffer[i]) {
          /* 3 time units of signal for each dash - notice there is no break ;-) */
          case '-': currentMillis += 2 * MORSE_TIME_UNIT;
          case '.': currentMillis += MORSE_TIME_UNIT; break;
        }
        signalBuffer.push(signal_t{currentMillis, 0});
      }
      switch (buffer[i]) {
        /* Notice there is no break in the first 2 cases */

        /* 7 time units of silence after each dot or dash (4, as all characters end with 3) */
        case '/': currentMillis += 2 * MORSE_TIME_UNIT;

        /* 3 time units of silence after each letter (2, as all dots and dashes end with 1) */
        case ' ': currentMillis += 2 * MORSE_TIME_UNIT;

        /* 1 time unit of silence after each dot or dash */
        case '.':
        case '-': currentMillis += MORSE_TIME_UNIT; break;


      }
      signalBuffer.push(signal_t{currentMillis, 0});
    }
  }
}


/*
   Output enqueued signals (and silences) to the pre-defined pin when
   their time (er, millis()) is come.
*/
void outputEnqueuedSignals() {
  if (!signalBuffer.isEmpty()) {
    byte nextSignal;
    unsigned long nextChange, currentMillis;

    currentMillis = millis();
    nextChange = signalBuffer.first().afterMillis;
    if (nextChange <= currentMillis) {
      nextSignal = signalBuffer.first().value;
      if (nextSignal != 3) {
        digitalWrite(OUTPUT_PIN, nextSignal);
      }
      signalBuffer.shift();
    }
  }
}


/* Finally, the setup() and loop() functions! */

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.print("Serial to Morse code running on pin ");
  Serial.println(OUTPUT_PIN);
  Serial.println("Ready.");
  pinMode(OUTPUT_PIN, OUTPUT);
}

void loop() {
  readSerialAndEnqueueSignals();
  outputEnqueuedSignals();
}
