# Automatic Firmware Update System

## Overview

This firmware update system allows the device to automatically check for new firmware releases on GitHub daily at 00:00:00 and perform over-the-air (OTA) updates when enabled.

## Configuration

### SD Card Configuration

Add the following line to your `config_v3.txt` file on the SD card:

```
#fwAutoUpgrade=true;
```

Set to `true` to enable automatic firmware updates, or `false` to disable them.

### Default Behavior

- **Default**: `false` (disabled)
- **When enabled**: Daily check at 00:00:00
- **GitHub Repository**: https://github.com/A-A-Milano-Smart-Park/msp-firmware/releases

## How It Works

1. **Daily Timer**: Every day at 00:00:00, if `fwAutoUpgrade=true`, the system checks for updates
2. **Version Check**: Compares current firmware version with latest GitHub release
3. **Download**: Downloads the firmware ZIP file if a newer version is available
4. **Extract**: Extracts the firmware.bin file from the ZIP
5. **OTA Update**: Performs over-the-air update and restarts the device

## Version Comparison

The system uses semantic versioning (SemVer) format:
- Format: `v1.2.3` or `1.2.3`
- Comparison: Major.Minor.Patch
- Special case: `DEV` version is always considered older than any release

## Testing

### Compile-Time Testing

Add `-DENABLE_FIRMWARE_UPDATE_TESTS` to your build flags to enable testing during startup.

### Manual Testing Functions

```cpp
// Test version comparison logic
vHalFirmware_testVersionComparison();

// Test GitHub API connectivity (requires internet)
vHalFirmware_testGitHubAPI();

// Test configuration parsing
vHalFirmware_testConfigParsing(&sysStatus);
```

## Safety Features

1. **Network Requirement**: Updates only occur when internet connectivity is available
2. **Version Validation**: Only installs newer versions based on semantic versioning
3. **File Validation**: Verifies firmware file exists before attempting update
4. **Rollback Protection**: Standard ESP32 OTA rollback mechanisms apply

## Troubleshooting

### Common Issues

1. **No Update Check Occurring**
   - Verify `fwAutoUpgrade=true` in config_v3.txt
   - Check that device has internet connectivity at 00:00:00
   - Review logs for configuration parsing errors

2. **GitHub API Errors**
   - Check internet connectivity
   - Verify GitHub repository URL is accessible
   - Check for API rate limiting (rare for this usage pattern)

3. **Download Failures**
   - Ensure sufficient SD card space
   - Verify network stability during download
   - Check download timeout settings (currently 60 seconds)

4. **OTA Update Failures**
   - Verify firmware file integrity
   - Check available flash memory
   - Review OTA partition configuration

### Log Messages

Monitor serial output for these key messages:

```
[INFO] Daily firmware update check triggered
[INFO] Current version: v1.0.0
[INFO] Latest version: v1.0.1
[INFO] New firmware version available, starting download...
[INFO] OTA update completed successfully, restarting...
```

## Development Notes

### File Structure

- `firmware_update.h` - Header file with function declarations
- `firmware_update.cpp` - Main implementation
- Integration in `msp-firmware.ino` - Daily timer logic

### Dependencies

- ArduinoJson library for GitHub API response parsing
- HTTPClient for downloading files
- Update library for OTA functionality
- SD library for file operations

### GitHub Release Requirements

For the automatic update to work, GitHub releases must:
1. Have semantic version tags (e.g., `v1.0.0`)
2. Include firmware ZIP files in the release assets
3. The ZIP files should contain a `firmware.bin` file

### Asset Selection Priority

The system will automatically select the most suitable firmware package:
1. **First preference**: `msp-firmware-vX.X.X-macos.zip` (typically smallest)
2. **Fallback**: Any other `msp-firmware-vX.X.X-*.zip` file
3. **Ignore**: Source code archives and non-firmware files

Example release assets:
- ✅ `msp-firmware-v1.0.0-macos.zip` (preferred - typically smallest)
- ✅ `msp-firmware-v1.0.0-win64.zip` (fallback)
- ❌ `Source code (zip)` (ignored)
- ❌ `Source code (tar.gz)` (ignored)

### Security Considerations

- GitHub API requests use HTTPS
- No authentication tokens stored (uses public API)
- OTA updates follow standard ESP32 security practices
- Firmware validation occurs before installation

## Future Enhancements

Potential improvements for production use:

1. **Cryptographic Verification**: Add signature verification for firmware files
2. **Incremental Updates**: Support delta/incremental updates to reduce download size
3. **Update Scheduling**: Allow custom update schedules beyond daily at midnight
4. **Rollback Mechanism**: Automatic rollback if new firmware fails health checks
5. **ZIP Library**: Full ZIP extraction support instead of simplified implementation
6. **Progress Indication**: Display update progress on device screen
7. **Network Retry Logic**: Better handling of temporary network failures during updates

## Configuration Example

Complete `config_v3.txt` example with firmware update enabled:

```
#ssid=YourWiFiSSID;
#password=YourWiFiPassword;
#device_id=device001;
#wifi_power=17dBm;
#o3_zero_value=0;
#average_measurements=5;
#average_delay(seconds)=300;
#sea_level_altitude=122.0;
#upload_server=milanosmartpark.info;
#mics_calibration_values=RED:100,OX:100,NH3:100;
#mics_measurements_offsets=RED:0,OX:0,NH3:0;
#compensation_factors=compH:0.0,compT:0.000,compP:0.0000;
#use_modem=false;
#modem_apn=;
#ntp_server=pool.ntp.org;
#timezone=CET-1CEST;
#fwAutoUpgrade=true;
```