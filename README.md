# BLE 门锁

项目采用`ESP32`作为舵机控制器，通过`BLE（蓝牙低功耗）`接收手机的开门请求，以实现无线门锁。

App 采用`Flutter`框架开发，BLE 部分自行用 Java 编写 API，通过`MethodChannel`和`EventChannel`与`Flutter`通信。

# License

GNU General Public License v3.0