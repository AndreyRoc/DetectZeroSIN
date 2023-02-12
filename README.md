| Supported Targets | ESP32 |
| ----------------- | ----- |

# Example: GPIO

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This test code shows how to configure GPIO and how to use it with interruption.

## GPIO functions:

| GPIO                         | Direction | Configuration                                          |
| ---------------------------- | --------- | ------------------------------------------------------ |
| CONFIG_GPIO_OUTPUT_0         | output    |                                                        |
| CONFIG_GPIO_INPUT_0          | input     | pulled up, interrupt from rising edge                  |

## Test:
 1. Connect CONFIG_GPIO_INPUT_0 with Generator pulses 50/60 Hz
 2. Generate pulses on CONFIG_GPIO_OUTPUT_0, which are called on the rising edge of CONFIG_GPIO_INPUT_0.
 
## How to use example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.



## Example Output

As you run the example, you will see the following log:

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (299) gpio: GPIO[18]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (309) gpio: GPIO[4]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:1 
I (319) example: Create timer handle
I (319) example: Start timer, auto-reload at alarm event
W (2329) example: Missed one count event
Minimum free heap size: 295028 bytes
Hz: 0
Hz: 0
Hz: 59
Hz: 0
Hz: 60
Hz: 0
Hz: 60
Hz: 0
Hz: 60
Hz: 0
Hz: 60
...
```
