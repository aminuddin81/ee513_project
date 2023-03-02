#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <iomanip>
#include <ctime>
#include <time.h>
#include <unistd.h>  // For usleep()
#include "I2CDevice.h"

#define DS3231_ADDRESS 0x68
#define REG_SEC		0x00
#define REG_MIN		0x01
#define REG_HOUR	0x02
#define REG_DOW		0x03
#define REG_DATE	0x04
#define REG_MON		0x05
#define REG_YEAR	0x06
#define REG_CON		0x0e
#define REG_STATUS	0x0f
#define REG_AGING	0x10
#define REG_TEMPM	0x11
#define REG_TEMPL	0x12

#define SQW_RATE_1		0
#define SQW_RATE_1K		1
#define SQW_RATE_4K		2
#define SQW_RATE_8K		3

#define OUTPUT_SQW		0
#define OUTPUT_INT		1

using namespace std;

// Convert the bcd registers to decimal
uint8_t bcd2dec(uint8_t b) { 
    uint8_t upper_bcd = (b >> 4) * 10;
    uint8_t lower_bcd = b & 0x0F;
    return upper_bcd + lower_bcd; 
    }

// convert decimal back to bcd
uint8_t DecimalToBCD (uint8_t d) {
    uint8_t upper_bcd = (d/10) << 4;
    uint8_t lower_bcd = (d % 10);
    return upper_bcd | lower_bcd; 
    }

void set_dateTime() {
    EE513::I2CDevice rtc(/*bus*/2, /*device*/0x68);

    // current date/time based on current system
    time_t now = time(0);
    struct tm *ltm = localtime(&now);

    rtc.writeRegister(REG_SEC, DecimalToBCD((uint8_t) ltm->tm_sec));
    rtc.writeRegister(REG_MIN, DecimalToBCD((uint8_t) ltm->tm_min));
    rtc.writeRegister(REG_HOUR, DecimalToBCD((uint8_t) ltm->tm_hour));
    rtc.writeRegister(REG_DOW, DecimalToBCD((uint8_t) ltm->tm_wday));
    rtc.writeRegister(REG_DATE, DecimalToBCD((uint8_t) ltm->tm_mday));
    rtc.writeRegister(REG_MON, DecimalToBCD((uint8_t) ltm->tm_mon + 1));
    rtc.writeRegister(REG_YEAR, DecimalToBCD((uint8_t) ltm->tm_year - 100));
    
    }
   
// Read the time and date registers starting at address 0x00 to 0x06
void get_dateTime() {
    EE513::I2CDevice rtc(/*bus*/2, /*device*/0x68);

    uint8_t seconds = bcd2dec(rtc.readRegister(REG_SEC));
    uint8_t minutes = bcd2dec(rtc.readRegister(REG_MIN));
    uint8_t hours = bcd2dec(rtc.readRegister(REG_HOUR));
    uint8_t dow = bcd2dec(rtc.readRegister(REG_DOW));
    uint8_t date = bcd2dec(rtc.readRegister(REG_DATE));
    uint8_t month = bcd2dec(rtc.readRegister(REG_MON));
    uint16_t year = bcd2dec(rtc.readRegister(REG_YEAR)) + 2000;
    
    // Display the time and date values
    cout << "Time: " << (int)hours << ":" << (int)minutes << ":" << (int)seconds << endl;
    cout << "Date: " << (int)date << "/" << (int)month << "/" << year << endl;

    }

float get_temp() {
    EE513::I2CDevice rtc(/*bus*/2, /*device*/0x68);
	uint8_t _msb = rtc.readRegister(REG_TEMPM);
	uint8_t _lsb = rtc.readRegister(REG_TEMPL);
	return (float)_msb + ((_lsb >> 6) * 0.25f);
    }

// Set the alarm1 to trigger at 12:00:00
void set_alarm1() {
    EE513::I2CDevice rtc(/*bus*/2, /*device*/0x68);

    rtc.writeRegister(0x07, 0x00);
    rtc.writeRegister(0x08, 0x00);
    rtc.writeRegister(0x09, 0x12);

    printf("Set alarm1 to 12: 00: 00\n");

    EE513::I2CDevice ds3231(2, DS3231_ADDRESS);

    // Enable the alarm1 interrupt on the SQW/INT pin
    ds3231.writeRegister(0x0E, 0x01);

    // Set the alarm1 to trigger every hour at minute 0
    ds3231.writeRegister(0x07, 0x00);  // Minute
    ds3231.writeRegister(0x08, 0x80);  // Hour
    ds3231.writeRegister(0x09, 0x80);  // Day of week / date (don't care for hourly alarm)
}

// Set the alarm2 to trigger at 14:30:00
void set_alarm2() {
    EE513::I2CDevice rtc(/*bus*/2, /*device*/0x68);

    rtc.writeRegister(0x0B, 0x00);
    rtc.writeRegister(0x0C, 0x30);
    rtc.writeRegister(0x0D, 0x14);

    printf("Set alarm2 to 14: 30: 00\n");
}


int main (int argc, char *argv[]) {

// Open I2C bus on the BeagleBone Black and connect to DS3231
EE513::I2CDevice ds3231(2, DS3231_ADDRESS);

// Set the alarm to trigger at 6 pm
ds3231.writeRegister(0x07, 0x00);   // Set seconds to 0
ds3231.writeRegister(0x08, 0x10);   // Set minutes to 0
ds3231.writeRegister(0x09, 0x18);   // Set hour to 6 pm

// Enable Alarm 1 and set the interrupt to output on INT/SQW pin
uint8_t control = ds3231.readRegister(0x0E);
control |= 0x01;        // Set A1IE bit
control |= 0x04;        // Set INTCN bit to output interrupt on INT/SQW pin
ds3231.writeRegister(0x0E, control);

// Set the alarm mask to trigger when hours, minutes, and seconds match
uint8_t mask = ds3231.readRegister(0x0A);
mask |= 0x07;           // Set A1M4, A1M3, and A1M2 bits to 1
ds3231.writeRegister(0x0A, mask);

std::cout << "Alarm 1 set for 6 pm." << std::endl;

// Continuously check for Alarm 1 trigger
while (true) {
    uint8_t status = ds3231.readRegister(0x0F);
    if ((status & 0x01) != 0) {
        std::cout << "Alarm 1 triggered!" << std::endl;

        // Clear the Alarm 1 flag
        ds3231.writeRegister(0x0F, status & ~0x01);
    }

    // Wait for 1 second before checking again
    usleep(1000000);
}
    return 0;
   
}
