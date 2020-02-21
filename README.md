# esp32-weather-clock
ESP32 based clock that also has a text marquee showing local weather conditions.  In my particular setup, I am using two 32x16 LED panels originally sourced from Pixel Purses: https://www.amazon.com/Project-Mc2-545170-Pixel-Purse/dp/B06XT2LQPL

I am using an ESP32 board to controll the panels, using the PXMatrix library: 
https://www.tindie.com/products/brianlough/esp32-matrix-shield-mini-32/
https://github.com/2dom/PxMatrix

Much of the sketch is based on various sketchs by https://github.com/witnessmenow
Time sync is done through https://github.com/ropg/ezTime

The openweathermap query occurs every two minutes as a FreeRTOS task not blocking loop()

https://imgur.com/a/QXXN7yN
