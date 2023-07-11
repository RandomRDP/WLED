#pragma once

#include "wled.h"
#include "driver/can.h"
#include "usermod_can_bus_sigs.h"

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * 
 * Using a usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
 */

// TODO more configurabilty though web app / CAN / API

typedef enum vehicle_t {
  None,
  MX5,
  GT86
};

class CanBusUsermod : public Usermod {

  private:

    // Private class members. You can declare variables and functions only accessible to your usermod here
    bool enabled = false;
    bool initDone = false;
    unsigned long lastTime = 0;

    // string that are used multiple time (this will save some flash memory)
    static const char _name[];
    static const char _enabled[];
    static const char _ctxPin[];
    static const char _crxPin[];

    int8_t canState = 0; // disabled = -2, misconfigured = -1, start 0, configured = 1, running = 2
    uint8_t baudrate;
    vehicle_t vehicle;
    uint32_t canId;
    int8_t ctxPin = -1;
    int8_t crxPin = -1;

    CanBusMX5* mx5;
    CanBus86* gt86;
    

    can_timing_config_t t_config;   // = CAN_TIMING_CONFIG_500KBITS();

    can_filter_config_t f_config;   // = {.acceptance_code = (0x100 << 21),
                                    //    .acceptance_mask = ~(0x7FF << 21),
                                    //    .single_filter = true};
    can_general_config_t g_config;  // = {.mode = CAN_MODE_LISTEN_ONLY,
                                    //    .tx_io = GPIO_NUM_14, .rx_io = GPIO_NUM_15,
                                    //    .clkout_io = CAN_IO_UNUSED, .bus_off_io = CAN_IO_UNUSED,
                                    //    .tx_queue_len = 0, .rx_queue_len = 8,
                                    //    .alerts_enabled = CAN_ALERT_NONE,
                                    //    .clkout_divider = 0};


    // any private methods should go here (non-inline methosd should be defined out of class)
    void ConfigureCan(); 


  public:

    // non WLED related methods, may be used for data exchange between usermods (non-inline methods should be defined out of class)

    /**
     * Enable/Disable the usermod
     */
    inline void enable(bool enable) { enabled = enable; }

    /**
     * Get usermod enabled/disabled state
     */
    inline bool isEnabled() { return enabled; }

    // methods called by WLED (can be inlined as they are called only once but if you call them explicitly define them out of class)

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * readFromConfig() is called prior to setup()
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() 
    { 
      gpio_num_t tmpCtx, tmpCrx;
      tmpCtx = static_cast<gpio_num_t>(ctxPin);
      tmpCrx = static_cast<gpio_num_t>(ctxPin);
      // initialise CAN config variables
      t_config = CAN_TIMING_CONFIG_500KBITS();

      f_config = CAN_FILTER_CONFIG_ACCEPT_ALL();

      g_config = {.mode = CAN_MODE_LISTEN_ONLY,
                  .tx_io = tmpCtx, .rx_io = tmpCrx,
                  .clkout_io = CAN_IO_UNUSED, .bus_off_io = CAN_IO_UNUSED,
                  .tx_queue_len = 0, .rx_queue_len = 8,
                  .alerts_enabled = CAN_ALERT_NONE,
                  .clkout_divider = 0};

      ConfigureCan();

      initDone = true;
    }


    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     * 
     * Tips:
     * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
     * 
     * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
     *    Instead, use a timer check as shown here.
     */
    void loop() {   //every ~ 6ms 
      // if usermod is disabled or called during strip updating just exit !enabled ||
      // NOTE: on very long strips strip.isUpdating() may always return true so update accordingly
      if (!enabled ||  strip.isUpdating()) return;

      can_status_info_t status;
      can_message_t rx_msg;
      esp_err_t error;

      for (uint8_t i = 0; i < 10; i++) 
      {

        error = can_get_status_info(&status);
        if (error) break; // if return can bus error then break

        if (status.msgs_to_rx > 1) {

          error = can_receive(&rx_msg, 1);
          #ifdef WLED_DEBUG
            if (ESP_ERR_TIMEOUT != error)
            {
              DEBUG_PRINT(F("Error: ")); DEBUG_PRINT(error); DEBUG_PRINT(F(" CAN MSG ID:")); DEBUG_PRINTLN(rx_msg.identifier);
            }
          #endif

          // thing
          if (rx_msg.identifier == canId)
          {
            applyPreset(rx_msg.data[0] ,CALL_MODE_DIRECT_CHANGE);
          }
          switch (vehicle)
          {
          case MX5:
            mx5->msgHandler(rx_msg);
            break;
          case GT86: 
            gt86->msgHandler(rx_msg);
            break;
          default:
            break;
          }
        }
      }
    }


    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     * 
     * CAUTION: serializeConfig() will initiate a filesystem write operation.
     * It might cause the LEDs to stutter and will cause flash wear if called too often.
     * Use it sparingly and always in the loop, never in network callbacks!
     * 
     * addToConfig() will make your settings editable through the Usermod Settings page automatically.
     *
     * Usermod Settings Overview:
     * - Numeric values are treated as floats in the browser.
     *   - If the numeric value entered into the browser contains a decimal point, it will be parsed as a C float
     *     before being returned to the Usermod.  The float data type has only 6-7 decimal digits of precision, and
     *     doubles are not supported, numbers will be rounded to the nearest float value when being parsed.
     *     The range accepted by the input field is +/- 1.175494351e-38 to +/- 3.402823466e+38.
     *   - If the numeric value entered into the browser doesn't contain a decimal point, it will be parsed as a
     *     C int32_t (range: -2147483648 to 2147483647) before being returned to the usermod.
     *     Overflows or underflows are truncated to the max/min value for an int32_t, and again truncated to the type
     *     used in the Usermod when reading the value from ArduinoJson.
     * - Pin values can be treated differently from an integer value by using the key name "pin"
     *   - "pin" can contain a single or array of integer values
     *   - On the Usermod Settings page there is simple checking for pin conflicts and warnings for special pins
     *     - Red color indicates a conflict.  Yellow color indicates a pin with a warning (e.g. an input-only pin)
     *   - Tip: use int8_t to store the pin value in the Usermod, so a -1 value (pin not set) can be used
     *
     * See usermod_v2_auto_save.h for an example that saves Flash space by reusing ArduinoJson key name strings
     * 
     * If you need a dedicated settings page with custom layout for your Usermod, that takes a lot more work.  
     * You will have to add the setting to the HTML, xml.cpp and set.cpp manually.
     * See the WLED Soundreactive fork (code and wiki) for reference.  https://github.com/atuline/WLED
     * 
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      //save these vars persistently whenever settings are saved
      top[FPSTR(_ctxPin)]         = ctxPin;
      top[FPSTR(_crxPin)]         = crxPin;
      top["baudrate"] = baudrate;
      top["ID"] = canId;
      top["vehicle"] = vehicle;
    }


    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
     * 
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     * 
     * Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
     * 
     * getJsonValue() returns false if the value is missing, or copies the value into the variable provided and returns true if the value is present
     * The configComplete variable is true only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to save them
     * 
     * This function is guaranteed to be called on boot, but could also be called every time settings are updated
     */
    bool readFromConfig(JsonObject& root)
    {
      JsonObject top = root[FPSTR(_name)];
      int8_t newCtxPin   = ctxPin;
      int8_t newCrxPin   = crxPin;
      vehicle_t newVehicle = vehicle;
      
      if (top.isNull()) {
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      bool configComplete = !top.isNull();

      // A 3-argument getJsonValue() assigns the 3rd argument as a default value if the Json value is missing
      configComplete &= getJsonValue(top["baudrate"], baudrate, 500);  
      configComplete &= getJsonValue(top[_enabled], enabled, false); 

      // "pin" fields have special handling in settings page (or some_pin as well)
      configComplete &= getJsonValue(top[_ctxPin], newCtxPin, -1);
      configComplete &= getJsonValue(top[_crxPin], newCrxPin, -1);

      configComplete &= getJsonValue(top["vehicle"], vehicle, None); 
      configComplete &= getJsonValue(top["can ID"], canId, None); 

      if (!initDone) {
        // first run: reading from cfg.json
        ctxPin = newCtxPin;
        crxPin = newCrxPin;
        DEBUG_PRINTLN(F(" config loaded."));
      } else {
        DEBUG_PRINTLN(F(" config (re)loaded."));
        // changing paramters from settings page
        if (ctxPin != newCtxPin || crxPin != newCrxPin || vehicle != newVehicle) {
          DEBUG_PRINTLN(F("Re-init sig."));
          switch (vehicle)
          {
          case MX5:
            delete mx5;
            break;
          case GT86: 
            delete gt86;
            break;
          default:
            break;
          }
          vehicle = newVehicle;

          DEBUG_PRINTLN(F("Re-init pins."));
          // deallocate pin and release interrupts
          pinManager.deallocatePin(ctxPin, PinOwner::UM_CANBUS);
          pinManager.deallocatePin(crxPin, PinOwner::UM_CANBUS);
          ctxPin = newCtxPin;
          crxPin = newCrxPin;
          ConfigureCan();
          
        }
      }
      
      return configComplete;
    }


    /*
     * appendConfigData() is called when user enters usermod settings page
     * it may add additional metadata for certain entry fields (adding drop down is possible)
     * be careful not to add too much as oappend() buffer is limited to 3k
     */
    void appendConfigData()
    {
      oappend(SET_F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F("','baudrate');"));
      oappend(SET_F("addOption(dd,'25 kBits',2);"));
      oappend(SET_F("addOption(dd,'50 kBits',5);"));
      oappend(SET_F("addOption(dd,'100 kBits',10);"));
      oappend(SET_F("addOption(dd,'125 kBits',12);"));
      oappend(SET_F("addOption(dd,'250 kBits',25);"));
      oappend(SET_F("addOption(dd,'500 kBits',50);"));
      oappend(SET_F("addOption(dd,'800 kBits',80);"));
      oappend(SET_F("addOption(dd,'1 mBits',100);"));

      oappend(SET_F("dd=addDropdown('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F("','vehicle');"));
      oappend(SET_F("addOption(dd,'None',0);"));
      oappend(SET_F("addOption(dd,'MX5',1);"));
      oappend(SET_F("addOption(dd,'GT86',2);"));     
    }


    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    void handleOverlayDraw()
    {
    switch (vehicle)
        {
        case MX5:
          mx5->overlayHandler();
          break;
        case GT86: 
          gt86->overlayHandler();
          break;
        default:
          break;
        }
    }


    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_CAN_BUS;
    }

   //More methods can be added in the future, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};

// implementation of non-inline member methods

void CanBusUsermod::ConfigureCan()
{
  if (canState > 1)
  {
    ESP_ERROR_CHECK(can_stop());
    ESP_ERROR_CHECK(can_driver_uninstall());
  }
  canState = 1;
  DEBUG_PRINT(F("Switching baudrate : "));
  DEBUG_PRINTLN(baudrate);
  
  switch (baudrate)
  {
  case 2:
    t_config = CAN_TIMING_CONFIG_25KBITS();
    break;
  case 5:
    t_config = CAN_TIMING_CONFIG_50KBITS();
    break;
  case 10:
    t_config = CAN_TIMING_CONFIG_100KBITS();
    break;
  case 12:
    t_config = CAN_TIMING_CONFIG_125KBITS();
    break;
  case 25:
    t_config = CAN_TIMING_CONFIG_250KBITS();
    break;
  case 50:
    t_config = CAN_TIMING_CONFIG_500KBITS();
    break;
  case 80:
    t_config = CAN_TIMING_CONFIG_800KBITS();
    break;
  case 100:
    t_config = CAN_TIMING_CONFIG_1MBITS();
    break;
  default:
    canState = -1;
    break;
  }

  switch (vehicle)
  {
  case MX5:
    mx5 = new CanBusMX5;
    canId = mx5->canId;
    f_config = mx5->f_config;
    t_config = mx5->t_config;
    break;
  case GT86: 
    gt86 = new CanBus86;
    canId = gt86->canId;
    f_config = gt86->f_config;
    t_config = gt86->t_config;
    break;
  default:
    f_config  = {.acceptance_code = (canId << 21),
                .acceptance_mask = ~(0x7FF << 21),
                .single_filter = true};
    break;
  }

  if (crxPin > 0 && ctxPin > 0) 
  {
    gpio_num_t tmpPin;

    tmpPin = static_cast<gpio_num_t>(crxPin);
    if (GPIO_IS_VALID_OUTPUT_GPIO(tmpPin)) 
    {
      g_config.rx_io = tmpPin;
      pinManager.allocatePin(crxPin, false, PinOwner::UM_CANBUS);
    } else 
    {
      canState = -1;
    }

    tmpPin = static_cast<gpio_num_t>(ctxPin);
    if (GPIO_IS_VALID_GPIO(tmpPin)) 
    {
      g_config.tx_io = tmpPin;
      pinManager.allocatePin(ctxPin, true, PinOwner::UM_CANBUS);
    } else 
    {
      canState = -1;
    }
  }

  if (canState == 1)
  {
    DEBUG_PRINT(F("Switching CTX : "));
    DEBUG_PRINT(g_config.tx_io);
    DEBUG_PRINT(F(" CRX : "));
    DEBUG_PRINTLN(g_config.rx_io);
    ESP_ERROR_CHECK(can_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(can_start());
    canState = 2;
  }
}



// add more strings here to reduce flash memory usage
const char CanBusUsermod::_name[]    PROGMEM = "Can Bus";
const char CanBusUsermod::_enabled[] PROGMEM = "enabled";
const char CanBusUsermod::_ctxPin[]  PROGMEM = "CTX-pin";
const char CanBusUsermod::_crxPin[]  PROGMEM = "CRX-pin";
