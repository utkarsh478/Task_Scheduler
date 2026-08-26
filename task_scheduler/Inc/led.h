#ifndef LED_H_
#define LED_H_

#include <stdint.h>

#define LED_GREEN   5U      /* PA5 - on-board LD2   D13*/
#define LED_ORANGE  6U      /* PA6 - external     D12*/
#define LED_RED     7U      /* PA7 - external     D11*/
#define LED_BLUE    8U      /* PA8 - external     D7*/


#define DELAY_COUNT_1MS      1250U
#define DELAY_COUNT_1S       (1000U * DELAY_COUNT_1MS)
#define DELAY_COUNT_500MS    (500U  * DELAY_COUNT_1MS)
#define DELAY_COUNT_250MS    (250U  * DELAY_COUNT_1MS)
#define DELAY_COUNT_125MS    (125U  * DELAY_COUNT_1MS)

void led_init_all(void);
void led_on(uint8_t led_no);
void led_off(uint8_t led_no);
void led_toggle(uint8_t led_no);
void delay(uint32_t count);

#endif /* LED_H_ */
