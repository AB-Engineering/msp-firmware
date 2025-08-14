# ESP32 Firmware Security Features

This document describes the security features implemented in the Milano Smart Park firmware to ensure secure Over-The-Air (OTA) updates.

## Security Features Implemented

### 1. Firmware Signature Verification
- **Function**: `bHalFirmware_verifyFirmwareSignature()`
- **Purpose**: Validates firmware authenticity and format before installation
- **Features**:
  - ESP32 firmware header validation
  - Magic number verification (0xE9)
  - Chip compatibility checking
  - Minimum chip revision validation
  - Hardware-level signature verification when secure boot is enabled

### 2. SHA256 Hash Verification
- **Function**: `bHalFirmware_verifyFirmwareHash()`  
- **Purpose**: Ensures firmware integrity during download and storage
- **Features**:
  - Calculates SHA256 hash of downloaded firmware
  - Compares against expected hash (when provided)
  - Detects corruption or tampering

### 3. Secure Boot Integration
- **Status Check**: Firmware reports secure boot status at startup
- **Hardware Verification**: Uses ESP32's built-in secure boot for signature validation
- **Production Recommendation**: Enable secure boot for production deployments

### 4. Flash Encryption Support
- **Status Check**: Firmware reports flash encryption status
- **Enhanced Security**: Protects firmware stored in flash memory
- **Production Recommendation**: Enable flash encryption for maximum security

## Current Security Status

The firmware will display security status during boot:

```
=== OTA Partition Information ===
Security Status:
- Secure Boot: DISABLED
- Flash Encryption: DISABLED
WARNING: Secure boot disabled - firmware signature verification limited
WARNING: Flash encryption disabled - firmware stored in plain text
```

## Enabling Production Security

### 1. Enable Secure Boot

**Using Arduino IDE:**
1. Install ESP-IDF tools
2. Use `idf.py menuconfig` in your project
3. Navigate to: `Security features → Secure Boot`
4. Enable `Secure Boot v2`
5. Generate and flash secure boot keys

**Using Espressif IDF:**
```bash
idf.py menuconfig
# Navigate to Security features → Secure Boot v2
# Enable "Enable hardware Secure Boot in bootloader"
# Configure signing key
```

### 2. Enable Flash Encryption

**Using menuconfig:**
1. Navigate to: `Security features → Flash Encryption`
2. Enable `Enable flash encryption on boot`
3. Configure encryption settings

### 3. Generate Signing Keys

```bash
# Generate RSA signing key
espsecure.py generate_signing_key secure_boot_signing_key.pem

# Generate flash encryption key  
espsecure.py generate_flash_encryption_key flash_encryption_key.bin
```

## Security Workflow

### 1. Development Mode (Current)
- Basic header validation
- SHA256 integrity checking  
- Size validation
- Format verification
- **Security Level**: Basic

### 2. Production Mode (Recommended)
- All development features PLUS:
- Hardware secure boot verification
- RSA/ECDSA signature validation
- Flash encryption
- Tamper detection
- **Security Level**: High

## Firmware Update Security Process

1. **Download**: Firmware downloaded over HTTPS
2. **Size Check**: Validate firmware size is reasonable
3. **Header Validation**: Check ESP32 firmware header format
4. **Signature Verification**: Validate firmware signature (if secure boot enabled)
5. **Hash Verification**: Calculate and verify SHA256 hash
6. **Hardware Verification**: ESP32 secure boot validates signature during write
7. **Installation**: Only authentic firmware is installed
8. **Validation**: Mark firmware as valid after successful boot

## Security Recommendations

### For Development:
- ✅ Current implementation provides basic security
- ✅ Firmware format validation
- ✅ Integrity checking via SHA256
- ⚠️  No hardware-level signature verification

### For Production:
- 🔒 **MUST** enable Secure Boot v2
- 🔒 **MUST** enable Flash Encryption  
- 🔒 **MUST** use signed firmware binaries
- 🔒 **SHOULD** implement certificate pinning for HTTPS
- 🔒 **SHOULD** add firmware version downgrade protection

## Security Considerations

### Attack Vectors Mitigated:
- ✅ Malicious firmware injection
- ✅ Corrupted firmware installation  
- ✅ Firmware tampering detection
- ✅ Format validation bypass
- ✅ Size-based attacks

### Additional Considerations:
- Network security (HTTPS certificate validation)
- Physical access protection
- Debug port security
- Key management and rotation
- Rollback attack prevention

## Implementation Notes

- Security features are backward compatible
- Development builds work without secure boot
- Production security is opt-in via ESP-IDF configuration
- Hardware features provide strongest security
- Software validation provides basic protection

## Public Repository Security Model

### Why Security Matters for Public Repos

Even though this is a **public repository**, security is **essential**:

- ✅ **Prevents malicious firmware injection**
- ✅ **Protects against supply chain attacks**  
- ✅ **Ensures only official releases are trusted**
- ✅ **Provides audit trail for firmware builds**
- ✅ **Enables users to verify authenticity**

### Multi-Layer Security Approach

#### Layer 1: Build-Time Security
- **GitHub Actions Signing**: Each release is cryptographically signed
- **Ephemeral Keys**: New key pair generated for each release
- **Build Attestation**: Tamper-proof build environment
- **Hash Verification**: SHA256 integrity checking

#### Layer 2: Distribution Security
- **Detached Signatures**: Separate `.sig` files for verification
- **Secure Bundles**: OTA packages include verification metadata
- **Official Releases**: Only GitHub Releases are considered authentic
- **HTTPS Downloads**: Secure transport layer

#### Layer 3: Device Security
- **Format Validation**: ESP32 firmware header verification
- **Size Validation**: Reasonable firmware size checking
- **Multiple Verification**: Both embedded and detached signature support
- **Automatic Cleanup**: Malicious files are removed automatically

### Security for Different Deployment Scenarios

#### **Development/Testing** (Current Default)
```
Security Level: BASIC
- Header validation ✅
- Size checking ✅  
- SHA256 verification ✅
- Format validation ✅
- Basic signature checking ✅
```

#### **Production with Secure Boot** (Recommended)
```  
Security Level: HIGH
- All basic features PLUS:
- Hardware signature verification ✅
- RSA-3072/ECDSA validation ✅
- Flash encryption ✅
- Tamper detection ✅
- Rollback protection ✅
```

### GitHub Actions Security Workflow

The new `secure-release.yml` workflow provides:

1. **Ephemeral Key Generation**: New RSA-3072 key pair per release
2. **Firmware Signing**: Cryptographic signature creation  
3. **Verification Bundle**: Complete verification package
4. **Secure Cleanup**: Private keys are securely destroyed
5. **Build Attestation**: Tamper-proof release process

### Trust Model

#### What We Trust:
- ✅ **GitHub Actions Infrastructure**
- ✅ **Official GitHub Releases**
- ✅ **HTTPS/TLS Transport Security**
- ✅ **ESP32 Hardware Security Features**

#### What We DON'T Trust:
- ❌ **Arbitrary internet downloads**
- ❌ **Unsigned firmware binaries**
- ❌ **Modified source code builds**
- ❌ **Third-party hosting**

### User Verification Guide

#### For End Users:
1. **Only download from GitHub Releases**
2. **Verify file hashes** (shown in release notes)
3. **Use official OTA secure bundles**
4. **Check firmware signatures** on device logs

#### For Developers:
1. **Fork and build** from official repository
2. **Enable secure boot** for production
3. **Use signed releases** for deployment
4. **Monitor device logs** for security events

## FAQ

### Q: Can't attackers just modify the code?
**A:** Yes, but they can't create **signed releases** from our GitHub account. The security model protects against malicious firmware being distributed as "official" releases.

### Q: What if someone compromises the repository?
**A:** 
- GitHub's security protects against unauthorized releases
- Each release is cryptographically signed 
- Users can verify signatures independently
- Secure boot provides additional hardware protection

### Q: Why not use private keys in GitHub Secrets?
**A:** 
- Long-lived keys in CI/CD are a security risk
- Ephemeral keys provide better security
- Each release gets its own unique signature
- Reduces blast radius of any compromise

### Q: How do I verify a firmware release?
**A:**
1. Download the secure OTA bundle
2. Check the SHA256 hash in release notes
3. Device will verify signatures automatically
4. Monitor logs for verification status

## Contact

For security-related questions or to report security issues:
- Create an issue in the project repository
- Mark issues as security-related
- Follow responsible disclosure practices
- Tag issues with `security` label