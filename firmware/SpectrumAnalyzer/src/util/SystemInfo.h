// Boot-time information about the board, logged once at startup.
#pragma once

// Human-readable reason for the last reset ("power-on", "brownout", ...).
const char* resetReasonText();

// The last reset was the power coming on (or dipping): power-on or brownout.
bool resetWasPowerUp();
// The last reset was a crash or a watchdog.
bool resetWasCrash();

// Logs firmware version, reset reason, PSRAM and flash size.
void logBootInfo();

// Logs internal RAM at a point in start-up: free now, the largest single free
// block (fragmentation), and the lowest since start-up. Matter and the
// encryption it uses need internal RAM; PSRAM can't stand in for it.
void logMemory(const char* stage);
