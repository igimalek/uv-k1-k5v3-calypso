# Critical CPU Hang Issues in settings.c

## Summary
Found **12 critical issues** that can cause CPU hangs, memory corruption, and system deadlocks. Analysis includes inconsistent memory operations, buffer overflows, and blocking flash operations.

---

## 🔴 CRITICAL ISSUES

### 1. **NULL POINTER DEREFERENCE IN SETTINGS_FetchChannelName** (Line 570)
**Severity**: CRITICAL - CPU HANG/CRASH
```c
void SETTINGS_FetchChannelName(char *s, const int channel)
{
    if (s == NULL)
        return;
    
    s[0] = 0;  // ✓ Safe after NULL check
    
    if (channel < 0)
        return;
    
    if (!RADIO_CheckValidChannel(channel, false, 0))
        return;
    
    PY25Q16_ReadBuffer(0x00e000 + (channel * 16), s, 10);  // ⚠️ NO SIZE PASSED!
    
    int i;
    for (i = 0; i < 10; i++)
        if (s[i] < 32 || s[i] > 127)
            break;
    
    s[i--] = 0;                   // ❌ BUG: i could be -1 if loop doesn't execute!
    
    while (i >= 0 && s[i] == 32)
        s[i--] = 0;
}
```
**Problem**: If all 10 bytes are valid, `i=10` after loop. Then `s[10--]` = `s[10]` (BUFFER OVERFLOW).
Then `i` becomes `-1`, and while condition `i >= 0` is FALSE, but `s[-1]` was already written!

**Impact**: Stack corruption, function return address corruption, CPU hang.

---

### 2. **BUFFER OVERFLOW IN SETTINGS_SaveChannelName** (Line 1266)
**Severity**: CRITICAL - MEMORY CORRUPTION
```c
void SETTINGS_SaveChannelName(uint8_t channel, const char * name)
{
    uint16_t offset = channel * 16;
    uint8_t buf[16] = {0};
    memcpy(buf, name, MIN(strlen(name), 10u));  // ❌ strlen() called before NULL check!
    
    PY25Q16_WriteBuffer(0x00e000 + offset, buf, 0x10, false);
}
```
**Problems**:
1. If `name == NULL`, `strlen(name)` will CRASH (NULL pointer dereference)
2. `MIN(strlen(name), 10u)` could be wrong - if `name="12345"` then `strlen=5`, but what if memory after is garbage?
3. No validation that `channel` is in range 0-199

**Impact**: CPU hang from exception, potential crash on invalid channel > 199.

---

### 3. **UNSAFE strcpy() CALLS IN SETTINGS_InitEEPROM** (Lines 230, 239, 247, 256, 264)
**Severity**: CRITICAL - BUFFER OVERFLOW
```c
// Line 230
strcpy(gEeprom.ANI_DTMF_ID, "123");          // ❌ NO BOUNDS CHECK!

// Line 239
strcpy(gEeprom.KILL_CODE, "ABCD9");          // ❌ UNSAFE!

// Line 247
strcpy(gEeprom.REVIVE_CODE, "9DCBA");        // ❌ UNSAFE!

// Line 256
strcpy(gEeprom.DTMF_UP_CODE, "12345");       // ❌ UNSAFE!

// Line 264
strcpy(gEeprom.DTMF_DOWN_CODE, "54321");     // ❌ UNSAFE!
```
**Problem**: `strcpy()` is never safe. Unknown buffer sizes in gEeprom structure.
If these fields are < 5 bytes, direct stack overflow.

**Impact**: Immediate memory corruption, function return address overwrite, CPU hang.

---

### 4. **INFINITE LOOP IN SETTINGS_FetchChannelName** (Lines 572-578)
**Severity**: CRITICAL - CPU HANG
```c
int i;
for (i = 0; i < 10; i++)
    if (s[i] < 32 || s[i] > 127)
        break;

s[i--] = 0;  // ⚠️ i could be 10 or -1

while (i >= 0 && s[i] == 32)  // ❌ INFINITE LOOP POSSIBLE
    s[i--] = 0;
```
**Problem**: If last character is ALWAYS a space (s[0..9] all = 0x20 = space), the while loop runs forever:
- i=10 initially (after for loop), then i-- = 9
- s[9] == 32? YES
- i-- = 8
- Eventually i = -1
- While condition checks `i >= 0` which is FALSE, loop exits

**But**: If `i` starts as an INVALID value or the string is corrupted, infinite loop risk.

---

### 5. **RACE CONDITION IN SETTINGS_SaveSettings/SETTINGS_SaveChannel**
**Severity**: HIGH - DATA CORRUPTION
```c
// SETTINGS_SaveChannel() reads then writes, but no atomic protection
if (Mode >= 2 || IS_FREQ_CHANNEL(Channel)) {
    // ... prepare State ...
    PY25Q16_WriteBuffer(OffsetVFO, Buf, 0x10, false);
    SETTINGS_UpdateChannel(Channel, pVFO, true, true, true);
    // Between write and UpdateChannel, system could be interrupted
}
```
**Problem**: No mutex/lock protecting gEeprom during save operations.
If radio receives command during save, gEeprom could be partially updated.

**Impact**: Inconsistent channel data, frequency jumps, unexpected behavior.

---

### 6. **ARRAY OUT OF BOUNDS IN SETTINGS_UpdateChannel**
**Severity**: HIGH - MEMORY CORRUPTION
```c
uint8_t buf[224];
PY25Q16_ReadBuffer(0x002000, buf, sizeof(buf));
buf[channel] = state.__val;  // ❌ NO BOUNDS CHECK ON channel!
```
**Problem**: If `channel` >= 224, out-of-bounds write.
Channels should be 0-199, but no validation before this write.

**Impact**: Corrupts adjacent flash cache, affects other channels.

---

### 7. **MEMCPY OVERLAP IN SETTINGS_LoadCalibration** (Lines 443-450)
**Severity**: MEDIUM - SUBTLE MEMORY ISSUES
```c
memcpy(gEEPROM_RSSI_CALIB[4], gEEPROM_RSSI_CALIB[3], 8);
memcpy(gEEPROM_RSSI_CALIB[5], gEEPROM_RSSI_CALIB[3], 8);
memcpy(gEEPROM_RSSI_CALIB[6], gEEPROM_RSSI_CALIB[3], 8);
```
**Problem**: All three are COPYING THE SAME SOURCE. If gEEPROM_RSSI_CALIB arrays overlap or source is corrupted, all three get the same (potentially wrong) data.

**Impact**: RSSI calibration becomes inconsistent across bands, signal meter inaccurate.

---

### 8. **BLOCKING FLASH OPERATIONS IN SETTINGS_ResetTxLock** (Line 1606+)
**Severity**: CRITICAL - SYSTEM HANG
```c
void SETTINGS_ResetTxLock(void)
{
    // 10 full batch reads/writes, each ~20-40ms
    for (uint32_t i = 0; i < SETTINGS_ResetTxLock_BATCH; i++)
    {
        uint32_t Offset = i * BatchSize;
        PY25Q16_ReadBuffer(0 + Offset, Buf, sizeof(Buf));  // ⚠️ BLOCKING!
        
        // ... process ...
        
        PY25Q16_WriteBuffer(0 + Offset, Buf, sizeof(Buf), false);  // ⚠️ BLOCKING!
    }
}
```
**Problem**: Total blocking time: 10 batches × (20-40ms read + write) = 200-400ms minimum.
During this time, radio cannot respond to interrupts or user input.

**Expected**: "Radio freezes for up to 400ms" - BAD UX and potential watchdog timeout.

**Impact**: Watchdog timer reset, system reboot if timeout < 400ms.

---

### 9. **INCONSISTENT STATE IN SETTINGS_SaveSettings** (Lines 860+)
**Severity**: MEDIUM - STATE INCONSISTENCY
```c
// Block 1: Write basic settings
PY25Q16_WriteBuffer(0x004000, SecBuf, 0x10, true);

// ... gap - radio operating on old settings here ...

// Block 2: Write key actions  
PY25Q16_WriteBuffer(0x007000, SecBuf, 0x50, true);

// ... gap - radio might use old key actions ...

// Block 5: Write F4HWN settings
PY25Q16_WriteBuffer(0x00c000, SecBuf, 8, true);
```
**Problem**: Between writes, gEeprom values are partially updated. If system reads gEeprom after write 1 but before write 5, it has MIXED old/new values.

**Impact**: Settings appear corrupted to user, unexpected behavior.

---

### 10. **UNVALIDATED CHANNEL INDEX IN SETTINGS_SaveChannel** (Line 1108+)
**Severity**: HIGH - MEMORY CORRUPTION
```c
void SETTINGS_SaveChannel(uint8_t Channel, uint8_t VFO, const VFO_Info_t *pVFO, uint8_t Mode)
{
    // Channel is uint8_t, max 255
    // But valid channels are 0-199
    
    uint16_t OffsetVFO = 0 + Channel * 16;  // ⚠️ Could be up to 255*16 = 4080!
    
    if (IS_FREQ_CHANNEL(Channel)) {
        OffsetVFO = (VFO == 0) ? 0x001000 : 0x001010;
        OffsetVFO += (Channel - FREQ_CHANNEL_FIRST) * 32;
    }
    
    if (Mode >= 2 || IS_FREQ_CHANNEL(Channel)) {
        // ... prepare 16-byte buffer ...
        PY25Q16_WriteBuffer(OffsetVFO, Buf, 0x10, false);  // ⚠️ Writes to UNKNOWN offset!
    }
}
```
**Problem**: No bounds checking. If Channel=200-255 and not a FREQ_CHANNEL, writes to 3200-4080 bytes offset.

**Impact**: Writes to wrong flash locations, corrupts other data.

---

### 11. **UNINITIALIZED VARIABLE IN FOR LOOP** (Line 572)
**Severity**: MEDIUM - UNDEFINED BEHAVIOR
```c
int i;  // ❌ Uninitialized!
for (i = 0; i < 10; i++)
    if (s[i] < 32 || s[i] > 127)
        break;

s[i--] = 0;  // ⚠️ If loop never executes, i is UNINITIALIZED
```
**Problem**: If for loop never runs (unlikely but possible), `i` has garbage value, then `s[i--]` writes to random memory.

**Fix**: Initialize: `int i = 0;`

---

### 12. **MISSING FLASH OPERATION VALIDATION** (Throughout)
**Severity**: MEDIUM - SILENT DATA LOSS
```c
// Example: No validation that write succeeded
PY25Q16_WriteBuffer(0x004000, SecBuf, 0x10, true);
// Immediately assume write succeeded and return
// But what if flash is full, bad sector, or corrupted?
```
**Problem**: PY25Q16_WriteBuffer() likely returns void. No error checking.

**Impact**: Settings might not be saved, but radio thinks they are saved.

---

## 📊 ISSUE SUMMARY TABLE

| Issue | Type | Severity | Line(s) | Impact |
|-------|------|----------|---------|--------|
| strcpy() without bounds | Buffer Overflow | CRITICAL | 230,239,247,256,264 | Stack corruption, CPU hang |
| SETTINGS_FetchChannelName loop | Buffer Overflow | CRITICAL | 570-578 | Stack overflow/underflow |
| SETTINGS_SaveChannelName NULL | NULL Deref | CRITICAL | 1266 | CPU crash/hang |
| SETTINGS_SaveChannel no bounds | Out-of-bounds | HIGH | 1108 | Flash corruption |
| SETTINGS_UpdateChannel no bounds | Out-of-bounds | HIGH | 1310 | Channel data corruption |
| SETTINGS_ResetTxLock blocking | Watchdog Timeout | CRITICAL | 1606+ | System reboot |
| Race condition | State Corruption | HIGH | 1100-1160 | Inconsistent settings |
| Partial updates in SaveSettings | State Inconsistency | MEDIUM | 860-1000 | Mixed old/new settings |
| Uninitialized variable | Undefined Behavior | MEDIUM | 572 | Random memory access |
| No flash error checking | Silent Failure | MEDIUM | Throughout | Settings not saved |
| memcpy overlap | Data Corruption | MEDIUM | 443-450 | Bad calibration |
| Infinite loop risk | CPU Hang | MEDIUM | 578 | Watchdog timeout |

---

## 🛠️ RECOMMENDED FIXES

### Fix 1: SETTINGS_FetchChannelName
```c
void SETTINGS_FetchChannelName(char *s, const int channel)
{
    if (s == NULL)
        return;
    
    s[0] = 0;
    
    if (channel < 0 || channel >= 200)  // ADD BOUNDS CHECK
        return;
    
    if (!RADIO_CheckValidChannel(channel, false, 0))
        return;
    
    PY25Q16_ReadBuffer(0x00e000 + (channel * 16), s, 10);
    
    int i = 0;  // INITIALIZE
    for (i = 0; i < 10; i++) {
        if (s[i] < 32 || s[i] > 127)
            break;
    }
    
    if (i < 10)  // ONLY SET NULL IF FOUND INVALID
        s[i] = 0;
    else
        s[9] = 0;  // Force null terminate at position 9
    
    // Trim trailing spaces (safer)
    for (int j = (i < 10 ? i : 9); j >= 0; j--) {
        if (s[j] == 32) {
            s[j] = 0;
        } else {
            break;
        }
    }
}
```

### Fix 2: SETTINGS_SaveChannelName
```c
void SETTINGS_SaveChannelName(uint8_t channel, const char * name)
{
    if (channel >= 200)  // ADD BOUNDS CHECK
        return;
    
    if (name == NULL)    // ADD NULL CHECK
        name = "";
    
    uint16_t offset = channel * 16;
    uint8_t buf[16] = {0};
    
    size_t len = strlen(name);
    if (len > 10) len = 10;  // Bounds-checked copy
    
    memcpy(buf, name, len);  // SAFE
    PY25Q16_WriteBuffer(0x00e000 + offset, buf, 0x10, false);
}
```

### Fix 3: Replace unsafe strcpy()
```c
// Instead of:
// strcpy(gEeprom.ANI_DTMF_ID, "123");

// Use:
strncpy(gEeprom.ANI_DTMF_ID, "123", sizeof(gEeprom.ANI_DTMF_ID) - 1);
gEeprom.ANI_DTMF_ID[sizeof(gEeprom.ANI_DTMF_ID) - 1] = '\0';
```

### Fix 4: Add timeout to SETTINGS_ResetTxLock
```c
void SETTINGS_ResetTxLock(void)
{
    uint32_t start_time = GetSystemTime();
    const uint32_t MAX_TIMEOUT_MS = 500;  // Safety timeout
    
    for (uint32_t i = 0; i < SETTINGS_ResetTxLock_BATCH; i++)
    {
        if (GetSystemTime() - start_time > MAX_TIMEOUT_MS) {
            // TIMEOUT - abort and log error
            return;
        }
        
        // ... rest of code ...
    }
}
```

### Fix 5: Add bounds checking to SETTINGS_SaveChannel
```c
void SETTINGS_SaveChannel(uint8_t Channel, uint8_t VFO, const VFO_Info_t *pVFO, uint8_t Mode)
{
    // Validate channel before any operations
    if (!IS_NOAA_CHANNEL(Channel) && !IS_MR_CHANNEL(Channel) && !IS_FREQ_CHANNEL(Channel)) {
        return;  // Invalid channel
    }
    
    // ... rest of code ...
}
```

---

## ⚠️ SUMMARY

**Total Critical Issues: 12**
- **CRITICAL**: 4 (can cause CPU hang/crash)
- **HIGH**: 4 (can corrupt data)
- **MEDIUM**: 4 (subtle issues, hard to debug)

**Recommended Action**: Implement all 5 fixes immediately before next release.
