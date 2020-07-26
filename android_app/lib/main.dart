import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

enum Events {
  EVENT_SCAN_RESULT,
  EVENT_CONNECTED,
  EVENT_DISCONNECTED,
}

void main() {
  runApp(MyApp());
  SystemUiOverlayStyle systemUiOverlayStyle =
      SystemUiOverlayStyle(statusBarColor: Colors.transparent);
  SystemChrome.setSystemUIOverlayStyle(systemUiOverlayStyle);
}

class MyApp extends StatelessWidget {
  // root widget of application
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BLE Door Lock',
      theme: ThemeData(
        primarySwatch: Colors.blue,
        accentColor: Colors.pinkAccent,
        visualDensity: VisualDensity.adaptivePlatformDensity,
      ),
      home: MainWidget(),
    );
  }
}

class MainWidget extends StatefulWidget {
  MainWidget({Key key}) : super(key: key);

  @override
  _MainWidgetState createState() => _MainWidgetState();
}

class _MainWidgetState extends State<MainWidget> {
  static const String methodChannelName = 'com.tfx2001.ble_door_lock/method';
  static const String eventChannelName = 'com.tfx2001.ble_door_lock/event';

  static const MethodChannel _methodChannel =
      const MethodChannel(methodChannelName);
  static const EventChannel _eventChannel =
      const EventChannel(eventChannelName);

  bool isConnected = false;
  DateTime _lastPressedAt;

  @override
  void initState() {
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    // 注册 EventChannel 监听器
    _eventChannel.receiveBroadcastStream().listen(this.eventListener);

    return new WillPopScope(
        onWillPop: () async {
          if (_lastPressedAt == null ||
              DateTime.now().difference(_lastPressedAt) >
                  Duration(seconds: 2)) {
            /* 2s 后重新计时 */
            _lastPressedAt = DateTime.now();
            _methodChannel.invokeMethod("showToast", {'msg': '再按一次退出'});
            return false;
          }
          await _methodChannel.invokeMethod("disconnectGatt");
          return true;
        },
        child: Scaffold(
            appBar: AppBar(
              title: Text('BLE 门锁'),
            ),
            body: Center(
              child: AnimatedSwitcher(
                duration: const Duration(milliseconds: 300),
                switchInCurve: Curves.easeOutBack,
                switchOutCurve: Curves.easeIn,
                transitionBuilder:
                    (Widget child, Animation<double> animation) =>
                        ScaleTransition(child: child, scale: animation),
                child: MaterialButton(
                  onPressed: () {
                    if (this.isConnected) {
                      _methodChannel.invokeMethod('writeCharacteristic');
//                      _methodChannel.invokeMethod('disconnectGatt');
                    } else {
                      _methodChannel.invokeMethod('startScan');
                    }
                  },
                  color: Colors.blue,
                  textColor: Colors.white,
                  child: Icon(
                    this.isConnected ? Icons.lock_open : Icons.bluetooth_searching,
                    size: 64,
                  ),
                  padding: EdgeInsets.all(24),
                  shape: CircleBorder(),
                  key: ValueKey<bool>(this.isConnected),
                ),
              ),
            )));
  }

  void eventListener(event) {
    print(event);
    var eventJson = json.decode(event);
    switch (eventJson["event"]) {
      case "EVENT_SCAN_RESULT":
        if (eventJson["value"] != null) {
          _methodChannel.invokeMethod('connectGatt');
        } else {
          _methodChannel.invokeMethod("showToast", {'msg': '没有扫描到设备！'});
        }
        break;
      case "EVENT_CONNECTED":
        _methodChannel.invokeMethod("showToast", {'msg': '连接成功'});
        setState(() {
          this.isConnected = true;
        });
        break;
      case "EVENT_DISCONNECTED":
        setState(() {
          this.isConnected = false;
        });
        break;
    }
  }
}
