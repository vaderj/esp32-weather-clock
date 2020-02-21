# esp32-weather-clock
ESP32 based clock that also has a text marquee showing local weather conditions.  In my particular setup, I am using two 32x16 LED panels originally sourced from Pixel Purses: https://www.amazon.com/Project-Mc2-545170-Pixel-Purse/dp/B06XT2LQPL

Much of the sketch is based on various sketchs by https://github.com/witnessmenow

The openweathermap query occurs every two minutes as a FreeRTOS task not blocking loop()

https://imgur.com/a/QXXN7yN
