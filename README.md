# Voice Sound Machine
This project is based on the WWE example from the ESP-ADF, found [here](https://github.com/espressif/esp-adf/tree/master/examples/speech_recognition/wwe). 
## ESP WWE Example
The WWE example runs two ML models: one the Espressif Wakenet model that listens for an activation word, and the speech recognition model that listens after the wake word has been used, interpretting speech into text and monitoring for recognized commands.
## This Project
This project looks to extend the WWE example by linking the configurable commands to timers, as well as white noise audio playback