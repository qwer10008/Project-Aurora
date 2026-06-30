#pragma once

// 初始化 I2S 喇叭
void audio_init(void);

// 播放频率为 freq_hz 的正弦波，持续 duration_ms 毫秒（阻塞）
void audio_play_tone(int freq_hz, int duration_ms);
