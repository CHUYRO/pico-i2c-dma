#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include "i2c_dma.h"
#include "mprintf.h"

static const uint8_t MCP9808_ADDR = 0x18;
static const uint8_t MCP9808_TEMP_REG = 0x05;

// After power-up the MCP9808 typically requires 250 ms to perform the first
// conversion at the power-up default resolution. See datasheet.
static const int32_t MCP9808_POWER_UP_DELAY_MS = 300;

static void blink_led_task(void *args) {
  (void) args;

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, 1);
  gpio_put(PICO_DEFAULT_LED_PIN, !PICO_DEFAULT_LED_PIN_INVERTED);

  while (true) {
    gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

static double mcp9808_raw_temp_to_celsius(uint16_t raw_temp) {
  double celsius = (raw_temp & 0x0fff) / 16.0;

  if (raw_temp & 0x1000) {
    celsius -= 256;
  }

  return celsius;
}

static void mcp9808_task(void *args) {
  i2c_dma_t *i2c_dma = (i2c_dma_t *) args;

  vTaskDelay(pdMS_TO_TICKS(MCP9808_POWER_UP_DELAY_MS));

  for (int err_cnt = 0, i = 0; true; i += 1) {
    uint16_t raw_temp;
    const int rc = i2c_dma_read_word_swapped(
      i2c_dma, MCP9808_ADDR, MCP9808_TEMP_REG, &raw_temp
    );

    if (rc != PICO_OK) {
      err_cnt += 1;
      mprintf("error (i: %d, rc: %d, errors: %d)\n", i, rc, err_cnt);
    } else {
      const double celsius = mcp9808_raw_temp_to_celsius(raw_temp);
      mprintf("temp: %.4f (i: %d, errors: %d)\n", celsius, i, err_cnt);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

int main(void) {
  stdio_init_all();

  static i2c_dma_t *i2c0_dma;
  const int rc = i2c_dma_init(&i2c0_dma, i2c0, (1000 * 1000), 4, 5);
  if (rc != PICO_OK) {
    mprintf("can't configure I2C0\n");
    return rc;
  }

  xTaskCreate(
    blink_led_task,
    "blink-led-task",
    configMINIMAL_STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 2,
    NULL
  );

  xTaskCreate(
    mcp9808_task,
    "mcp9808-task",
    configMINIMAL_STACK_SIZE,
    i2c0_dma,
    configMAX_PRIORITIES - 2,
    NULL
  );

  vTaskStartScheduler();
}

