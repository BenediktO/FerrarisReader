# Ferraris Reader

This repo contains the files of the FerrarisReader project - A tool to read back power consumption of an old-school Ferraris meter.

## Physical setup

The sensor consists of an IR LED and a matching IR fototransistor or fotodiode, mounted such that the red marking on the turning wheel reduce the transmitted light effectively.
A holding structure is provided in `Objects/FerrarisHolder.stl` ready to slice.
It is designed to hold a 5mm LED and a 3mm fototransistor.
Edit `Objects/FerrarisHolder.scad` to fit the design to your need and use [openscad](https://openscad.org/) to convert to a slice-able file.
It is designed such that the point where the reflection occurs in a distance of 9mm. This can be changed by modifiying the distance between LED and sensor.
The schematic is shown below:

![schematic](doc/schematic.png)

You only need the LED, sensor pair, a few resistors and a transistor (which is not really needed) together with an Arduino.
The Arduino needs to have a Serial interface, native or not or via FTDI does not really matter. You might, however need to change the Serial Port.
One pin of the arduino is reserved for the IR LED(s), in case you have more than one meter you want to read out, simply connect more IR LEDs in serial or parallel to the transistor. If you connect them in series, you need to lower the resistor value.
If you use a fotodiode instead of a fototransistor, you might need to increase the resistor to account for the reduced current.

Put the LED and the sensor into the 3D printed holder and connect wires to each of the pins.
On the other end of the wires create the above-shown circuit, test the LED polarity using a multimeter in diode-mode and the photo sensor in resistor mode.

In case you want some visual feedback, connect an LED to a spare pin using an appropriate resistor.
You can also use the builtin LED (13).

## Arduino sketch

You might need to modify the Arduino sketch.

- Specify the IR LED pin, there is only one even though many sensors are connected.
- Specify the number of sensors
- Specify their pin numbers. Important: All those have to be analog pins (`A0`, `A1`, ...)
- Optional: Specify the pins of the activity LED

## Experimenting with the settings

Connect the sensor(s) to the meter by using a bit of tape. Make sure that the sensor is properly aligned.
This can be done by enabling `ALIGNMENT_MODE` in the Arduino Sketch and observing the results in the serial monitor or serial plotter.
The dips in reflectivity should be as clearly visible as possible.

## SCPI Interface

The device follows the SCPI standard, a well-established text-based protocol to communicate with laboratory equipment.

The following commands are supported:

- `*IDN?`: returns the identification string
- `*RST`: resets the device
- `MEASure`
  - `:TICKs`
    - `?`: returns the number of ticks received
    - `:SET [CHANNEL = 0,]<VALUE>`: used to preset the tick counter
    - `:PERiod? [CHANNEL = 0]`: returns the revolution time of the last turn
    - `:LAST? [CHANNEL = 0]`: returns the time since the last tick
  - `:SENSor`
    - `:DARK? [CHANNEL = 0]`: returns the dark current (IR LED off)
    - `:ACTIve? [CHANNEL = 0]`: returns the active current (IR LED on)
    - `:EFFEctive? [CHANNEL = 0]`: returns the difference active - dark current
    - `:SNR? [CHANNEL = 0]`: returns a quantity from 0 - 1, representing the quality, 1 is best.

- `CALCulate`
  - `:ENERgy? [CHANNEL = 0]`: returns the calculated energy
  - `:POWer? [CHANNEL = 0]`: returns the calculated power, which is the average power of the last revolution

- `CONFigure`
  - `:SENSor`
    - `:THREshold`
      - `:UPPer [CHANNEL = 0,]<THRESHOLD>`: sets the upper threshold of a channel
      - `:UPPer? [CHANNEL = 0]`: returns the upper threshold of a channel
      - `:LOWer [CHANNEL = 0,]<THRESHOLD>`: sets the lower threshold of a channel
      - `:LOWer? [CHANNEL = 0]`: returns the lower threshold of a channel
    - `:DURation [CHANNEL = 0,]<DURATION>`: sets the minimum duration of the pulse
    - `:DURation? [CHANNEL = 0]`: returns the minimum duration of the pulse

- `SYSTem`
  - `:CHANnels?`: returns the number of channels of the device
  - `:FREQuency?`: returns the sampling frequency of the last second

Explanation:

This tree represents all possible commands. To assemble a command, join the strings on the branches.
I.e. to set the upper threshold of sensor 1 to 200, use:

`CONFigure:SENSor:THREshold:UPPer 1,200`

The upper case parts of the tokens are required, the lower case part can be omitted.
Note that the command is case-insensitive.
The following commands are equivalent:

```SCPI
CONFigure:SENSor:THREshold:UPPer 1,200
CONF:SENS:THRE:UPP 1,200
conf:sens:thre:upp 1,200
```

Values in brackets `[]` are optional, the default for `CHANNEL` is always 0.
Values in `<>` are required. Parameters are separated by `,`, commands can be joined using `;`.
Queries which return an answer are terminated with a `?`.

## Python scripts

### logger.py

To read back the data from a PC or raspberry, use the provided python script.
It needs the additional package `pyserial`, which can be installed using pip.
Use `python3 logger.py --help` for a reference how to use the program.

### plot.py

To plot the captured data, the script `analyze/plot.py` can be used.
It needs the additional packages `numpy` and `matplotlib`.
Use `python3 plot.py --help` for a reference how to use the program.
