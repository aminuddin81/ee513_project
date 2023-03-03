#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <iomanip>
#include <time.h>
#include <unistd.h>  // For usleep()
#include <bitset>
#include "I2CDevice.h"

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

enum SquareWaveFrequency {
    FREQ_1HZ,
    FREQ_1024HZ,
    FREQ_4096HZ,
    FREQ_8192HZ
};

class RTC : public EE513::I2CDevice {
public:
    RTC(int bus, int device) : EE513::I2CDevice(bus, device) {}

    // Set the current time and date to RTC registers
    void set_dateTime() {
        
        time_t now = time(0);   // current date&time on linux system
        struct tm *ltm = localtime(&now);

        writeRegister(0x00, DecimalToBCD((uint8_t) ltm->tm_sec));
        writeRegister(0x01, DecimalToBCD((uint8_t) ltm->tm_min));
        writeRegister(0x02, DecimalToBCD((uint8_t) ltm->tm_hour));
        writeRegister(0x03, DecimalToBCD((uint8_t) ltm->tm_wday));
        writeRegister(0x04, DecimalToBCD((uint8_t) ltm->tm_mday));
        writeRegister(0x05, DecimalToBCD((uint8_t) ltm->tm_mon + 1));
        writeRegister(0x06, DecimalToBCD((uint8_t) ltm->tm_year - 100));

        // Display the time and date values
        cout << "Time: " << ltm->tm_hour << ":" << ltm->tm_min << ":" << ltm->tm_sec << " has been set." << endl;
        cout << "Date: " << ltm->tm_mday << "/" << ltm->tm_mon + 1 << "/" << ltm->tm_year - 100 << " has been set." << endl;
        cout << "Day: " << ltm->tm_wday << " has been set." << endl;
    }
   
    // Read the time and date RTC registers starting at address 0x00 to 0x06
    void get_dateTime() {
        uint8_t seconds = bcd2dec(readRegister(0x00));
        uint8_t minutes = bcd2dec(readRegister(0x01));
        uint8_t hours = bcd2dec(readRegister(0x02));
        uint8_t dow = bcd2dec(readRegister(0x03));
        uint8_t date = bcd2dec(readRegister(0x04));
        uint8_t month = bcd2dec(readRegister(0x05));
        uint16_t year = bcd2dec(readRegister(0x06)) + 2000;
    
        // Display the time and date values
        cout << "RTC Time is: " << (int)hours << ":" << (int)minutes << ":" << (int)seconds << endl;
        cout << "RTC Date is: " << (int)date << "/" << (int)month << "/" << year << endl;
        cout << "RTC Day is: " << (int)dow << endl;
    }

    float get_temperature() {
        uint8_t msb = readRegister(0x11);
        uint8_t lsb = readRegister(0x12);
        return (float)msb + ((lsb >> 6) * 0.25f);
    }

    // Set the alarm1 
    void set_alarm1() {

        // Set the alarm to trigger every day at 6 pm
        writeRegister(0x07, DecimalToBCD(0));   // Set seconds to 0
        writeRegister(0x08, DecimalToBCD(0));   // Set minutes to 0
        writeRegister(0x09, DecimalToBCD(18));   // Set hour to 6 pm

        // Enable Alarm 1 and set the interrupt to output on INT/SQW pin
        uint8_t control = readRegister(0x0E);
        control |= 0x01;        // Set A1IE bit
        control |= 0x04;        // Set INTCN bit to output interrupt on INT/SQW pin
        writeRegister(0x0E, control);

        // Set the alarm mask to trigger when hours, minutes, and seconds match
        uint8_t mask = readRegister(0x0A);
        mask |= 0x0F;           // Set A1M4, A1M3, and A1M2 bits to 1
        writeRegister(0x0A, mask);

        std::cout << "Alarm 1 set for every day at 6 PM." << std::endl;

        // Continuously check for Alarm 1 trigger
        /**while (true) {
            uint8_t status = readRegister(0x0F);
            if ((status & 0x01) != 0) {
                std::cout << "Alarm 1 triggered!" << std::endl;

                // Clear the Alarm 1 flag
                writeRegister(0x0F, status & ~0x01);
            }

            // Wait for 1 second before checking again
            usleep(1000000);
        }**/
    }

    // Set the alarm2
    void set_alarm2() {
    
        // Set the alarm rate to "Alarm when day, hours, and minutes match" for 12:30 PM
        writeRegister(0x0D, DecimalToBCD(0)); // Set the day of the month to match
        writeRegister(0x0C, DecimalToBCD(12)); // Set the hour to match
        writeRegister(0x0B, DecimalToBCD(30)); // Set the minute to match

        // Enable Alarm 2 interrupt
        uint8_t control = readRegister(0x0E); // Read the current value of the control register
        control |= 0b00000100; // Set bit 2 to 1 to enable Alarm 2
        writeRegister(0x0E, control); // Write the new value back to the control register

        // Set the DY/DT Alarm 2 Register Mask Bits (BIT 7) to 1 and A2M4, A2M3, and A2M2 bits to 0
        uint8_t mask = readRegister(0x0D);
        mask &= 0b00001111; // Clear A2M4, A2M3, and A2M2 bits
        mask |= 0b10000000; // Set BIT 7 to 1
        writeRegister(0x0D, mask);

        std::cout << "Alarm 2 set for trigger once per day at 12:30 PM." << std::endl;

        // Continuously check for Alarm 2 trigger
        /**while (true) {
            uint8_t status = readRegister(0x0F);
                if ((status & 0x02) != 0) {
                    std::cout << "Alarm 2 triggered!" << std::endl;

                    // Clear the Alarm 2 flag
                    writeRegister(0x0F, status & ~0x02);
                    }

            // Wait for 1 second before checking again
            usleep(1000000);
        }**/
    }

    void read_alarm1() {
        // Read the alarm 1 settings
        uint8_t seconds = bcd2dec(readRegister(0x07));
        uint8_t minutes = bcd2dec(readRegister(0x08));
        uint8_t hours = bcd2dec(readRegister(0x09));
        uint8_t mask = readRegister(0x0A);
        uint8_t control = readRegister(0x0E);

        // Print the settings to the console
        std::cout << "Alarm 1 settings:" << std::endl;
        std::cout << "Seconds: " << static_cast<int>(seconds) << std::endl;
        std::cout << "Minutes: " << static_cast<int>(minutes) << std::endl;
        std::cout << "Hours: " << static_cast<int>(hours) << std::endl;
        std::cout << "Mask: " << std::bitset<8>(mask) << std::endl;
        std::cout << "Control: " << std::bitset<8>(control) << std::endl;
    }

    void read_alarm2() {
        // Read the alarm 2 settings
        uint8_t minutes = bcd2dec(readRegister(0x0B));
        uint8_t hours = bcd2dec(readRegister(0x0C));
        uint8_t mask = readRegister(0x0D);
        uint8_t control = readRegister(0x0E);

        // Print the settings to the console
        std::cout << "Alarm 2 settings:" << std::endl;
        std::cout << "Minutes: " << static_cast<int>(minutes) << std::endl;
        std::cout << "Hours: " << static_cast<int>(hours) << std::endl;
        std::cout << "Mask: " << std::bitset<8>(mask) << std::endl;
        std::cout << "Control: " << std::bitset<8>(control) << std::endl;
    }

    void set_SquareWaveFrequency(EE513::I2CDevice& rtc, SquareWaveFrequency freq) {
        uint8_t control = readRegister(0x0E);

        // Clear the RS2 and RS1 bits
        control &= ~(0x03 << 3);

        switch (freq) {
            case FREQ_1HZ:
                // Set the RS1 bit to 0 and the RS2 bit to 0
                break;
            case FREQ_1024HZ:
                // Set the RS1 bit to 1 and the RS2 bit to 0
                control |= (0x01 << 3);
                break;
            case FREQ_4096HZ:
                // Set the RS1 bit to 0 and the RS2 bit to 1
                control |= (0x02 << 3);
                break;
            case FREQ_8192HZ:
                // Set the RS1 bit to 1 and the RS2 bit to 1
                control |= (0x03 << 3);
                break;
            default:
                std::cerr << "Invalid frequency." << std::endl;
                return;
        }

        // Enable the square-wave output on the INT/SQW pin
        control |= 0x40;

        writeRegister(0x0E, control);
        // Print the RS2 & RS1 bit to the console
        std::cout << "Square Wave Frequency selected is: " << freq << std::endl;
        std::cout << "RS2 and RS1 Control is: " << std::bitset<8>(control) << std::endl;
    }

};

int main() {
    RTC rtc(2, 0x68); // Create an object of RTC class
    
    // Set the square wave frequency to 4096 Hz
    rtc.set_SquareWaveFrequency(rtc, FREQ_4096HZ);
    usleep(1000000);
    
    return 0;
}

    //cout << "Temperature is: " << rtc.get_temperature() << " degrees Celsius" << endl;  // read current temperature of RTC
    /**rtc.set_alarm1();
    usleep(1000000);
    rtc.read_alarm1();

    rtc.set_alarm2();
    usleep(1000000);
    rtc.read_alarm2();**/

    /**rtc.set_dateTime();  // set date&time to RTC register
    //usleep(1000000);
    //rtc.get_dateTime(); // read date&time from RTC register
       
    //rtc.set_alarm1();   // Set the alarm1 to trigger at 6 pm
    **/
    /**
    // Continuously check for Alarm 1 trigger
        //while (true) {
            //uint8_t status = rtc.readRegister(0x0F);
            //if ((status & 0x01) != 0) {
                //std::cout << "Alarm 1 triggered!" << std::endl;

                // Clear the Alarm 1 flag
                //rtc.writeRegister(0x0F, status & ~0x01);
            //}

            // Wait for 1 second before checking again
            //usleep(1000000);
        //}**/
    
    //rtc.set_alarm2();   // Set the alarm2
    /**rtc.set_alarm2();  // set alarm 2
    // Continuously check for Alarm 2 trigger
        while (true) {
            uint8_t status = rtc.readRegister(0x0F);
                if ((status & 0x02) != 0) {
                    std::cout << "Alarm 2 triggered!" << std::endl;

                    // Clear the Alarm 2 flag
                    rtc.writeRegister(0x0F, status & ~0x02);
                    }

            // Wait for 1 second before checking again
            usleep(1000000);
        }**/
    // Set the square wave frequency to 4096 Hz
    //set_SquareWaveFrequency(rtc, FREQ_4096HZ);

    // Wait for 1 second before exiting
    //usleep(1000000);
    //rtc.read_alarm2();  // read alarm 2 settings
