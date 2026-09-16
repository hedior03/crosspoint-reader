"""Fix the pinned PDM reader and expose overflow for the PaperMono capture test."""
from pathlib import Path

Import("env")
root = Path(env["PROJECT_DIR"]) / "freeink-sdk/libs/hardware/Microphone"

def replace(path, old, new):
    source = path.read_text()
    if source.count(new) == 1:
        return
    if source.count(old) == 1:
        path.write_text(source.replace(old, new))
    else:
        raise RuntimeError(f"Microphone SDK changed: review patch for {path}")

header = root / "include/Microphone.h"
source = root / "src/Microphone.cpp"
replace(header, "  uint32_t sampleRate() const", "  bool overflowed() const;\n\n  uint32_t sampleRate() const")
replace(source, "namespace {\n// Assert", """namespace {
// One PDM capture owner; ISR state stays in internal RAM.
DRAM_ATTR portMUX_TYPE captureMux = portMUX_INITIALIZER_UNLOCKED;
DRAM_ATTR volatile bool captureOverflow = false;
bool IRAM_ATTR onOverflow(i2s_chan_handle_t, i2s_event_data_t*, void* user) {
  (void)user;
  portENTER_CRITICAL_ISR(&captureMux);
  captureOverflow = true;
  portEXIT_CRITICAL_ISR(&captureMux);
  return false;
}
// Assert""")
replace(source, "  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);", """  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
#if FREEINK_DEVICE_PAPERMONO
  chanCfg.id = I2S_NUM_0;
#endif
  portENTER_CRITICAL(&captureMux);
  captureOverflow = false;
  portEXIT_CRITICAL(&captureMux);""")
replace(source, "  if (i2s_channel_init_pdm_rx_mode(rx, &pdmCfg) != ESP_OK || i2s_channel_enable(rx) != ESP_OK) {", """#if FREEINK_DEVICE_PAPERMONO
  pdmCfg.slot_cfg.slot_mask = I2S_PDM_SLOT_RIGHT;
#endif
  i2s_event_callbacks_t callbacks{};
  callbacks.on_recv_q_ovf = onOverflow;
  if (i2s_channel_init_pdm_rx_mode(rx, &pdmCfg) != ESP_OK ||
      i2s_channel_register_event_callback(rx, &callbacks, nullptr) != ESP_OK ||
      i2s_channel_enable(rx) != ESP_OK) {""")
replace(source, "&bytesRead, pdMS_TO_TICKS(timeoutMs));\n  if (err == ESP_ERR_TIMEOUT) return 0;\n  if (err != ESP_OK) return -1;", "&bytesRead, timeoutMs);\n  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) return -1;")

replace(source, "bool Microphone::begin(uint32_t sampleRate) {", "bool Microphone::overflowed() const {\n  portENTER_CRITICAL(&captureMux);\n  const bool overflow = captureOverflow;\n  portEXIT_CRITICAL(&captureMux);\n  return overflow;\n}\n\nbool Microphone::begin(uint32_t sampleRate) {")
replace(source, "bool Microphone::begin(uint32_t) { return false; }", "bool Microphone::overflowed() const { return false; }\nbool Microphone::begin(uint32_t) { return false; }")
