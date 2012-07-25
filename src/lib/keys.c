#include "keys.h"

void keys_init() {
    
    // konfiguracja pin�w jako wej�cia z podci�ganiem
    DDR(KEYS_S1_PORT) &= ~(1<<KEYS_S1_BIT);
    DDR(KEYS_S2_PORT) &= ~(1<<KEYS_S2_BIT);
    DDR(KEYS_S3_PORT) &= ~(1<<KEYS_S3_BIT);
    DDR(KEYS_S4_PORT) &= ~(1<<KEYS_S4_BIT);
    KEYS_S1_PORT |= (1<<KEYS_S1_BIT);
    KEYS_S2_PORT |= (1<<KEYS_S2_BIT);
    KEYS_S3_PORT |= (1<<KEYS_S3_BIT);
    KEYS_S4_PORT |= (1<<KEYS_S4_BIT);

    // stan klawiszy
    keys_state        = 0x0f;
    keys_pressed_mask = 0x00;
}

void keys_scan() {

    // aktualny stan przycisk�w -> do por�wnania ze zmienn� keys_state
    unsigned state = 0x00;
    
    // wpisuj jedynki w miejsca niewcisnietych przyciskow
    state |= (PIN(KEYS_S1_PORT) & (1<<KEYS_S1_BIT)) ? KEYS_S1 : 0;
    state |= (PIN(KEYS_S2_PORT) & (1<<KEYS_S2_BIT)) ? KEYS_S2 : 0;
    state |= (PIN(KEYS_S3_PORT) & (1<<KEYS_S3_BIT)) ? KEYS_S3 : 0;
    state |= (PIN(KEYS_S4_PORT) & (1<<KEYS_S4_BIT)) ? KEYS_S4 : 0;

    // czy jest r�znica mi�dzy aktualnym stanem a poprzednim?
    if (state != keys_state) {
        keys_pressed_mask = (~state) & 0x0F; // tak: wci�ni�to przycisk -> ustaw mask� wci�ni�tych przycisk�w
    }
    else {
        keys_pressed_mask = 0;
    }

    // przepisz aktualny stan klawiatury
    keys_state = state;
}

unsigned char keys_get(unsigned char key) {
    return (keys_pressed_mask & key) ? 1 : 0; 
}

unsigned char keys_pressed() {
    return (keys_pressed_mask != 0) ? 1 : 0;
}
