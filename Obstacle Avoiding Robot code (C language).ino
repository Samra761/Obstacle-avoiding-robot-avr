#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define LEFT_SPEED      230   
#define RIGHT_SPEED     230   
#define SAFE_DISTANCE   16.0f 

#define SERVO_MIN       1000  
#define SERVO_MAX       5000  

#define ECHO_TIMEOUT    120000UL

void timer1_init(void);
void timer0_init(void);
void gpio_init(void);
void set_servo_angle(uint8_t angle);
float get_distance(void);
float get_stable_distance(void);
float scan(uint8_t angle);
void move_forward(void);
void move_backward(void);
void turn_left(void);
void turn_right(void);
void stop_bot(void);

void timer1_init(void) {
    TCCR1A = (1 << COM1B1) | (1 << WGM11);
    TCCR1B = (1 << WGM13)  | (1 << WGM12) | (1 << CS11); 
    ICR1   = 39999;
    OCR1B  = 3000;
}

void timer0_init(void) {
    TCCR0A = (1 << WGM01) | (1 << WGM00); 
    TCCR0B = (1 << CS01)  | (1 << CS00);  
}

void gpio_init(void) {
    DDRB |=  (1 << PB4) | (1 << PB2);
    DDRD |=  (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);

    DDRB &= ~(1 << PB0);

    PORTD &= ~((1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7));
}

void set_servo_angle(uint8_t angle) {
    OCR1B = SERVO_MIN + ((uint32_t)angle * (SERVO_MAX - SERVO_MIN) / 180);
}

float get_distance(void) {
    uint32_t count = 0;

    PORTB &= ~(1 << PB4); _delay_us(2);
    PORTB |=  (1 << PB4); _delay_us(10);
    PORTB &= ~(1 << PB4);

    while (!(PINB & (1 << PB0))) {
        count++;
        if (count >= ECHO_TIMEOUT) return 400.0f;
    }

    count = 0;

    while (PINB & (1 << PB0)) {
        count++;
        if (count >= ECHO_TIMEOUT) return 400.0f;
    }

    return (float)count * 0.004288f;
}

float get_stable_distance(void) {
    float total = 0.0f;
    for (uint8_t i = 0; i < 4; i++) {
        total += get_distance();
        _delay_ms(25);
    }
    return total / 4.0f;
}

float scan(uint8_t angle) {
    set_servo_angle(angle);
    _delay_ms(500); 
    return get_stable_distance();
}

void move_forward(void) {
    // Motor A
    TCCR0A |= (1 << COM0A1);     
    OCR0A   = LEFT_SPEED;        
    PORTD  &= ~(1 << PD7);       

    // Motor B
    TCCR0A |= (1 << COM0B1);     
    OCR0B   = RIGHT_SPEED;       
    PORTD  &= ~(1 << PD4);       
}

void move_backward(void) {
    // Motor A
    TCCR0A &= ~(1 << COM0A1);    
    PORTD  &= ~(1 << PD6);       
    PORTD  |=  (1 << PD7);       

    // Motor B
    TCCR0A &= ~(1 << COM0B1);    
    PORTD  &= ~(1 << PD5);       
    PORTD  |=  (1 << PD4);       
}

void turn_left(void) {
    // Right motor forward
    TCCR0A |= (1 << COM0A1);
    OCR0A = LEFT_SPEED;
    PORTD &= ~(1 << PD7);

    // Left motor backward
    TCCR0A &= ~(1 << COM0B1);
    PORTD &= ~(1 << PD5);
    PORTD |= (1 << PD4);
}

void turn_right(void) {
    // Right motor backward
    TCCR0A &= ~(1 << COM0A1);
    PORTD &= ~(1 << PD6);
    PORTD |= (1 << PD7);

    // Left motor forward
    TCCR0A |= (1 << COM0B1);
    OCR0B = RIGHT_SPEED;
    PORTD &= ~(1 << PD4);
}

void stop_bot(void) {
    TCCR0A &= ~((1 << COM0A1) | (1 << COM0B1));
    PORTD  &= ~((1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7));
}

/* ================= REFINED MAIN ================= */

int main(void) {
    gpio_init();
    timer1_init();
    timer0_init();

    // Initial Servo Calibration
    set_servo_angle(90);
    _delay_ms(1000);

    while (1) {
        float front = get_stable_distance();

        if (front > SAFE_DISTANCE) {
            move_forward();
            // Short delay to prevent "jittery" movement
            _delay_ms(50); 
        } else {
            // 1. Obstacle detected: Stop and think
            stop_bot();
            _delay_ms(200);

            // 2. Scan surroundings
            float left  = scan(150); // 160 can sometimes hit the chassis
            float right = scan(30);
            
            // Look forward again before deciding
            set_servo_angle(90);
            _delay_ms(300);

            // 3. Back up to create turning space
            move_backward();
            _delay_ms(500);
            stop_bot();
            _delay_ms(200);

            // 4. Turn toward the clearer side
            // Increase delay if it's not turning enough (90 degrees is ideal)
            if (left > right) {
                turn_left();
                _delay_ms(700); 
            } else {
                turn_right();
                _delay_ms(700);
            }

            // 5. Hard stop before restarting the loop
            stop_bot();
            _delay_ms(400); 
            
            // Now the loop restarts, checks the front, and moves if clear.
        }
    }
    return 0;
}