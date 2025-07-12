# EMG Bluetooth Communication Project 💪📡

This project transmits EMG (Electromyography) signals wirelessly using two Arduino Nano 33 BLE boards over Bluetooth Low Energy (BLE).

## 🔧 Files
- `sender.ino`: Reads EMG signals from analog pins and transmits over BLE.
- `receiver.ino`: Receives EMG data and prints it to the Serial Monitor.

## 🚀 How to Use
1. Connect EMG signal outputs to A0 and A1 on the sender board.
2. Upload `sender.ino` to one Arduino Nano 33 BLE.
3. Upload `receiver.ino` to the second board.
4. Open Serial Monitor on the receiver’s port to view live EMG data.

## 📦 Requirements
- 2x Arduino Nano 33 BLE
- EMG sensor
- Arduino IDE

