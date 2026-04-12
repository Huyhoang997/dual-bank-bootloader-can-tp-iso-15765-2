# STM32 Dual-Bank Bootloader (CAN Bus Version)

A custom bootloader for STM32 microcontrollers implementing a dual-bank (A/B) firmware update strategy over **CAN Bus** with CRC32 verification and rollback support.

---

## Features

- Dual-bank firmware storage (App A & App B)
- CAN Bus-based firmware transfer (8-byte packets per CAN frame)
- 1024-byte write buffer with word-aligned flash programming
- CRC32 integrity verification (hardware CRC unit)
- Atomic bank switching with metadata persistence
- One-click rollback to previous firmware version
- Pending flag mechanism with CRC re-validation on boot
- Hardware GPIO trigger to enter bootloader mode

---

## Flash Memory Layout

### Bootloader & Metadata Sector

> Stores the bootloader code. Runs first on every power-on or reset.

<img width="1596" height="330" alt="image" src="https://github.com/user-attachments/assets/1e2e9442-6081-4f7a-bc97-4ac6e8d0ee69" />

---

### Application A

> Firmware bank A — 15 flash pages.

<img width="1614" height="348" alt="image" src="https://github.com/user-attachments/assets/ec9b2e93-11e2-4a32-a5ff-09692fd3965d" />


---

### Application B

> Firmware bank B — 15 flash pages.

<img width="1608" height="305" alt="image" src="https://github.com/user-attachments/assets/1494c1b5-a8bd-41b2-93df-cdb1bca24a55" />


---

## Metadata Structures

```c
typedef struct {
    uint32_t Hostsize_AppA;     // Size of App A
    uint32_t Hostcrc_AppA;      // CRC32 of App A
    uint32_t version_AppA;      // Version of App A
    uint32_t Hostsize_AppB;     // Size of App B
    uint32_t Hostcrc_AppB;      // CRC32 of App B
    uint32_t version_AppB;      // Version of App B
    uint32_t host_cmd;          // Last received command
} METADATA_SectorDef_t;

typedef struct {
    uint32_t activeBank;        // Currently active bank (ACTIVE_APP_A / ACTIVE_APP_B)
    uint32_t pFlag;             // 1 = new firmware pending, 0 = stable
} METADATA_ActiveBank_t;
```

---

## Behavior

### Boot Flow

<img width="1440" height="1568" alt="image" src="https://github.com/user-attachments/assets/36422e0b-8847-45b7-b4fd-0609ad9e66d4" />
On every power-on or reset, the bootloader runs the following logic:

1. Check GPIO PA0 — if LOW, enter Bootloader Mode and wait for a CAN command.
2. Read `activeBank` and `pFlag` from the Metadata Active Bank sector.
3. If `pFlag == 0`, jump directly to the currently active bank.
4. If `pFlag == 1`, re-verify CRC of the pending firmware:
   - If CRC passes → switch bank, clear flag, jump to new firmware.
   - If CRC fails → rollback, jump to current (old) firmware.
Power ON / Reset
│
▼
GPIO PA0 LOW? ──YES──► Enter Bootloader Mode (CAN command wait)
│
NO
▼
Read activeBank + pFlag from Metadata
│
├── pFlag == 0 ──────────────────────────────────► Jump to active bank
│
└── pFlag == 1 ──► Re-verify CRC of opposite bank
│
CRC OK ───┤──► Switch bank → Clear flag → Jump to new firmware
│
CRC FAIL ─┴──► Keep current bank → Jump to old firmware

![Boot Flow Diagram](images/boot_flow.png)

---

### Firmware Update Flow

1. Pull GPIO PA0 LOW before reset to enter bootloader mode.
2. Send a CAN frame with command byte `BOOTLOADER_WRITE_CMD` and Info Frame fields in the same message.
3. The bootloader saves the Info Frame to the Metadata sector, erases the inactive bank, and sends `CAN_START`.
4. Send firmware data in 8-byte CAN frames continuously.
5. When the 1024-byte write buffer is full, the bootloader sends `CAN_ACK_BUSY`, flushes to flash, then sends `CAN_ACK_OK`.
6. Receive `CAN_DONE` when all packets are transferred.
7. On next boot, the bootloader re-verifies CRC, sets `pFlag = 1`, switches bank, and jumps to the new firmware.
Host                         STM32 Bootloader
│                               │
│──── CMD_WRITE + Info ────────►│
│                               │ (save metadata, erase target bank)
│◄─── CAN_START ─────────────── │
│──── CAN Frame 1 (8 bytes) ───►│
│──── CAN Frame 2 (8 bytes) ───►│
│         ... (128 frames)      │
│◄─── CAN_ACK_BUSY ──────────── │ (flushing 1024 bytes to flash)
│◄─── CAN_ACK_OK ─────────────  │
│         ... (repeat)          │
│◄─── CAN_DONE ───────────────  │
│                               │ (on next boot: CRC verify → set pFlag=1 → switch bank)

#### Demo — Entering Bootloader & Sending Firmware

> Device running current firmware → GPIO PA0 pulled LOW → reset → enter bootloader mode → receive new firmware via CAN.

<img width="600" height="600" alt="image" src="https://github.com/user-attachments/assets/dd909f94-f430-48a9-91e0-e08decb0ee5c" />


#### Demo — Firmware Received & CRC Verified

> New firmware fully received, CRC check passed, pending flag set.

<img width="600" height="600" alt="image" src="https://github.com/user-attachments/assets/0bf62c7c-f602-4278-9ad9-ac639a4d733d" />


#### Demo — Jump to New Firmware

> On next boot, bootloader detects pending flag, switches bank, jumps to new application.

<img width="600" height="600" alt="image" src="https://github.com/user-attachments/assets/6b0be76b-a3b4-424a-a9eb-e791a9f24c25" />


---

### Rollback Flow

1. Pull GPIO PA0 LOW before reset to enter bootloader mode.
2. Send command byte `BOOTLOADER_ERASE_CMD` via CAN.
3. The bootloader erases the currently active bank's sectors.
4. Switches `activeBank` to the other bank.
5. On next boot, automatically jumps to the older firmware.

<img width="600" height="600" alt="image" src="https://github.com/user-attachments/assets/75335b09-84ce-40f3-8989-1305ca94ac73" />

<img width="600" height="600" alt="image" src="https://github.com/user-attachments/assets/20e1060a-83ee-429b-8d4f-d2c147d78540" />



---

## CAN Bus Protocol

### CAN Configuration

| Parameter | Value |
|---|---|
| Host CAN ID | `0x34` |
| Bootloader Std ID | `0x123` |
| RX FIFO | FIFO 0 |
| Frame size | 8 bytes max (standard CAN DLC) |

### Commands

| Command Byte | Field | Action |
|---|---|---|
| `BOOTLOADER_WRITE_CMD` | `rxdata[0]` | Start firmware update |
| `BOOTLOADER_ERASE_CMD` | `rxdata[0]` | Erase current firmware & rollback |

### Info Frame Format (packed into CAN data bytes, sent with CMD_WRITE)

| Byte(s) | Field | Description |
|---|---|---|
| [0] | Command | `BOOTLOADER_WRITE_CMD` |
| [1:2] | Size | Firmware size in bytes (big-endian) |
| [3:6] | CRC32 | Expected CRC32 of firmware (big-endian) |
| [7] | Version | Firmware version |

### ACK Codes

| Code | Meaning |
|---|---|
| `CAN_START` | Ready to receive firmware |
| `CAN_ACK_BUSY` | Write buffer full, flushing to flash |
| `CAN_ACK_OK` | Flush complete, ready for next batch |
| `CAN_DONE` | All firmware received |

---

## API Reference

| Function | Description |
|---|---|
| `BOOTLOADER_Init()` | Entry point — checks boot mode and jumps to the correct app |
| `BOOTLOADER_ReceiveCmd()` | Waits for a CAN command and dispatches handler |
| `BOOTLOADER_SaveInfoFrame()` | Saves firmware metadata from Info Frame to flash |
| `BOOTLOADER_ReceiveFirmware()` | Receives firmware via CAN and writes to inactive bank |
| `BOOTLOADER_CheckFirmwareCRC()` | CRC32 verification of received firmware; sets pending flag on pass |
| `BOOTLOADER_SetActiveBank()` | Persists active bank selection to Metadata Active Bank sector |
| `BOOTLOADER_SetPendingFlag()` | Sets or clears the pending update flag |
| `BOOTLOADER_EraseCurrentFirmware()` | Erases active firmware and switches bank for rollback |
| `BOOTLOADER_SendACK()` | Sends a CAN ACK frame back to the host |

---

## Return Codes

| Code | Meaning |
|---|---|
| `BOOTLOADER_OK` | Operation successful |
| `BOOTLOADER_ERR` | Generic error |
| `BOOTLOADER_ERASE_ERR` | Flash erase failed |
| `BOOTLOADER_PROGRAM_ERR` | Flash write failed |
| `BOOTLOADER_CHECK_CRC_FAILD` | CRC32 mismatch after transfer |

---

## Hardware Requirements

| Peripheral | Usage |
|---|---|
| CAN | Firmware transfer (8-byte frames) |
| UART1 | Debug output (`mPrintf`) |
| GPIO PA0 | Bootloader mode trigger (active LOW) |
| GPIO PA1 | Bootloader mode indicator LED |
| CRC Unit | Hardware CRC32 calculation |
| Internal Flash | Firmware & metadata storage |

---

## LED Mapping

| LED | Color | Condition |
|-----|-------|-----------|
| PA1 | Green | ON when in Bootloader Mode |
| PA1 | Off | OFF when running application |

---

## Known Limitations

- No timeout handling on CAN receive — `HAL_CAN_GetRxFifoFillLevel` polling loop blocks indefinitely
- No retry mechanism on failed packet or CRC mismatch during transfer
- `static uint16_t index` in `BOOTLOADER_ReceiveFirmware()` is not reset between calls — may cause issues on repeated update attempts without reset
- `BOOTLOADER_SetActiveBank()` returns `BOOTLOADER_ERASE_ERR` on program failure (should be `BOOTLOADER_PROGRAM_ERR`)
- Remainder bytes after last full 1024-byte block are written without error checking

---

## License

MIT License — feel free to use and modify for your own STM32 projects.
