package com.tfx2001.ble_door_lock;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.NonNull;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

import io.flutter.embedding.android.FlutterActivity;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.plugin.common.EventChannel;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;

public class MainActivity extends FlutterActivity {
    private final static int REQUEST_ENABLE_BT = 1;
    private final static int PERMISSION_REQUEST_FINE_LOCATION = 1;
    private final static int SCAN_PERIOD = 2000;
    private final static byte[] MANUFACTURE_DATA = {(byte) 0xD4, (byte) 0xFF, (byte) 0x69, (byte) 0x38, (byte) 0x64, (byte) 0xE4};
    private final static String METHOD_CHANNEL = "com.tfx2001.ble_door_lock/method";
    private final static String EVENT_CHANNEL = "com.tfx2001.ble_door_lock/event";

    // EventSink
    private EventChannel.EventSink eventSink;
    private Handler handler;
    // Bluetooth LE
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bleScanner;
    private ScanCallbackImpl scanCallback;
    private BluetoothDevice device;
    private BluetoothGattCallbackImpl gattCallback;
    private BluetoothGatt gattClient;
    private BluetoothGattCharacteristic gattCharacteristic;
    private boolean isScanning = false;

    @Override
    public void configureFlutterEngine(@NonNull FlutterEngine flutterEngine) {
        super.configureFlutterEngine(flutterEngine);

        // 创建 MethodChannel
        new MethodChannel(flutterEngine.getDartExecutor().getBinaryMessenger(), METHOD_CHANNEL).setMethodCallHandler(this::onMethodCall);
        // 创建 EventChannel
        EventChannel eventChannel = new EventChannel(flutterEngine.getDartExecutor().getBinaryMessenger(), EVENT_CHANNEL);
        // 设置 handler
        eventChannel.setStreamHandler(new EventChannel.StreamHandler() {
            @Override
            public void onListen(Object o, EventChannel.EventSink eventSink) {
                MainActivity.this.eventSink = eventSink;
            }

            @Override
            public void onCancel(Object o) {
                MainActivity.this.eventSink = null;
            }
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        this.handler = new Handler();
        // 蓝牙适配器
        this.bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        // BLE 扫描器
        this.bleScanner = bluetoothAdapter.getBluetoothLeScanner();
        // callback 对象
        this.scanCallback = new ScanCallbackImpl();
        this.gattCallback = new BluetoothGattCallbackImpl();
    }

    /**
     * 开始扫描
     */
    public void startScanning() {
        // filter 列表
        List<ScanFilter> filters = new ArrayList<>();
        filters.add(new ScanFilter.Builder().setManufacturerData(0x116B, MANUFACTURE_DATA).build());

        // 确保蓝牙已开启，如果没有，则申请启动蓝牙
        if (this.bluetoothAdapter == null || !this.bluetoothAdapter.isEnabled()) {
            // 申请启动蓝牙
            Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
        }
        // 检查位置权限 (必须要 FINE_LOCATION 权限，COARSE_LOCATION 权限不行)
        else if (Build.VERSION.SDK_INT >= 23 && this.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, PERMISSION_REQUEST_FINE_LOCATION);
        } else if (this.isScanning) {
            Toast.makeText(this, "扫描进行中...", Toast.LENGTH_SHORT).show();
        } else {
            // 2s 后停止扫描
            this.handler.postDelayed(() -> {
                JSONObject jsonObject = new JSONObject();

                if (this.isScanning) {
                    this.bleScanner.stopScan(this.scanCallback);
                    this.isScanning = false;
                    try {
                        jsonObject.put("event", Events.EVENT_SCAN_RESULT);
                        jsonObject.put("value", null);
                    } catch (JSONException e) {
                        e.printStackTrace();
                    }
                }
                this.eventSink.success(jsonObject.toString());
            }, SCAN_PERIOD);
            // 开始扫描，并设置 callback
            this.bleScanner.startScan(filters, new ScanSettings.Builder()
                    .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                    .setCallbackType(ScanSettings.CALLBACK_TYPE_FIRST_MATCH)
                    .build(), this.scanCallback);
            this.isScanning = true;
        }
    }

    /**
     * onActivityResult 回调函数
     *
     * @param requestCode 请求代码
     * @param resultCode  结果代码
     * @param data        返回数据
     */
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_ENABLE_BT) {
            if (resultCode == RESULT_OK) {
                if (this.bluetoothAdapter == null) {
                    this.bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
                }
                // 重启扫描
                this.startScanning();
            } else {
                Toast.makeText(this, "打开蓝牙失败！", Toast.LENGTH_SHORT).show();
            }
        }
    }

    /**
     * onRequestPermissionsResult 回调函数
     *
     * @param requestCode  请求代码
     * @param permissions  权限
     * @param grantResults 结果
     */
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == PERMISSION_REQUEST_FINE_LOCATION) {
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                this.startScanning();
            } else {
                Toast.makeText(this, "请求位置权限失败！", Toast.LENGTH_SHORT).show();
            }
        }
    }

    private void onMethodCall(MethodCall call, MethodChannel.Result result) {
        switch (call.method) {
            case "startScan":
                // 开始扫描
                this.startScanning();
                result.success(null);
                break;
            case "showToast":
                // 调用原生 Toast
                Toast.makeText(this, call.argument("msg"), Toast.LENGTH_SHORT).show();
                result.success(null);
                break;
            case "connectGatt":
                // 连接至 GATT 服务器
                this.gattClient = device.connectGatt(this, false, gattCallback);
                result.success(null);
                break;
            case "writeCharacteristic":
                // 写入特性
                // 不设置值无法写入
                this.gattCharacteristic.setValue(0x01, BluetoothGattCharacteristic.FORMAT_UINT8, 0x00);
                this.gattClient.writeCharacteristic(this.gattCharacteristic);
                result.success(null);
                break;
            case "disconnectGatt":
                if (this.gattClient != null && ((BluetoothManager) getActivity().getSystemService(Context.BLUETOOTH_SERVICE))
                        .getConnectionState(this.device, BluetoothProfile.GATT) == BluetoothGatt.STATE_CONNECTED) {
                    this.gattClient.disconnect();
                }
                result.success(null);
                break;
            default:
                result.notImplemented();
                break;
        }
    }

    enum Events {
        EVENT_SCAN_RESULT,
        EVENT_CONNECTED,
        EVENT_DISCONNECTED,
    }

    /**
     * Override ScanCallback
     */
    class ScanCallbackImpl extends ScanCallback {
        private final static String TAG = "ScanCallback";

        /**
         * 扫描到设备
         *
         * @param callbackType 回调类型
         * @param result       扫描结果
         */
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            JSONObject jsonObject = new JSONObject();

            super.onScanResult(callbackType, result);
            MainActivity.this.bleScanner.flushPendingScanResults(MainActivity.this.scanCallback);
            MainActivity.this.bleScanner.stopScan(MainActivity.this.scanCallback);
            MainActivity.this.device = result.getDevice();
            MainActivity.this.isScanning = false;
            try {
                jsonObject.put("event", Events.EVENT_SCAN_RESULT);
                jsonObject.put("value", MainActivity.this.device.getAddress());
            } catch (JSONException e) {
                e.printStackTrace();
            }
            MainActivity.this.eventSink.success(jsonObject.toString());
        }

        @Override
        public void onBatchScanResults(List<ScanResult> results) {
            super.onBatchScanResults(results);
            Log.d(TAG, String.format("onBatchScanResults: %s", results));
        }

        @Override
        public void onScanFailed(int errorCode) {
            super.onScanFailed(errorCode);
            Log.e(TAG, String.format("onScanFailed: %d", errorCode));
        }
    }

    /**
     * GATT 回调函数
     */
    class BluetoothGattCallbackImpl extends BluetoothGattCallback {
        /**
         * 连接状态改变
         *
         * @param gatt     GATT 客户端
         * @param status   状态
         * @param newState 当前连接情况
         */
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            JSONObject jsonObject = new JSONObject();

            super.onConnectionStateChange(gatt, status, newState);
            if (newState == BluetoothGatt.STATE_CONNECTED) {
                handler.postDelayed(() -> {
                    gattClient.discoverServices();
                }, 600);
            } else if (newState == BluetoothGatt.STATE_DISCONNECTED) {
                // 连接断开
                try {
                    jsonObject.put("event", Events.EVENT_DISCONNECTED);
                    jsonObject.put("value", null);
                } catch (JSONException e) {
                    e.printStackTrace();
                }
            }
            handler.post(() -> MainActivity.this.eventSink.success(jsonObject.toString()));
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            final UUID serviceUUID = UUID.fromString("00f02981-52a6-8eec-7b56-ef7bfab3f5c1");
            final UUID characteristicUUID = UUID.fromString("01f02981-52a6-8eec-7b56-ef7bfab3f5c1");
            JSONObject jsonObject = new JSONObject();

            super.onServicesDiscovered(gatt, status);
            Log.d("GattCallback", "onServicesDiscovered: service discovered");
            BluetoothGattService gattService = gattClient.getService(serviceUUID);
            MainActivity.this.gattCharacteristic = gattService.getCharacteristic(characteristicUUID);
            try {
                jsonObject.put("event", Events.EVENT_CONNECTED);
                jsonObject.put("value", null);
            } catch (JSONException e) {
                e.printStackTrace();
            }
            MainActivity.this.handler.post(() -> MainActivity.this.eventSink.success(jsonObject.toString()));
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
            super.onCharacteristicChanged(gatt, characteristic);
        }
    }
}
