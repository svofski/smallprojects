///\file main.c
///\author Viacheslav Slavinsky
///
///\brief Strobeshnik
///
/// \mainpage Strobeshnik strobes digits in a spinning disc to display time

/// \section Files
/// - main.c    main file
/// - timekeep.c non-RTC-based timekeeping stuff
///

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <stdio.h>
#include <stdlib.h>

#include <util/delay.h>

#include "usrat.h"
#include "util.h"
#include "buttonry.h"
#include "modes.h"
#include "timekeep.h"

volatile union _timebcd {
    uint16_t time;
    struct _hhmm {
        uint8_t mm;
        uint8_t hh;
    } hhmm;
} time;

volatile uint8_t blinktick;

#define TIMERCOUNT 98

volatile uint8_t motorcoils = 1;

/// Init display-related DDRs.
void initdisplay() {
    DDRSTROBE |= BV5(0,1,2,3,4);
    PORTMOTOR &= ~BV3(MOT1,MOT2,MOT3);
    DDRMOTOR |= BV3(MOT1,MOT2,MOT3);
}

void extint_init() {
    DDRD &= ~BV2(2,3);
    EICRA = _BV(ISC01); // falling edge of INT0 
    EIMSK = _BV(INT0);  // enable INT0
}

/// Return current BCD display value 
inline uint16_t get_display_value() {
    return time.time;
}

/// Start timer 0.
void timer0_init() {
    TIMSK0 |= _BV(TOIE0);    // enable Timer0 overflow interrupt
    TCNT0 = 256-TIMERCOUNT;
    TCCR0B = _BV(CS01);     // 20MHz/8

//    TCCR2B = _BV(CS20);     // 20MHz
//    TIMSK2 |= _BV(TOIE2);
}

#define TICKS_PER_SECOND 23200
#define DIGITTIME 64

uint16_t globalctr = 0;

int16_t phase1 = 174-384, phase2 = 125-384, phase3 = 88-384, phase4 = 49-384, phase5 = 0-384;
struct _phase_precalc {
    int16_t p1, p2, p3, p4, p5;
} phasepre;

int16_t strobectr = -768/2;
int16_t strobeindexmark = 32;

uint16_t spintime = 2048;// 2112; //2240;//400;//352;
uint16_t spinctr = 352;

uint8_t motorduty = 0;
uint8_t mduty2 = 0;
volatile uint8_t eightctr = 0;

uint8_t motorduty_set = 40; // 48

uint16_t strobe_fullspin = 768;
uint16_t strobe_halfspin = 384;

volatile uint16_t time_ctr = 0;
volatile uint16_t blinkctr = TICKS_PER_SECOND/4;


enum _spinup {
    SPIN_START = 0,
    SPIN_SPUN,
    SPIN_DUTYDOWN,
    SPIN_STABLE
};
uint8_t spinned_up;

uint8_t motorbits = 1;

uint8_t halfctr = 0;

ISR(TIMER0_OVF_vect) {
    uint8_t yes;
    
    TCNT0 = 256-TIMERCOUNT/2;
    
    ++halfctr;
    //if ((halfctr & 3) != 0) {
    if ((halfctr & 1) != 0) {
        //if (spinned_up == SPIN_STABLE && ((motorduty_set - motorduty < 2) || (motorduty < 6)) )  PORTMOTOR &= ~BV3(MOT1,MOT2,MOT3);
        return;
    }

    yes = PORTSTROBE & ~BV5(0,1,2,3,4);


    do {
        if (blinkmode_get() == BLINK_ALL && ((blinktick & _BV(1)) == 0)) break;
        
        if (blinkmode_get() != BLINK_HH || (blinktick & _BV(1))) {
            if (abs(strobectr - phasepre.p1) < 2) {
                yes |= _BV(0);
            } 
            
            if (abs(strobectr - phasepre.p2) < 2) {
                yes |= _BV(1);
            } 
        }
    
        if (blinkmode_get() != BLINK_MM || (blinktick & _BV(1))) {
            if (abs(strobectr - phasepre.p4) < 2) {
                yes |= _BV(3);
            } 
            
            if (abs(strobectr - phasepre.p5) < 2) {
                yes |= _BV(4);
            } 
        }
    } while (0);

    if (dotmode != DOT_OFF && ((blinktick & _BV(1)) || dotmode == DOT_ON) && abs(strobectr - phasepre.p3) < 2) {
        yes |= _BV(2);
    }
    
    PORTSTROBE = yes;

    
    if (spinned_up == SPIN_START && ((globalctr & 0xfff) == 0)) {
        if (spintime > 352) {
            spintime -= 16;
        } else {
            spintime-=2;
        }
        if (spintime == 64) {
            spinned_up = SPIN_SPUN;
        }
    }

    if (--spinctr == 0) {
        spinctr = spintime;
        motorbits <<= 1;
        if (motorbits == 8) {
            motorbits = 1;
        }

        motorduty = spintime > 300 ? 200 : motorduty_set;
    }
    
    yes = PORTMOTOR & ~BV3(MOT1,MOT2,MOT3);
    
    if (motorduty > 0) {
        --motorduty;

        yes |= motorbits;
    } 

    PORTMOTOR = yes;
    
    strobectr++;
    if (strobectr == strobe_halfspin) {
        strobectr = -strobe_halfspin;
    }
    
    globalctr++;
    
    if (--time_ctr == 0) {
        time_ctr = TICKS_PER_SECOND;// 25511
        blinktick |= _BV(0);
    } 
    
    if (blinkctr > 0) {
        blinkctr--;
    }
    
    if ((globalctr & 0x1fff) == 0) {
        switch (spinned_up) {
            case SPIN_START:
                break;
            case SPIN_SPUN:
                spinned_up = SPIN_DUTYDOWN;
                break;
            case SPIN_DUTYDOWN:
                motorduty_set--;
                if (motorduty_set == 12) {
                    spinned_up = SPIN_STABLE;
                }
                break;
            case SPIN_STABLE:
                break;
        }
    }
}


ISR(INT0_vect) {
    int16_t error = strobectr - strobeindexmark;
    if (abs(error) > 1) {
        strobe_fullspin = 768+(error < 0 ? -2 : 2);  // seek
        strobe_halfspin = strobe_fullspin/2;
    } else {
        strobe_fullspin = 768;
        strobe_halfspin = 384;
    }
}

/// Start fading time to given value.
/// Transition is performed in TIMER0_OVF_vect and takes FADETIME cycles.
void fadeto(uint16_t t) {
    time.time = t;
    phasepre.p1 = phase1 + ((((time.hhmm.hh&0xf0)>>4))<<6);
    phasepre.p2 = phase2 + ((((time.hhmm.hh&0x0f)>>0))<<6);
    phasepre.p3 = phase3 + (10<<6);
    phasepre.p4 = phase4 + ((((time.hhmm.mm&0xf0)>>4))<<6);
    phasepre.p5 = phase5 + ((((time.hhmm.mm&0x0f)>>0))<<6);  
}

/// Program main
int main() {
    uint8_t i;
    uint8_t byte;
    volatile uint16_t skip = 0;
    uint8_t uart_enabled = 0;
    volatile uint16_t mmss, mmss1;

    usart_init(F_CPU/16/19200-1);
    
    printf_P(PSTR("\033[2J\033[HB%s CAN WE MAKE IT BACK TO EARTH? %02x\n"), BUILDNUM, MCUCR);


    spinned_up = SPIN_START;
    
    sei();

    timer0_init();

    initdisplay();
    dotmode_set(DOT_BLINK);
    buttons_init();
    extint_init();

    
    for(i = 0;;i++) {
        wdt_reset();
        
        // handle keyboard commands
        if (uart_available()) {
            byte = uart_getchar();
            switch (uart_enabled) {
                case 0: if (byte == 'z') 
                            uart_enabled = 1;
                        else
                            uart_enabled = 0;
                        break;
                case 1: if (byte == 'c') 
                            uart_enabled = 2;
                        else
                            uart_enabled = 0;
                        break;
                case 2:
                        switch (byte) { 
                        case 'd':   phase1--; break;
                        case 'D':   phase1++; break;
                        case 'f':   phase2--; break;
                        case 'F':   phase2++; break;
                        case 'g':   phase3--; break;
                        case 'G':   phase3++; break;
                        case 'h':   phase4--; break;
                        case 'H':   phase4++; break;
                        case 'j':   phase5--; break;
                        case 'J':   phase5++; break;
                        case 'a':   strobe_fullspin--;
                                    break;
                        case 's':   strobe_fullspin++;
                                    break;
                        case 'e':   motorduty_set--;
                                    break;
                        case 'r':   motorduty_set++;
                                    break;
                        case '1':   strobeindexmark--;
                                    break;
                        case '2':   strobeindexmark++;
                                    break;
                        case 't':   time.time = 0;
                                    break;
                        case '.':   break;
                        default:
                                    break;
                        }
            
                        if (byte >= '0' && byte <= '9') {
                            byte = byte - '0';
                            fadeto((byte<<12)+(byte<<8)+(byte<<4)+byte);
                            skip = 255;
                        }
                        
                        printf_P(PSTR("time=%04x ph=%d,%d,%d,%d,%d fullspin=%d duty=%d strobeinexmark=%d\n"), 
                            time, phase1,phase2,phase3,phase4,phase5, 
                            strobe_fullspin, motorduty_set, strobeindexmark
                            );
                        break;
            }
        }


        buttonry_tick();

        if (blinktick & _BV(0) != 0) {
            blinktick &= ~_BV(0);

            time_nextsecond();
        }
        
        switch (mode_get()) {
            case HHMM:
                fadeto(time_get_hhmm());
                break;
            default:
            case MMSS:
                fadeto(time_get_mmss());
                break;
        }

        if (blinkctr == 0) {
            cli();
            blinkctr = TICKS_PER_SECOND/4;
            sei();
            if (blinkhandler != NULL) {
                blinkhandler(1);
            }
        }

    
        if (time_ctr > TICKS_PER_SECOND/2) {
            blinktick |= _BV(1);
        } else {
            blinktick &= ~_BV(1);
        }
        
        _delay_ms(100);

        // just waste time
        //while((blinktick & _BV(2)) == 0) {
        //    sleep_enable();
        //    sleep_cpu();
        //}
    }
}

