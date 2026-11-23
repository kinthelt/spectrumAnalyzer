// Boot-time information about the board, logged once at startup.
#pragma once

// Human-readable reason for the last reset ("power-on", "brownout", ...).
const char* resetReasonText();

// Logs firmware version, reset reason, PSRAM and flash size.
void logBootInfo();
