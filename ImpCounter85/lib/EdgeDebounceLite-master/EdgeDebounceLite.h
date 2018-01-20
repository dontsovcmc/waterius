/*
 *EdgeDebounceLite.h 
 *Created by: Jacques bellavance, July 17 2017
 *Released into the public domain
 *GNU GENERAL PUBLIC LICENSE V3
 *
 *This library is designed to debounce switches.
 *It was inspired by this article: http://www.ganssle.com/debouncing.htm
 *It is designed to be lightweight and very fast ( <90 microseconds 99% of the time )
 *PSEUDOCODE
 * 1) Repeat
 * 1)   Read the switch n times (Between 1 and 32 times)
 * 2) Until all reads are identical
 * 3) Return the switch's status
 *    
*/
 
#ifndef EdgeDebounceLite_h
#define EdgeDebounceLite_h

#include "Arduino.h"

class EdgeDebounceLite {
  public:
    //methods
    EdgeDebounceLite();                   //Constructor
    virtual byte pin(byte pin);           //read the pin
    virtual void setSensitivity(byte w);  //Set debounce reads (1..32)
    virtual byte getSensitivity();        //Returns the current sensitivity of Debounce
    
  private:
    //Attributes
    byte MYsensitivity = 16;                      //Current sensitivity (1..32)
    unsigned long debounceDontCare = 0xffff0000;  //Don't care mask
};

#endif
