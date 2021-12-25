# Brewery_automation
Control unit for home brewery based on Arduino Mega with 3.2 TFT screen and touch control.

**Sensors:** temperature DS1820,  reed switch liquid level censors.

**Pereferial devises:** heater connected with solid state relay, pump connected with relay, buzzer.

IMPORTANT NOTE !
By default UTFT_Buttons.h library support 20 buttons. For proper work please open UTFT_Buttons.h file and change string " #define MAX_BUTTONS	20 " to " #define MAX_BUTTONS	26
