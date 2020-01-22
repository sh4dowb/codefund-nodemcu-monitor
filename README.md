# Codefund.io NodeMCU Monitor

This project uses NodeMCU and SSD1306 OLED screen for codefund.io balance tracking.

![Codefund.io NodeMCU balance monitor](https://i.ibb.co/KXBsqWF/image.png)

**Note**: the request to app.codefund.io uses **HTTP**. See https://github.com/nodemcu/nodemcu-firmware/blob/master/docs/modules/tls.md . "The TLS glue provided by Espressif provides no interface to TLS SNI."


### How to use?

    const char *ssid = "wifi_ssid";
    const char *password = "wifi_password";
    const char *property_id = "123"; // https://app.codefund.io/properties/123
    const char cookie[] PROGMEM = "Cookie: remember_user_token=eyJf.......\r\n";
    // TO FIND THIS COOKIE:
    // Go to app.codefund.io
    // F12 -> Storage -> Cookies -> app.codefund.io
    // Replace remember_user_token value

 - Change lines above for your codefund.io account and Wi-Fi.
 - Install NodeMCU board, Adafruit_GFX and Adafruit_SSD1306 libraries.
 - Flash the code.
 - Connect your SSD1306 OLED screen as: SDA -> D2 and SCL -> D3


### Issues

 - `updateToken()` and `updateDatetime()` has problems when using SSL. `updateToken()`'s issue is about Espressif's TLS library, which apparently does not support SNI. `updateDatetime()` with SSL, for some reason, only works in the setup. 5-minute date updates fail to connect. Currently these both use HTTP.
