# MINT Final Channel 0.1.1

This update makes the master bypass state immediately recognizable.

## Changed

- Replaced the ambiguous `LIVE / BYPASS` toggle with a hardware-style latching
  button shared by the Simple and Formal views.
- A green LED and `ACTIVE / PROCESSING` indicate that the processing chain is
  enabled.
- A red LED and `BYPASSED / DRY SIGNAL` indicate true dry-signal bypass.
- Chinese Simple mode displays `启用 / 正在处理` and `已旁路 / 原始信号`.
- The pressed state has a physical depth change and the full bezel changes
  color, so the state is visible without relying on small text.

## Validation

- DSP bypass transparency test: passed.
- pluginval GUI and state validation: passed.
- Installed-bundle host scan: passed.
