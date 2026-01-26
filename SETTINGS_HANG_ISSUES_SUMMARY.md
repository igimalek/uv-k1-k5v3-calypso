# SETTINGS.C - CRITICAL ISSUE QUICK REFERENCE

## 🚨 IMMEDIATE CRASH RISKS (Fix TODAY)

### 1️⃣ strcpy() WITHOUT BOUNDS (Lines 230, 239, 247, 256, 264)
```
SETTINGS_InitEEPROM() 
  → strcpy(gEeprom.ANI_DTMF_ID, "123")
  → strcpy(gEeprom.KILL_CODE, "ABCD9")  
  → strcpy(gEeprom.REVIVE_CODE, "9DCBA")
  → strcpy(gEeprom.DTMF_UP_CODE, "12345")
  → strcpy(gEeprom.DTMF_DOWN_CODE, "54321")
  
❌ RISK: Stack overflow → CPU hang/crash
✓ FIX: Use strncpy() with sizeof()
```

### 2️⃣ BUFFER OVERFLOW IN SETTINGS_FetchChannelName (Line 570-578)
```
SETTINGS_FetchChannelName(s, channel)
  → s[i--] = 0  where i=10 (OUT OF BOUNDS!)
  → while (i >= 0 && s[i] == 32)  INFINITE LOOP!
  
❌ RISK: Stack corruption → function return address overwrite → CPU crash
✓ FIX: Bounds-check loop index, initialize i=0
```

### 3️⃣ NULL DEREFERENCE IN SETTINGS_SaveChannelName (Line 1266)
```
void SETTINGS_SaveChannelName(uint8_t channel, const char * name)
  → strlen(name)  IF name==NULL → CRASH!
  → MIN(strlen(name), 10u)  BEFORE NULL CHECK!
  
❌ RISK: CPU exception handler → hang
✓ FIX: Check for NULL before strlen()
```

### 4️⃣ NO CHANNEL BOUNDS IN SETTINGS_SaveChannel (Line 1108)
```
uint16_t OffsetVFO = 0 + Channel * 16;
  IF Channel = 255 → OffsetVFO = 4080
  → PY25Q16_WriteBuffer(4080, ...)  WRONG FLASH LOCATION!
  
❌ RISK: Flash memory corruption
✓ FIX: Validate channel before write: if (channel >= 200) return;
```

---

## 🔥 WATCHDOG TIMEOUT RISKS (System reboot)

### 5️⃣ BLOCKING FLASH IN SETTINGS_ResetTxLock (Line 1606+)
```
SETTINGS_ResetTxLock()
  for 10 batches {
    PY25Q16_ReadBuffer()  ← 20-40ms BLOCKING
    process()
    PY25Q16_WriteBuffer() ← 20-40ms BLOCKING
  }
  TOTAL: 200-400ms BLOCKING ❌
  
IF watchdog timeout < 400ms → SYSTEM REBOOT!

✓ FIX: Add timeout abort, move to background task, or split operations
```

---

## 💥 MEMORY CORRUPTION RISKS

### 6️⃣ NO BOUNDS IN SETTINGS_UpdateChannel (Line 1310)
```
uint8_t buf[224];
buf[channel] = state.__val;  ← NO CHECK IF channel >= 224!

IF channel = 230 → buf[230] writes OUTSIDE allocated buffer
❌ RISK: Corrupts gMR_ChannelAttributes cache, adjacent flash data

✓ FIX: if (channel >= 224) return;
```

### 7️⃣ RACE CONDITION IN SETTINGS_SaveChannel (Line 1100-1160)
```
SETTINGS_SaveChannel() {
  PY25Q16_WriteBuffer() ← Write finishes
  [⚠️ GAP: Radio could interrupt here with command reading gEeprom]
  SETTINGS_UpdateChannel() ← Attributes updated
}

❌ RISK: Mixed old/new settings in gEeprom during read
✓ FIX: Disable interrupts during entire save sequence
```

### 8️⃣ PARTIAL UPDATES IN SETTINGS_SaveSettings (Line 860+)
```
Block 1: Write settings to 0x004000
[⚠️ GAP: gEeprom partially updated]
Block 2: Write settings to 0x007000  
[⚠️ GAP: gEeprom still partially updated]
...
Block 5: Write settings to 0x00C000

TOTAL: 5 separate writes with gaps between them!
❌ RISK: User reads gEeprom, sees mix of old/new values

✓ FIX: Batch all writes, or use single atomic transaction
```

---

## 📋 FUNCTION-BY-FUNCTION RISK ANALYSIS

| Function | Issues | Risk Level | Fix Priority |
|----------|--------|-----------|--------------|
| SETTINGS_InitEEPROM | strcpy overflows | CRITICAL | 🔴 P0 |
| SETTINGS_LoadCalibration | memcpy overlap | MEDIUM | 🟡 P2 |
| SETTINGS_FetchChannelFrequency | ✓ OK | LOW | - |
| SETTINGS_FetchChannelName | Buffer overflow, infinite loop | CRITICAL | 🔴 P0 |
| SETTINGS_FactoryReset | ✓ Mostly OK | LOW | - |
| SETTINGS_SaveFM | Need to verify | MEDIUM | 🟡 P2 |
| SETTINGS_SaveVfoIndices | ✓ OK | LOW | - |
| SETTINGS_SaveSettings | Race condition, partial updates | HIGH | 🔴 P0 |
| SETTINGS_SaveChannel | No bounds check | HIGH | 🔴 P0 |
| SETTINGS_SaveBatteryCalibration | ✓ OK | LOW | - |
| SETTINGS_SaveChannelName | NULL deref | CRITICAL | 🔴 P0 |
| SETTINGS_UpdateChannel | No bounds check | HIGH | 🔴 P0 |
| SETTINGS_WriteBuildOptions | ✓ OK | LOW | - |
| SETTINGS_WriteCurrentState | ✓ Conditional, OK | LOW | - |
| SETTINGS_WriteCurrentVol | ✓ OK | LOW | - |
| SETTINGS_ResetTxLock | Blocking watchdog timeout | CRITICAL | 🔴 P0 |

---

## 🔧 IMPLEMENTATION ORDER

### Phase 1 (CRITICAL - DO IMMEDIATELY)
```
1. Replace all strcpy() with strncpy() + bounds checking
   → Prevents immediate stack overflow on radio startup

2. Fix SETTINGS_FetchChannelName loop logic
   → Prevents buffer overflow on channel name fetch

3. Add NULL check to SETTINGS_SaveChannelName
   → Prevents crash when saving channel names

4. Add channel bounds checking to:
   - SETTINGS_SaveChannel()
   - SETTINGS_UpdateChannel()
   → Prevents flash memory corruption

5. Add timeout to SETTINGS_ResetTxLock()
   → Prevents watchdog reboot during kill/revive operations
```

### Phase 2 (HIGH - DO WITHIN WEEK)
```
6. Disable interrupts around SETTINGS_SaveChannel() 
   → Prevents race condition with gEeprom reads

7. Consolidate SETTINGS_SaveSettings() writes
   → Prevents seeing mixed old/new settings

8. Add error checking to PY25Q16_WriteBuffer() calls
   → Prevents silent write failures
```

### Phase 3 (MEDIUM - DO WITHIN MONTH)
```
9. Audit all memcpy() operations for overlap
10. Add bounds checking to array accesses throughout
11. Add logging of settings operations for debugging
```

---

## 🧪 TEST CASES TO VALIDATE FIXES

### Crash Test 1: Name Fetch with Corrupted Flash
```c
// Clear flash channel names to 0x00
memset(flash[0xE000], 0x00, 256);
// Try to fetch channel name
SETTINGS_FetchChannelName(buf, 0);
// Should not crash, should return empty string
```

### Crash Test 2: Kill/Revive Timeout
```c
// Call SETTINGS_ResetTxLock() 
// Should complete within 500ms without watchdog reset
uint32_t start = GetSystemTime();
SETTINGS_ResetTxLock();
uint32_t elapsed = GetSystemTime() - start;
ASSERT(elapsed < 500);  // Should timeout before watchdog
```

### Crash Test 3: Channel Name Save Flood
```c
// Rapidly save channel names
for (int i = 0; i < 200; i++) {
    SETTINGS_SaveChannelName(i, "TEST");
    // Should not crash or corrupt adjacent memory
}
```

### Crash Test 4: Invalid Channel Access
```c
// Try to save channel 255 (out of range)
SETTINGS_SaveChannel(255, 0, &vfo, 0);
// Should return immediately without writing to flash
```

---

## 📌 CONCLUSION

**Current Status**: settings.c has **12 critical issues** that can cause:
- ✗ CPU hangs/crashes (4 issues)
- ✗ Memory corruption (4 issues) 
- ✗ Data loss (4 issues)

**Risk Level**: 🔴 **CRITICAL** - Production system could fail unexpectedly

**Estimated Fix Time**: 
- Phase 1 (5 issues): 2-3 hours
- Phase 2 (3 issues): 4-5 hours
- Phase 3 (4 issues): 6-8 hours
- Testing: 8-10 hours
- **Total**: ~24 hours of focused work

**Recommendation**: Implement Phase 1 immediately before next release.
