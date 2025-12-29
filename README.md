# How to build

```bash
git clone https://github.com/uncor3/idevice-rs-bug.git --recursive
cd idevice-rs-test
cmake -B build
cmake --build build
```

# Run

```bash
./build/idevice-rs-test <device_ip> <pairing_file_path> [--wait]
```

# Bug is reproducible when `--wait` is provided. Otherwise, everything works fine.

## When `--wait` is NOT provided:

```bash
 ./build/idevice-rs-test 192.168.1.146 /var/lib/lockdown/a5c08c1dfdc9fcf81366bd6159c81bba73deaa27.plist
Heartbeat client created successfully
Heartbeat thread started
Received marco, new interval: 10
Sent polo successfully
Contents of /:
  .
  ..
  Podcasts
  Downloads
  CloudAssets
  Books
  Photos
  EnhancedAudioSharedKeys
  Recordings
  PhotoStreamsData
  Radio
  DCIM
  general_storage
  iTunes_Control
  MediaAnalysis
  PhotoData
  PublicStaging
  Purchases
  AirFair
```

## When `--wait` is provided:

```bash
./build/idevice-rs-test 192.168.1.146 /var/lib/lockdown/a5c08c1dfdc9fcf81366bd6159c81bba73deaa27.plist --wait
Heartbeat client created successfully
Heartbeat thread started
Received marco, new interval: 10
Sent polo successfully
Sleeping for 10 seconds
Contents of /:
  (empty)
```

## Waiting for 15 seconds to receive another heartbeat... still the same output:

```bash

./build/idevice-rs-test 192.168.1.146 /var/lib/lockdown/a5c08c1dfdc9fcf81366bd6159c81bba73deaa27.plist --wait
Heartbeat client created successfully
Heartbeat thread started
Received marco, new interval: 10
Sent polo successfully
Sleeping for 15 seconds
Received marco, new interval: 10
Sent polo successfully
Contents of /:
  (empty)

```
