#pragma once

#include "driver/can.h"
#include <functional>

class CanBus{

  public:

  can_filter_config_t f_config;
  can_timing_config_t t_config;
  uint32_t canId = 0x100; // general config ID
  
  void msgHandler(can_message_t msg);

  void setSegement(uint8_t num, CRGB c){
    Segment &seg = strip.getSegment(num);
    for (uint8_t i = seg.start; i <= seg.start; i++){
      strip.setPixelColor(i, c);
    }
  }

};



class CanBusMX5 : public CanBus{

  #define NUMBER_CAN_MSGS_MX5 5

  public:

  CanBusMX5( ) {     // Constructor  11 bit IDs
    f_config = {.acceptance_code = ((0b00010000101) << 21) & ((0b01000110001) << 5),
                .acceptance_mask = ((0b01010000101) << 21) & ((0b11010000001) << 5),
                .single_filter = true };
    
    t_config = CAN_TIMING_CONFIG_500KBITS();

    canId = 0x631; 

    msgHandlers[0] = &CanBusMX5::msg085Handler;
    msgHandlers[1] = &CanBusMX5::msg201Handler;
    msgHandlers[2] = &CanBusMX5::msg231Handler;
    msgHandlers[3] = &CanBusMX5::msg4B0Handler;
    msgHandlers[4] = &CanBusMX5::msg630Handler;
  }

  void msgHandler(can_message_t msg) {
    
    for (uint8_t i = 0; i < NUMBER_CAN_MSGS_MX5; i++){
      if (msg.identifier == canIds[i] ) {
        (this->*msgHandlers[i])(msg);
      }
    }
  }

  void overlayHandler(){
    if (brake){
      setSegement(4, RGBW32(255,0,0,0));
      setSegement(5, RGBW32(255,0,0,0));
    }

    //hazard  = msg.data[0] & 0b0000001;
    //indiL   = msg.data[0] & 0b0000010;
    //indiR   = msg.data[0] & 0b0000100;
    //lightL  = msg.data[0] & 0b0001000;
    //lightR  = msg.data[0] & 0b0010000;

    // TODO L&R hazards with scan
    if (indicators & 0b0000001){
      if (indicators & 0b0001000 || indicators & 0b0010000) {
        setSegement(2, RGBW32(239,183,0,0)); 
        setSegement(4, RGBW32(239,183,0,0)); 
      } else {
        setSegement(2, RGBW32(0,0,0,0));
        setSegement(4, RGBW32(0,0,0,0));
      }
    }
    if (indicators & 0b0000001 || indicators & 0b0000010){ // left or hazard
      if (indicators & 0b0001000){
        setSegement(1, RGBW32(239,183,0,0));
      } else {
        setSegement(1, RGBW32(0,0,0,0));
      }
    }
    if (indicators & 0b0000001 || indicators & 0b0000100){
      if (indicators & 0b0010000){
        setSegement(3, RGBW32(239,183,0,0));
      } else {
        setSegement(3, RGBW32(0,0,0,0));
      }
    }

  }
 
  private:

    void (CanBusMX5::*msgHandlers[NUMBER_CAN_MSGS_MX5]) (can_message_t msg);
    uint16_t canIds [NUMBER_CAN_MSGS_MX5] = {0x085, 0x201, 0x231, 0x4b0, 0x630};

    // https://github.com/timurrrr/RaceChronoDiyBleDevice/blob/master/can_db/mazda_mx5_nc.md

    // signals
    bool brake; // clutch
    uint8_t indicators; // speed, throttle pos, accel pedal pos, flSpeed, frSpeed, rlSpeed, rrSpeed
    // uint16_t rpm; // steering angle, brake pressure

    void msg085Handler (can_message_t msg) { // brake pressure & switch byte 2 bit 1
      // pressure = ((((msg.data[0] << 8) & msg.data[1]) * 3.4518689053)- 327.27) / 1000.00
      brake = msg.data[2] & 0b10000000;
    }

    void msg201Handler (can_message_t msg) {
      // rpm = ((msg.data[0] << 8) & msg.data[1]) / 4;
      // speed = (((msg.data[4] << 8) & msg.data[5]) / 100) - 100;
      // pedalPos = msg.data[6] * 2;
    }

    void msg231Handler (can_message_t msg) {
      // clutch = msg.data[1] & 0b100000000;
    }

    void msg4B0Handler (can_message_t msg) {
      //speedFL = (((msg.data[4] << 8) & msg.data[5]) / 100) - 100;
      //speedFR = (((msg.data[4] << 8) & msg.data[5]) / 100) - 100;
      //speedRL = (((msg.data[4] << 8) & msg.data[5]) / 100) - 100;
      //speedRR = (((msg.data[4] << 8) & msg.data[5]) / 100) - 100;

      // analyse to determine when drifting 
    }

    void msg630Handler (can_message_t msg) { 
      indicators = msg.data[0];
    }
};

//TODO GT86
class CanBus86 : public CanBus{

  #define NUMBER_CAN_MSGS_86 0

  public:

  CanBus86( ) {     // Constructor  11 bit IDs
    f_config = {.acceptance_code = ((0x00) << 21) & ((0x00) << 5),
                .acceptance_mask = ((0x00) << 21) & ((0x00) << 5),
                .single_filter = true };
    
    t_config = CAN_TIMING_CONFIG_500KBITS();

    canId = 0x00;

  }

  void msgHandler(can_message_t msg) {
    
    for (uint8_t i = 0; i < NUMBER_CAN_MSGS_86; i++){
      if (msg.identifier == canIds[i] ) {
        (this->*msgHandlers[i])(msg);
      }
    }
  }

  void overlayHandler(){

  }

  private:

    void (CanBus86::*msgHandlers[NUMBER_CAN_MSGS_86]) (can_message_t msg);
    uint16_t canIds [NUMBER_CAN_MSGS_86] = {}; // 0x630

};