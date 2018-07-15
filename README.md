# _NetGate_  

_This is a project which show how to connect esp32 and Qt by ethernet and bluetooth._  
* Please first read the [docs](https://esp-idf.readthedocs.io) about [esp32](https://www.espressif.com/en/products/hardware/esp32/overview) and [esp-idf](https://github.com/espressif/esp-idf)  
* The project use [arduino-esp32](https://github.com/espressif/arduino-esp32) as a submodule that I can use lost of C++ libraries.  
* So easy to use Because there are many kinds of powerfull examples in arduino libraries.  
* Download a Bluetooth serial APP in your phone.  



## How to use  

### Hardware Required

* An Ethernet development board with LAN872x.  

* Connect to the network router.  

### Configure the project  

* Get project    
  ```  
  git clone --recursive https://github.com/xiongyumail/NetGate.git  
  cd NetGate
  git submodule update
  ```  

* Get esp-idf and toolchain  
  Please look at the official esp-idf Handbook first.  
  <https://esp-idf.readthedocs.io/en/latest/get-started/index.html>  

* menuconfig  
  ```  
  cd esp32
  make menuconfig
  ```  
  * Set serial port under Serial Flasher Options.

  * There are seven options here that must be modified for this project  s 
    1) Arduino Configuration -> Select: Autostart Arduino setup and loop on boot  
    2) Compiler options -> Select: Enable C++ exceptions  
    3) Component config/Bluetooth -> Select: Bluetooth   
    4) Component config/Bluetooth/Bluedroid Enable -> Select: SPP
    5) Component config/ESP32-specific/Main XTAL frequency -> Select: Autodetect  
    6) Component config/Ethernet/Number of DMA RX buffers -> Modify: 5  
    7) Component config/Ethernet/Number of DMA TX buffers -> Modify: 5  

* Install Qt and build
  ```  
  sudo apt-get install g++
  sudo apt-get install qt5-default qtcreator
  ``` 
  Open NetGate project `qt` and build.  

* Open NetGate TcpSever   
  ```
  ./build-NetGate-Desktop-Debug/NetGate
  ```  
  Slect your port allow board to connect.(You can modify your ip and port in `esp32/main.cpp`)

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
make clean
make -j4 flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.  

## More  

### Need more help?
* _We have a QQ ESP32 discussion group, you can join us to get more information._
* QQ Group: 428301646  

### Contributor  
* XiongYu  
  * mail: xiongyu@espressif.com  
  * WeChat: xiongyumail  
* KangZuoLing: 
  * Twitter: kangkang2congK


