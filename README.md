Dự án triển khai thuật toán điều khiển động cơ BLDC vòng hở (Open-Loop) sử dụng vi điều khiển STM32.

## Kỹ Thuật Phần Cứng & Ngoại Vi Áp Dụng
* **6-Step Commutation (LUT):** Sử dụng bảng tra trạng thái bằng mảng để chuyển mạch động cơ.
* **Hardware Trigger ADC:** Dùng sự kiện `TIM1_CC4` để kích hoạt bộ ADC1 tự động lấy mẫu.

## Sơ Đồ Chân:

| Ngoại vi | Chân MCU | Chức năng |
| :--- | :--- | :--- |
| **ADC1** | `PA3` | Đọc biến trở cấu hình **Dead-time** |
| **ADC1** | `PA4` | Đọc biến trở điều khiển Tốc độ chuyển step |
| **ADC1** | `PA5` | Đọc biến trở điều khiển **PWM** |
| **GPIO Input** | `PB0` | Nút nhấn chuyển **Start/Stop/Reset/OverStep(PB0+PB1)** |
| **GPIO Input** | `PB1` | Nút nhấn chuyển STEP(6) |
| **TIM1 (PWM High)** | `PA8, PA9, PA10` | Ngõ ra PWM Phase U, V, W (High-Side) |
| **GPIO Output** | `PB13, PB14, PB15`| U, V, W (Low-Side) |

```c
// Bảng tra LUT 6 bước chuyển mạch BLDC mẫu
const uint8_t BLDC_LUT[6] = { ... };

// Cấu hình Timer Trigger cho ADC trong hàm Init
ADC_CR2_JEXTTRIG; // Kích hoạt ngắt ngoại vi cho Injected Channels
ADC_CR2_JEXTSEL_0; // Chọn TIM1_CC4 làm nguồn Trigger
