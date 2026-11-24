#include "BootValidator.h"

void BootValidator::begin(bool onTrial, uint32_t nowMillis) {
  onTrial_ = onTrial;
  startMillis_ = nowMillis;
  healthy_ = false;
}

BootValidator::Action BootValidator::update(bool healthy, uint32_t nowMillis) {
  if (!onTrial_) return Action::None;
  if (healthy && !healthy_) healthySince_ = nowMillis;   // a new unbroken stretch starts
  healthy_ = healthy;
  if (healthy_ && nowMillis - healthySince_ >= goodAfter_) {
    onTrial_ = false;
    return Action::MarkGood;
  }
  if (nowMillis - startMillis_ >= giveUp_) {
    onTrial_ = false;
    return Action::RollBack;
  }
  return Action::None;
}
