#include "CrashGuard.h"

#include "../config/Config.h"

void CrashGuard::begin(bool crashed) {
  if (s_.magic != kMagic) {   // first start after a power cut
    s_ = State{kMagic, 0, 0};
    return;
  }
  if (s_.onTrial && crashed) {
    if (s_.crashes < 255) ++s_.crashes;
  } else if (s_.onTrial) {
    s_.crashes = 0;   // restarted on purpose (update, new strip setup) before it was stable
  }
  s_.onTrial = 0;
}

bool CrashGuard::allowed() const { return s_.crashes < config::kMatterCrashesToPause; }

void CrashGuard::starting() { s_.onTrial = 1; }

void CrashGuard::stable() {
  s_.onTrial = 0;
  s_.crashes = 0;
}

void CrashGuard::reset() { s_ = State{kMagic, 0, 0}; }
