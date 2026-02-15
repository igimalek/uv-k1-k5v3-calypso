# Menu System Call Chain Documentation

> **Last Updated:** 2026-02-15  
> **Purpose:** Document the complete flow from button press → menu interaction → flash save

---

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Complete Call Chain Flow](#complete-call-chain-flow)
4. [Function Reference](#function-reference)
5. [Global State Variables](#global-state-variables)
6. [Special Menu Cases](#special-menu-cases)
7. [Flash Save Triggers](#flash-save-triggers)
8. [Timing and Sequencing](#timing-and-sequencing)

---

## Overview

The menu system implements a hierarchical state machine:
- **Level 0:** Menu list navigation (UP/DOWN between menu items)
- **Level 1:** Submenu value editing (UP/DOWN to adjust, 0-9 for digit entry)
- **Special modes:** CSS scan, name editing, channel deletion

Key concept: **Button presses trigger immediate state changes; flash writes are deferred and batched** via request flags that are processed in `ProcessKey()`.

---

## System Architecture

### Module Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                 GPIO / Keyboard Interrupt                    │
└────────────────────┬────────────────────────────────────────┘
                     │ (KEY_Code_t)
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              ProcessKey() [app.c]                            │
│  - Centralized key routing                                   │
│  - Handles PTT, keylock, low battery                         │
│  - Routes to ProcessKeysFunctions[] dispatch table           │
└────────────────────┬────────────────────────────────────────┘
                     │
        ┌────────────┴────────────┐
        │                         │
        ▼ (DISPLAY_MENU)          ▼ (Other displays)
┌──────────────────┐       ┌──────────────────┐
│ MENU_ProcessKeys │       │ UI_ProcessKeys   │
│     [app.c]      │       │ MAIN_ProcessKeys │
│                  │       │   etc.           │
└────────┬─────────┘       └──────────────────┘
         │
         ├─► gIsInSubMenu == false
         │   ├─► UP/DOWN: Navigate menu (gMenuCursor±1)
         │   ├─► MENU: Enter submenu (gIsInSubMenu=true)
         │   └─► EXIT: Cancel
         │
         └─► gIsInSubMenu == true
             ├─► UP/DOWN: Adjust value (gSubMenuSelection±1)
             ├─► 0-9: Enter digit (gInputBoxIndex)
             ├─► MENU: Accept and save (MENU_AcceptSetting())
             └─► EXIT: Cancel without save
```

---

## Complete Call Chain Flow

### Flow 1: Button Press → Menu Value Change → Flash Write

#### Scenario: User presses UP in submenu (editing Squelch Level)

```
KEYBOARD INTERRUPT
    │
    └─► ProcessKey(KEY_UP, true, false)
        │
        └─► if (DISPLAY_MENU) ProcessKeysFunctions[DISPLAY_MENU](...)
            │
            └─► MENU_ProcessKeys(KEY_UP, true, false)
                │
                ├─► if (!gIsInSubMenu)
                │   │   // User navigating menu list
                │   │
                │   └─► gMenuCursor++
                │       gUpdateDisplay = true
                │
                └─► if (gIsInSubMenu)
                    │   // User editing a value
                    │
                    ├─► Get current menu_id
                    │
                    ├─► Get limits (MENU_GetLimits)
                    │
                    ├─► Validate: gSubMenuSelection < Min ? = Min
                    │
                    ├─► gSubMenuSelection++
                    │
                    ├─► gUpdateDisplay = true
                    │
                    └─► Sync to internal variable:
                        (radio.h, audio.h, etc.)
                        (NO flash write yet!)
```

#### Key Point: **Value change ≠ Flash write**

The internal variable is updated immediately to show live preview, but EEPROM write is deferred.

---

### Flow 2: User Confirms Change (MENU Button) → Flash Write

#### Scenario: User presses MENU to accept Squelch Level change

```
KEYBOARD INTERRUPT
    │
    └─► ProcessKey(KEY_MENU, true, false)
        │
        └─► MENU_ProcessKeys(KEY_MENU, true, false)
            │
            ├─► if (gIsInSubMenu)
            │   │
            │   ├─► gFlagAcceptSetting = true
            │   │
            │   ├─► gIsInSubMenu = false
            │   │
            │   ├─► gInputBoxIndex = 0
            │   │
            │   └─► gUpdateDisplay = true
            │
            └─► [ProcessKey() continuation]
                │
                └─► if (gFlagAcceptSetting)
                    │
                    └─► gMenuCountdown = menu_timeout_500ms
                        │
                        ├─► MENU_AcceptSetting()
                        │   │
                        │   ├─► switch(current_menu_id)
                        │   │   │
                        │   │   ├─► case MENU_SQL:
                        │   │   │   └─► gEeprom.SQUELCH_LEVEL = gSubMenuSelection
                        │   │   │       gVfoConfigureMode = VFO_CONFIGURE
                        │   │   │       return  [NO SAVE FLAG]
                        │   │   │
                        │   │   ├─► case MENU_STEP:
                        │   │   │   └─► gTxVfo->STEP_SETTING = ...
                        │   │   │       gRequestSaveChannel = 1
                        │   │   │       return
                        │   │   │
                        │   │   ├─► case MENU_TXP:
                        │   │   │   └─► gTxVfo->OUTPUT_POWER = ...
                        │   │   │       gRequestSaveChannel = 1
                        │   │   │       return
                        │   │   │
                        │   │   ├─► case MENU_MEM_NAME:
                        │   │   │   └─► SETTINGS_SaveChannelName(...)
                        │   │   │       return
                        │   │   │
                        │   │   └─► ... [other cases] ...
                        │   │
                        │   └─► if (gVfoConfigureMode != NONE)
                        │       └─► gRequestDisplayScreen = DISPLAY_MAIN
                        │
                        ├─► gFlagRefreshSetting = true
                        │
                        └─► gFlagAcceptSetting = false
```

#### Post-ProcessKey() Save Handling (still in ProcessKey() continuation)

```
                            ┌─────────────────────────────────┐
                            │   ProcessKey() save dispatch     │
                            └─────────────────────────────────┘
                                    │
        ┌───────────────┬───────────┼───────────┬───────────────┐
        │               │           │           │               │
        ▼               ▼           ▼           ▼               ▼
    SETTINGS_       SETTINGS_   SETTINGS_   SETTINGS_      RADIO_
    SaveSettings   SaveFM      SaveVFO     SaveChannel    Configure
                                                          Channel
    
    (gRequestSaveSettings)  (gFlagSaveFM)  (gRequestSaveVFO)  (gRequestSaveChannel)
        │
        ├─► if (!bKeyHeld)  ◄─── KEY HOLD DETECTION
        │   │   Immediate write to flash
        │   │
        │   └─► EEPROM_WriteBuffer()
        │       FLASH_WriteBuffer()
        │
        └─► else (if bKeyHeld)
            │   Defer write until key release
            │
            └─► Set flag (gFlagSaveFM, flagSaveChannel, etc.)
                → Will be processed on key release
```

---

## Function Reference

### Main Menu Processing

#### `MENU_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)`
**File:** `App/app/menu.c` (line ~1935)  
**Called by:** `ProcessKey()` via `ProcessKeysFunctions[DISPLAY_MENU]`

**Responsibilities:**
- Route key events to menu navigation or value editing logic
- Manage `gIsInSubMenu` state transitions
- Accept/cancel input with MENU/EXIT keys
- Handle numeric digit entry (0-9)
- Handle special keys (STAR for CSS scan)

**Key Decision Table:**
```
Input (gIsInSubMenu=false, menu mode)
    ├─ UP/DOWN       → gMenuCursor ± 1 (wrap around)
    ├─ MENU (press)  → gIsInSubMenu=true, gSubMenuSelection=current
    └─ EXIT          → (state unchanged)

Input (gIsInSubMenu=true, edit mode)
    ├─ UP/DOWN       → gSubMenuSelection ± adjust
    ├─ 0-9           → gInputBoxIndex-based digit entry or direct set
    ├─ MENU (press)  → gFlagAcceptSetting=true, gIsInSubMenu=false
    ├─ STAR          → (CSS scan for R_CTCS/R_DCS only)
    └─ EXIT          → gIsInSubMenu=false (discard changes)
```

---

#### `MENU_AcceptSetting(void)`
**File:** `App/app/menu.c` (line ~450)  
**Called by:** `ProcessKey()` when `gFlagAcceptSetting == true`

**Responsibilities:**
- Validate the edited value against menu-specific limits
- Update internal state variables (gEeprom, gTxVfo, etc.)
- Set save request flags (`gRequestSaveChannel`, `gRequestSaveSettings`, etc.)
- Return early for terminal saves (memory name, reset)

**Example: MENU_SQL Case**
```c
case MENU_SQL:
    gEeprom.SQUELCH_LEVEL = gSubMenuSelection;  // Update RAM copy
    gVfoConfigureMode = VFO_CONFIGURE;          // Mark VFO dirty
    break;                                        // (NO immediate save)
```

**Example: MENU_TXP Case**
```c
case MENU_TXP:
    gTxVfo->OUTPUT_POWER = gSubMenuSelection;   // Update RAM
    gRequestSaveChannel = 1;                     // Request save
    return;                                       // Exit immediately
```

---

#### `MENU_ShowCurrentSetting(void)`
**File:** `App/app/menu.c` (line ~979)

**Called by:**
- `MENU_ProcessKeys()` before returning
- `ProcessKey()` when `gFlagRefreshSetting == true`

**Responsibilities:**
- Refresh `gSubMenuSelection` from current setting value
- Clear input box state
- Trigger UI display refresh
- Update voice announcement (if ENABLE_VOICE)

---

#### `MENU_GetLimits(uint8_t menu_id, int32_t *pMin, int32_t *pMax)`
**File:** `App/app/menu.c` (line ~113)

**Responsibilities:**
- Provide min/max valid range for each menu item
- Called by `MENU_AcceptSetting()` to clamp values
- Different scales: some 0-9, some 0-207, some freq values

---

### CSS Tone Scan Functions

#### `MENU_StartCssScan(void)`
**File:** `App/app/menu.c` (line ~78)

**Triggered by:** STAR key pressed when `gScreenToDisplay == DISPLAY_MENU` and menu item is R_CTCS or R_DCS

**Sets:**
```c
SCANNER_Start(true);              // Start hardware scan
gUpdateStatus = true;
gCssBackgroundScan = true;        // Flag for main loop
gRequestDisplayScreen = DISPLAY_MENU;
```

**Continues:** While user is in DISPLAY_MENU, scan runs in background (processed in APP_TimeSlice10ms)

---

#### `MENU_StopCssScan(void)`
**File:** `App/app/menu.c` (line ~102)

**Triggered by:** EXIT key pressed while scan active, or timeout

**Sets:**
```c
gCssBackgroundScan = false;
gUpdateDisplay = true;
gUpdateStatus = true;
```

---

#### `MENU_CssScanFound(void)`
**File:** `App/app/menu.c` (line ~87)

**Called by:** Background scanner when tone found (from `SCANNER_TimeSlice500ms()`)

**Sets:**
- Updates `gMenuCursor` to R_DCS or R_CTCS
- Updates `gSubMenuSelection` to scanned value
- Calls `MENU_ShowCurrentSetting()` to display
- Sets `gUpdateDisplay = true`

**Note:** User must manually confirm with MENU button to save

---

## Global State Variables

### Menu Navigation State

| Variable | Type | Purpose | Modified by |
|----------|------|---------|-------------|
| `gScreenToDisplay` | `GUI_DisplayType_t` | Current screen mode (DISPLAY_MAIN, DISPLAY_MENU, ...) | `GUI_SelectNextDisplay()` |
| `gMenuCursor` | `uint8_t` | Index in MenuList[] (0 = first item) | `MENU_ProcessKeys()` |
| `gIsInSubMenu` | `bool` | false=menu list, true=value editing | `MENU_ProcessKeys()` |
| `gSubMenuSelection` | `int32_t` | Currently edited value | `MENU_ProcessKeys()` |
| `gInputBoxIndex` | `uint8_t` | Position in digit entry | `MENU_ProcessKeys()` |
| `gMenuCountdown` | `uint16_t` | Timeout counter (500ms units) | Reset on any DISPLAY_MENU key press |

### Menu Update/Display Requests

| Variable | Type | Purpose | Triggers Save |
|----------|------|---------|----------------|
| `gFlagAcceptSetting` | `bool` | Queue MENU_AcceptSetting() | Sets save flags (conditional) |
| `gFlagRefreshSetting` | `bool` | Refresh submenu value from setting | No |
| `gUpdateDisplay` | `bool` | Redraw screen on next GUI_DisplayScreen() | No |
| `gUpdateStatus` | `bool` | Redraw status bar | No |

### Flash Save Request Flags

| Variable | Type | Context | Cleared by |
|----------|------|---------|-----------|
| `gRequestSaveSettings` | `bool` | EEPROM general settings (audio, display, etc.) | `ProcessKey()` after write |
| `gRequestSaveChannel` | `uint8_t` | Current VFO frequency/mode settings | `ProcessKey()` after write |
| `gRequestSaveVFO` | `bool` | VFO index selection (VFO_A vs VFO_B) | `ProcessKey()` after write |
| `gRequestSaveFM` | `bool` | FM radio settings | `ProcessKey()` after write |
| `gFlagSaveFM` | `bool` | Deferred FM save (button held) | `ProcessKey()` on key release |
| `flagSaveChannel` | `static bool` | Deferred channel save (button held) | `ProcessKey()` on key release |
| `flagSaveSettings` | `static bool` | Deferred settings save (button held) | `ProcessKey()` on key release |

---

## Special Menu Cases

### Case 1: Memory Channel Name Editing (MENU_MEM_NAME)

```
User presses MENU on MENU_MEM_NAME item
    │
    └─► gIsInSubMenu = true
        gSubMenuSelection = channel number
        
        Button presses now enter character editing:
        ├─► 0-9, A-Z, →, ← : Edit character at gInputBoxIndex
        ├─► UP/DOWN        : Cycle through available characters
        └─► MENU           : Call MENU_AcceptSetting()
            │
            └─► SETTINGS_SaveChannelName(gSubMenuSelection, gChannelName)
                │
                └─► EEPROM_WriteBuffer() → IMMEDIATE FLASH WRITE
                    (Terminal save, no flag needed)
```

**Special:** `MENU_AcceptSetting()` returns immediately after writing, 
doesn't propagate to generic save handlers.

---

### Case 2: Channel Deletion (MENU_DEL_CH)

```
User navigates to channel number and presses MENU
    │
    ├─► gAskToDelete = true
    │   (Shows confirmation dialog in UI_DisplayMenu)
    │
    └─► Next MENU press:
        │
        ├─► if (gAskToDelete == true)
        │   │
        │   └─► SETTINGS_DeleteChannel(channel)
        │       │
        │       └─► Mark channel as unused in EEPROM
        │           FLASH_WriteBuffer()
        │           (Terminal write)
        │
        └─► gAskToDelete = false
```

---

### Case 3: Factory Reset (MENU_RESET)

```
User presses MENU on MENU_RESET
    │
    ├─► gAskForConfirmation = 1 (first confirmation)
    │   (Shows "SURE?" in UI)
    │
    └─► Second MENU press:
        │
        └─► gAskForConfirmation = 2 (confirmed)
            │
            └─► gRequestSaveSettings = true
                gVfoConfigureMode = VFO_CONFIGURE_RELOAD
                (Full system reset flag)
```

**Then in ProcessKey():**
```c
if (gVfoConfigureMode != VFO_CONFIGURE_NONE) {
    RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
    RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);
    SETTINGS_SaveSettings();  // Write factory defaults
}
```

---

## Flash Save Triggers

### Immediate Flash Writes (Terminal Saving)

These functions write directly to flash without request flag:

| Function | Triggered From | Use Case |
|----------|---|---|
| `SETTINGS_SaveChannelName()` | `MENU_AcceptSetting()` MENU_MEM_NAME case | Name editing |
| `SETTINGS_DeleteChannel()` | Channel delete confirmation | Remove channel |
| `RADIO_ConfigureChannel()` with RELOAD | Reset factory defaults | Wipe settings |

**Code pattern:**
```c
if (condition) {
    SETTINGS_SaveChannelName(channel, name);  // Direct write
    return;  // Exit early, no flag needed
}
```

---

### Deferred Flash Writes (Batch Processing)

Processing happens in `ProcessKey()` after menu item is accepted:

```c
// 1. Value accepted and MENU_AcceptSetting() returned
// 2. Save request flag is set (if applicable)
// 3. ProcessKey() checks the flag:

if (gRequestSaveChannel > 0) {
    // Check if key is still held
    if ((!bKeyHeld && !bKeyPressed) || UI_MENU_GetCurrentMenuId()) {
        SETTINGS_SaveChannel(...);      // Write now
        gRequestSaveChannel = 0;
    } else if (bKeyHeld) {
        flagSaveChannel = gRequestSaveChannel;  // Defer
    }
}

if (gRequestSaveSettings) {
    if (!bKeyHeld) {
        SETTINGS_SaveSettings();
    } else {
        flagSaveSettings = 1;  // Defer
    }
    gRequestSaveSettings = false;
}
```

**Deferred writes are completed in ProcessKey() on next key release.**

---

## Timing and Sequencing

### Per-Keystroke Sequence (100ms max)

```
T₀: Key press detected
│
T₁: ProcessKey() invoked
├─► MENU_ProcessKeys() routes to menu state machine
├─► gSubMenuSelection/gMenuCursor updated
├─► gUpdateDisplay = true
└─► Return from ProcessKey()

T₂-T₃: gUpdateDisplay handled in GUI_DisplayScreen()
└─► LCD redrawn with new value showing

T₄: Next key press or timeout
```

**User perceives:** Live response (<100ms) to UP/DOWN keys

---

### On MENU Button (Accept)

```
T₀: MENU key press
│
T₁: ProcessKey() called
├─► MENU_ProcessKeys() sets gFlagAcceptSetting=true
│
T₂: Continuation in ProcessKey()
├─► MENU_AcceptSetting() executes
│   ├─► RAM variables updated
│   └─► gRequestSaveChannel set (conditional)
│
├─► Save handler checks flags
│   ├─► if !bKeyHeld: Write now
│   └─► else: Defer to release
│
└─► Return

T₃: Flash write (immediate or later)
```

---

### Menu Timeout Behavior

```
Menu timeout counter: gMenuCountdown (500ms units)
├─► Reset to menu_timeout_500ms (~3 sec) on any DISPLAY_MENU key
│
└─► Every 500ms from APP_TimeSlice500ms():
    └─► if (--gMenuCountdown == 0)
        └─► if (!gCssBackgroundScan && gScanStateDir == SCAN_OFF ...)
            ├─► DTMF_clear_input_box()
            ├─► gRequestDisplayScreen = DISPLAY_MAIN
            ├─► GUI_SelectNextDisplay(DISPLAY_MAIN)
            └─► Menu cleared, return to main screen
```

**Exit behavior:**
- Accepted changes ARE saved (gRequestSaveChannel already set)
- Pending edits (not accepted with MENU) are LOST
- RAM copy discarded, next menu entry re-syncs from EEPROM

---

## Call Chain Summary Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    KEYBOARD INTERRUPT                            │
│                                                                  │
│  Hardware: GPIO edge detected → Driver queues KEY_Code_t        │
└─────────────────────────┬───────────────────────────────────────┘
                          │
                          ▼
        ┌─────────────────────────────────────┐
        │  ProcessKey() [app.c line ~2077]    │
        │  Key routing dispatcher             │
        └─────────────────────────────────────┘
                          │
                ┌─────────┴──────────────┐
                │                        │
      ┌─────────▼──────────┐   ┌────────▼────────────┐
      │ if DISPLAY_MENU    │   │ Other screens       │
      │ ProcessKeysFunctions │  │ MAIN ProcessKeys... │
      │ [MENU_ProcessKeys] │   │                    │
      └─────────┬──────────┘   └────────────────────┘
                │
       ┌────────▼──────────────────────────┐
       │ MENU_ProcessKeys() [menu.c]       │
       │                                  │
       │ ├─ gIsInSubMenu: false           │
       │ │  └─ UP/DOWN → gMenuCursor±1    │
       │ │              MENU → enter edit │
       │ │                                 │
       │ └─ gIsInSubMenu: true            │
       │    ├─ UP/DOWN → validate & adjust│
       │    │            gSubMenuSelection │
       │    ├─ MENU → gFlagAcceptSetting  │
       │    │         gIsInSubMenu=false   │
       │    └─ EXIT → discard changes     │
       └────────┬──────────────────────────┘
                │ (return)
                │
       ┌────────▼──────────────────────────┐
       │ ProcessKey() Continuation          │
       │                                   │
       │ if (gFlagAcceptSetting)           │
       │   MENU_AcceptSetting()            │
       │   ├─ Validate value              │
       │   ├─ Update EEPROM/gTxVfo (RAM)  │
       │   └─ Set save flags              │
       │        (gRequestSaveChannel, ...) │
       │                                   │
       │ if (gRequestSave*)               │
       │   if !bKeyHeld                   │
       │     SETTINGS_Save*(...)          │
       │     └─ FLASH_WriteBuffer()       │
       │   else                            │
       │     Set deferred save flag       │
       └────────────────────────────────────┘
                          │
                          ▼
      ┌─────────────────────────────────────┐
      │  Flash operation (EEPROM/Flash IC)  │
      │                                     │
      │  - Timing: 10-100ms per page        │
      │  - May stall CPU during write       │
      │  - Persists across power cycle      │
      └─────────────────────────────────────┘
```

---

## Key Takeaways

1. **Input → Display (immediate):** Button press updates RAM and display within 1 frame
2. **Edit → Preview (immediate):** Values shown live while editing, before confirm
3. **Confirm → RAM (immediate):** MENU button updates RAM copy instantly
4. **RAM → Flash (deferred):** EEPROM write batched, may wait for key release
5. **Flash latency:** Up to 100ms write time, processor may stall
6. **Menu timeout:** Resets counter on each keystroke; exit returns to MAIN after idle
7. **Special saves:** Channel name, deletion, reset bypass deferred flag system

---

## Files Modified in Call Chain

| File | Functions | Role |
|------|-----------|------|
| `App/app/app.c` | `ProcessKey()` | Main dispatcher, save handler |
| `App/app/menu.c` | `MENU_ProcessKeys()` `MENU_AcceptSetting()` `MENU_ShowCurrentSetting()` | Menu state machine |
| `App/ui/menu.h` | Menu constants/enums | Menu item definitions |
| `App/ui/ui.c` | `GUI_SelectNextDisplay()` | Display transition cleanup |
| `App/settings.h` `settings.c` | `SETTINGS_SaveChannel()` `SETTINGS_SaveSettings()` etc. | Flash persistence |
| `App/misc.h` | Global variable declarations | State tracking |

---

## Example: Complete Flow for Changing Squelch Level

```
┌─ User in DISPLAY_MENU, viewing "SQL" menu item
│
├─► Press MENU (enter edit mode)
│   └─► gIsInSubMenu = true
│       gSubMenuSelection = current SQL value from EEPROM
│       gUpdateDisplay = true
│
├─► Press UP key (increase squelch)
│   └─► MENU_ProcessKeys() adjusts gSubMenuSelection += 1
│       Validates against MENU_GetLimits(MENU_SQL) → max=9
│       gUpdateDisplay = true
│       (NO save request)
│
├─► Press MENU (confirm)
│   └─► MENU_ProcessKeys() sets gFlagAcceptSetting=true
│       
│   Back in ProcessKey():
│   ├─► MENU_AcceptSetting()
│   │   └─► case MENU_SQL:
│   │       └─► gEeprom.SQUELCH_LEVEL = gSubMenuSelection
│   │           gVfoConfigureMode = VFO_CONFIGURE
│   │           return  (no save flag set!)
│   │
│   └─► if (gVfoConfigureMode != NONE)
│       └─► gRequestDisplayScreen = DISPLAY_MAIN
│           (SQL changes apply immediately via RADIO_SetupRegisters)
│
├─► Exit menu (timeout or EXIT key)
│   └─► GUI_SelectNextDisplay(DISPLAY_MAIN)
│       └─► Menu state cleared
│
└─► SQL level change: APPLIED IMMEDIATELY (no flash write)
    └─► (SQUELCH is RAM-only setting, not persisted)
```

**Special note:** Squelch has no `gRequestSaveChannel` flag, so it's applied 
without EEPROM write. Other settings like OUTPUT_POWER do set the flag.

---

## Example: Complete Flow for Changing TX Power

```
┌─ User in DISPLAY_MENU, viewing "TXP" menu item
│
├─► Press MENU (enter edit)
│   └─► gIsInSubMenu = true
│       gSubMenuSelection = current gTxVfo->OUTPUT_POWER
│
├─► Press UP key (higher power)
│   └─► gSubMenuSelection adjusted and validated
│
├─► Press MENU (confirm)
│   └─► MENU_ProcessKeys() sets gFlagAcceptSetting=true
│       
│   Back in ProcessKey():
│   ├─► MENU_AcceptSetting()
│   │   └─► case MENU_TXP:
│   │       ├─► gTxVfo->OUTPUT_POWER = gSubMenuSelection
│   │       ├─► gRequestSaveChannel = 1  ◄─── SAVE FLAG SET
│   │       └─► return
│   │
│   └─► if (gRequestSaveChannel > 0)
│       ├─► if (!bKeyHeld && !bKeyPressed) ◄─── No hold
│       │   ├─► SETTINGS_SaveChannel(...)
│       │   │   └─► EEPROM_WriteBuffer() [blocking]
│       │   └─► gRequestSaveChannel = 0
│       │
│       └─► else if (bKeyHeld) ◄─── If still pressing
│           └─► flagSaveChannel = gRequestSaveChannel
│               (Defer until release)
│
├─► When key released:
│   ├─► If deferred flag set:
│   │   └─► SETTINGS_SaveChannel() executes
│   └─► gRequestSaveChannel cleared
│
└─► TX Power: SAVED TO FLASH ✓
    └─► Persists across restart
```

---

End of Call Chain Documentation
