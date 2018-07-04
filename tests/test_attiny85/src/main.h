
#include "Arduino.h"

void setup();
void heartbeat();
void reset_target(bool reset);
void loop(void);
uint8_t getch();
void pulse(int pin, int times);
void prog_lamp(int state);
uint8_t spi_transaction(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void empty_reply();
void breply(uint8_t b);
void get_version(uint8_t c);
void set_parameters() ;
void start_pmode();
void end_pmode();
void universal();
void flash(uint8_t hilo, unsigned int addr, uint8_t data) ;
void commit(unsigned int addr);
unsigned int current_page();
uint8_t write_flash_pages(int length);
void write_flash(int length) ;
uint8_t write_eeprom(unsigned int length);
uint8_t write_eeprom_chunk(unsigned int start, unsigned int length);
void program_page() ;

uint8_t flash_read(uint8_t hilo, unsigned int addr);
char flash_read_page(int length);
char eeprom_read_page(int length);

void read_page() ;
void read_signature();
void avrisp();
