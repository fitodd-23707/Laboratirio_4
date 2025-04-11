#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Variables globales
volatile uint8_t counter = 0;          // Contador binario de 8 bits
volatile uint8_t lecADC = 0;           // Valor del ADC (0-255)
volatile uint8_t last_inc = 1;         // Estado anterior botón incremento (PB2)
volatile uint8_t last_dec = 1;         // Estado anterior botón decremento (PB3)

// Tabla para displays de 7 segmentos (Hex, cátodo común)
const uint8_t TABLITA[16] = {
	0x3F, // 0
	0x06, // 1
	0x5B, // 2
	0x4F, // 3
	0x66, // 4
	0x6D, // 5
	0x7D, // 6
	0x07, // 7
	0x7F, // 8
	0x6F, // 9
	0x77, // A
	0x7C, // B
	0x39, // C
	0x5E, // D
	0x79, // E
	0x71  // F
};

// Configuración inicial
void setup() {
	cli();
	DDRB &= ~((1 << PORTB2) | (1 << PORTB3));  // PB2 y PB3 como entradas
	PORTB |= (1 << PORTB2) | (1 << PORTB3);    // Pull-ups
	DDRD = 0xFF;                               // Segmentos de los displays
	DDRC |= (1 << PORTC0) | (1 << PORTC1) | (1 << PORTC2) | (1 << PORTC3); // Alarma + Control de displays
	
	// Configurar PORTB0-PORTB1 como salidas para los LEDs del contador binario
	DDRB |= (1 << PORTB0) | (1 << PORTB1);
	
	// ADC Canal 7 (0-255)
	ADMUX = (1 << REFS0) | (1 << ADLAR) | (7 << MUX0); // AVCC, ajuste izquierdo
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Prescaler 128
	ADCSRA |= (1 << ADSC);               // Iniciar conversión
	
	// Timer2 para multiplexado (frecuencia ~1kHz)
	TCCR2A = 0x00;                       // Modo normal
	TCCR2B = (1 << CS21);                // Prescaler 8
	TIMSK2 = (1 << TOIE2);               // Habilitar interrupción
	TCNT2 = 150;
	
	sei();
}

// Interrupción ADC
ISR(ADC_vect) {
	lecADC = ADCH;                       // Leer valor de 8 bits (0x00 a 0xFF)
	ADCSRA |= (1 << ADSC);               // Reiniciar conversión
}

// Interrupción Timer2 (refresco de displays)
ISR(TIMER2_OVF_vect) {
	static uint8_t disp = 0;
	
	// Apagar todos los displays primero
	PORTC &= ~((1 << PORTC1) | (1 << PORTC2) | (1 << PORTC3));
	
	switch(disp) {
		case 0: // Mostrar contador binario en LEDs (PB0-PB1) y display
		PORTB = (PORTB & 0xFC) | (counter & 0x03); // LEDs en PB0-PB1
		PORTD = counter;             // Mostrar en display principal
		PORTC |= (1 << PORTC1);      // Habilitar display del contador
		break;
		
		case 1: // Nibble bajo del ADC (Unidades hexadecimales)
		PORTD = TABLITA[lecADC & 0x0F];
		PORTC |= (1 << PORTC2);      // Habilitar display de unidades
		break;
		
		case 2: // Nibble alto del ADC (Decenas hexadecimales)
		PORTD = TABLITA[(lecADC >> 4) & 0x0F];
		PORTC |= (1 << PORTC3);      // Habilitar display de decenas
		break;
	}
	
	disp = (disp + 1) % 3;
	TCNT2 = 150;                        // Reiniciar timer
}

int main(void) {
	setup();
	while(1) {
		// Antirebote para PB2 (incremento)
		if (!(PINB & (1 << PORTB2)) && last_inc) {
			_delay_ms(20);
			if (!(PINB & (1 << PORTB2))) {
				counter++;               // Incrementar contador
				last_inc = 0;
			}
			} else if (PINB & (1 << PORTB2)) {
			last_inc = 1;
		}
		
		// Antirebote para PB3 (decremento)
		if (!(PINB & (1 << PORTB3)) && last_dec) {
			_delay_ms(20);
			if (!(PINB & (1 << PORTB3))) {
				counter--;               // Decrementar contador
				last_dec = 0;
			}
			} else if (PINB & (1 << PORTB3)) {
			last_dec = 1;
		}
		
		// Comparar ADC con contador y activar alarma
		if (lecADC > counter) {
			PORTC |= (1 << PORTC0);     // Encender LED de alarma
			} else {
			PORTC &= ~(1 << PORTC0);    // Apagar LED de alarma
		}
	}
}