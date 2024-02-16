# Bubblemaker [<img align="right" height="50px" src="https://avsystem.github.io/Anjay-doc/_images/avsystem_logo.png">](http://www.avsystem.com/)

## Supported hardware and overview
This folder contains support for the following targets:
 - [nrf9160dk_nrf9160_ns](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/ug_nrf9160.html)
 - [nrf7002dk_nrf5340_cpuapp_ns](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/device_guides/working_with_nrf/nrf70/gs.html)

 The following LwM2M Objects are supported:
 - Security (/0)
 - Server (/1)
 - Device (/3)
 - Connectivity Monitoring (/4)
 - Firmware Update (/5)
 - Sensors:
   - Temperature (/3303) with multiple DS18B20 sensors on single 1Wire line
   - Pressure (/3323)
   - Acidity (/3326)
 - On/Off switch (/3342)
 - Push button (/3347)
 - Water meter (/3424)

The Bubblemaker contains an example smart water meter demo with basic IPSO
sensor support. It is possible to use a single water meter with an electrical or
hand pump as well as two water meters with two hand pumps. When using two water
meters, two players can compete and the winner is indicated through led strip
light.

To exclude sensor in a build, it is enough to comment or delete it's name in
the `aliases` section or it's name and corresponding io-channel in the
`zephyr,user` section in the `boards/<board_name>.overlay` file. For example,
if the final configuration for `nRF9160 DK` should not contain temperature
sensor #1, water pump and pressure sensors, the
`boards/nrf9160dk_nrf9160_ns.overlay` file should look like this:
```
/ {
    aliases {
        push-button-0 = &button0;
        push-button-1 = &button1;
        switch-0 = &button2;
        switch-1 = &button3;
        status-led = &led0;
        temperature-0 = &ds18b200;
        //temperature-1 = &ds18b201;
        led-strip = &led_strip;
        water-meter-0 = &water_meter0;
        water-meter-1 = &water_meter1;
        //water-pump-0 = &water_pump0;
    };
    zephyr,user {
        /* these settings act as aliases for ADC sensors */
        io-channels = <&adc 2>, <&adc 3>;
        io-channel-names = "acidity0", "acidity1";
    };
/* rest of the file */
```
