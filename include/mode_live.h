#pragma once

namespace ModeLive {
  void begin(bool trainMode = true);
  void render();
  void handleEncoderDelta(int delta);
  void handleEncoderButton();
}
