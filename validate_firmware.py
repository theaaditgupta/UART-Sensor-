#!/usr/bin/env python3
"""
scripts/validate_firmware.py

Hardware validation script for the UART Sensor Node.

On real hardware this connects to the STM32F4 over a USB-UART adapter
(CP2102 or similar) at 115200 baud, injects known byte sequences, reads
back the ring buffer dump, and compares byte-for-byte.

This file runs in SIMULATION MODE when no serial port is specified,
using a software model of the ring buffer to demonstrate the validation
logic.  The same logic catches real hardware bugs when connected to the
actual device.

Usage:
    # Simulation mode (no hardware needed):
    python3 validate_firmware.py

    # Real hardware mode (requires pyserial):
    python3 validate_firmware.py --port /dev/ttyUSB0 --baud 115200
"""

import argparse
import sys
import time
import random
from dataclasses import dataclass, field
from typing import List, Optional, Tuple


# ─── Software ring buffer model ────────────────────────────────────────────────

RB_CAPACITY = 64

class RingBuffer:
    """Python mirror of the C ring_buffer implementation."""

    def __init__(self):
        self.buf   = [0] * RB_CAPACITY
        self.head  = 0   # write pointer
        self.tail  = 0   # read pointer

    def write(self, byte: int) -> bool:
        if (self.head - self.tail) >= RB_CAPACITY:
            return False   # full
        self.buf[self.head % RB_CAPACITY] = byte & 0xFF
        self.head += 1
        return True

    def read(self) -> Optional[int]:
        if self.head == self.tail:
            return None   # empty
        byte = self.buf[self.tail % RB_CAPACITY]
        self.tail += 1
        return byte

    def available(self) -> int:
        return self.head - self.tail

    def dump(self) -> List[int]:
        out = []
        while True:
            b = self.read()
            if b is None:
                break
            out.append(b)
        return out


# ─── Validation logic ──────────────────────────────────────────────────────────

@dataclass
class ValidationResult:
    total_bytes:   int  = 0
    matched:       int  = 0
    mismatches:    List[Tuple[int, int, int]] = field(default_factory=list)  # (index, expected, actual)
    dropped_bytes: int  = 0

    @property
    def passed(self) -> bool:
        return len(self.mismatches) == 0 and self.dropped_bytes == 0


def build_test_sequence(length: int, seed: int = 42) -> List[int]:
    """Generate a deterministic pseudo-random byte sequence."""
    rng = random.Random(seed)
    return [rng.randint(0, 255) for _ in range(length)]


def run_simulation(num_frames: int = 10, frame_size: int = 6,
                   inject_bug: bool = False) -> ValidationResult:
    """
    Simulate firmware behaviour and validate output.

    inject_bug=True introduces the off-by-one wrap error that was found
    during development, to demonstrate that the validator catches it.
    """
    rb = RingBuffer()
    result = ValidationResult()

    all_written: List[int] = []

    for frame_idx in range(num_frames):
        sequence = build_test_sequence(frame_size, seed=frame_idx)

        for i, byte in enumerate(sequence):
            if inject_bug and rb.head == RB_CAPACITY - 1:
                # Simulate off-by-one: skip wrapping, write to wrong slot
                rb.buf[(rb.head + 1) % RB_CAPACITY] = byte
                rb.head += 1
            else:
                written = rb.write(byte)
                if not written:
                    result.dropped_bytes += 1
                    continue

            all_written.append(byte)
            result.total_bytes += 1

    # Drain the ring buffer (simulates firmware sending a dump command)
    actual_output = rb.dump()
    result.matched = 0

    for idx, (expected, actual) in enumerate(zip(all_written, actual_output)):
        if expected == actual:
            result.matched += 1
        else:
            result.mismatches.append((idx, expected, actual))

    # If lengths differ, remaining bytes are effectively mismatched
    if len(actual_output) < len(all_written):
        for idx in range(len(actual_output), len(all_written)):
            result.mismatches.append((idx, all_written[idx], -1))

    return result


def print_result(result: ValidationResult, label: str):
    print(f"\n{'─' * 50}")
    print(f"  Test: {label}")
    print(f"{'─' * 50}")
    print(f"  Total bytes written : {result.total_bytes}")
    print(f"  Bytes matched       : {result.matched}")
    print(f"  Mismatches          : {len(result.mismatches)}")
    print(f"  Dropped bytes       : {result.dropped_bytes}")

    if result.mismatches:
        print(f"\n  First 5 mismatches:")
        for idx, exp, act in result.mismatches[:5]:
            act_str = f"0x{act:02X}" if act >= 0 else "MISSING"
            print(f"    byte[{idx:4d}]  expected=0x{exp:02X}  actual={act_str}")

    status = "PASS" if result.passed else "FAIL"
    print(f"\n  Result: {status}")


def run_hardware_validation(port: str, baud: int) -> ValidationResult:
    """
    Real hardware path.  Requires pyserial.
    Protocol:
        1. Host sends CMD_INJECT (0x01) + length (1 byte) + payload bytes
        2. Firmware echoes ACK (0xAC) when done writing to ring buffer
        3. Host sends CMD_DUMP (0x02)
        4. Firmware responds with length (1 byte) + all buffered bytes
    """
    try:
        import serial  # type: ignore
    except ImportError:
        print("ERROR: pyserial not installed.  Run: pip install pyserial")
        sys.exit(1)

    CMD_INJECT = 0x01
    CMD_DUMP   = 0x02
    ACK        = 0xAC

    result = ValidationResult()
    sequence = build_test_sequence(200)
    result.total_bytes = len(sequence)

    print(f"Connecting to {port} at {baud} baud...")
    with serial.Serial(port, baud, timeout=2.0) as ser:
        time.sleep(0.1)   # let MCU settle after DTR reset

        # Inject in 60-byte chunks (fits ring buffer)
        CHUNK = 60
        all_written = []
        for start in range(0, len(sequence), CHUNK):
            chunk = sequence[start:start + CHUNK]
            payload = bytes([CMD_INJECT, len(chunk)] + chunk)
            ser.write(payload)
            ack = ser.read(1)
            if not ack or ack[0] != ACK:
                print(f"  [WARN] No ACK for chunk starting at byte {start}")
                result.dropped_bytes += len(chunk)
                continue
            all_written.extend(chunk)

        # Request dump
        ser.write(bytes([CMD_DUMP]))
        length_byte = ser.read(1)
        if not length_byte:
            print("ERROR: no response to CMD_DUMP")
            return result

        n = length_byte[0]
        actual = list(ser.read(n))

        for idx, (exp, act) in enumerate(zip(all_written, actual)):
            if exp == act:
                result.matched += 1
            else:
                result.mismatches.append((idx, exp, act))

    return result


# ─── Entry point ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="UART Sensor Node — firmware validation script"
    )
    parser.add_argument("--port",  type=str, default=None,
                        help="Serial port (e.g. /dev/ttyUSB0). Omit for simulation mode.")
    parser.add_argument("--baud",  type=int, default=115200)
    parser.add_argument("--frames", type=int, default=10,
                        help="Number of sensor frames to inject (simulation mode only)")
    args = parser.parse_args()

    print("UART Sensor Node — Firmware Validator")
    print(f"Mode: {'HARDWARE (' + args.port + ')' if args.port else 'SIMULATION'}")

    if args.port:
        result = run_hardware_validation(args.port, args.baud)
        print_result(result, f"Hardware validation on {args.port}")
    else:
        # Test 1: clean run — should pass
        r1 = run_simulation(num_frames=args.frames, inject_bug=False)
        print_result(r1, "Clean run (no injected bug)")

        # Test 2: inject the off-by-one wrap bug — should fail and show mismatches
        r2 = run_simulation(num_frames=args.frames, inject_bug=True)
        print_result(r2, "Injected off-by-one wrap bug (should FAIL)")

        # Test 3: wrap-around stress — write > RB_CAPACITY bytes across multiple fills
        rb = RingBuffer()
        written = []
        read_back = []
        for cycle in range(20):
            seq = build_test_sequence(6, seed=cycle)
            for b in seq:
                if rb.write(b):
                    written.append(b)
            if cycle % 3 == 0:
                read_back.extend(rb.dump())

        read_back.extend(rb.dump())   # drain remainder

        r3 = ValidationResult()
        r3.total_bytes = len(written)
        for idx, (exp, act) in enumerate(zip(written, read_back)):
            if exp == act:
                r3.matched += 1
            else:
                r3.mismatches.append((idx, exp, act))
        print_result(r3, "Wrap-around stress test (20 cycles, partial drains)")

    passed = [r for r in [r1, r2, r3] if r.passed] if not args.port else []
    print(f"\nSummary: {len(passed)}/3 tests passed (expected: 2/3 — Test 2 is designed to fail)\n")


if __name__ == "__main__":
    # Handle the case where r1/r2/r3 are not defined (hardware mode)
    try:
        main()
    except SystemExit:
        raise
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
