#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define NUM_PAIRS 5

#define LED_NODE(i)   DT_ALIAS(led##i)
#define PIR_NODE(i)   DT_NODELABEL(pir##i)

typedef struct {
    struct gpio_dt_spec led;
    struct gpio_dt_spec pir;
    struct gpio_callback pir_cb;
    struct k_work_delayable led_off_work;
    int idx;
} pir_led_pair_t;

static pir_led_pair_t pairs[NUM_PAIRS];

static void led_off_work_fn(struct k_work *work)
{
    pir_led_pair_t *pair = CONTAINER_OF(work, pir_led_pair_t, led_off_work.work);
    gpio_pin_set_dt(&pair->led, 0);
    LOG_INF("LED %d turned off after timeout", pair->idx);
}

static void pir_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    pir_led_pair_t *pair = CONTAINER_OF(cb, pir_led_pair_t, pir_cb);
    bool active = gpio_pin_get_dt(&pair->pir);
    if (active) {
        gpio_pin_set_dt(&pair->led, 1);
        LOG_INF("PIR %d active → LED %d ON", pair->idx, pair->idx);
        k_work_cancel_delayable(&pair->led_off_work);
    } else {
        k_work_schedule(&pair->led_off_work, K_SECONDS(3));
        LOG_INF("PIR %d inactive → LED %d will turn OFF in 3s", pair->idx, pair->idx);
    }
}

int main(void)
{
    int err;
    const struct gpio_dt_spec leds[NUM_PAIRS] = {
        GPIO_DT_SPEC_GET(LED_NODE(0), gpios),
        GPIO_DT_SPEC_GET(LED_NODE(1), gpios),
        GPIO_DT_SPEC_GET(LED_NODE(2), gpios),
        GPIO_DT_SPEC_GET(LED_NODE(3), gpios),
        GPIO_DT_SPEC_GET(LED_NODE(4), gpios),
    };
    const struct gpio_dt_spec pirs[NUM_PAIRS] = {
        GPIO_DT_SPEC_GET(PIR_NODE(0), gpios),
        GPIO_DT_SPEC_GET(PIR_NODE(1), gpios),
        GPIO_DT_SPEC_GET(PIR_NODE(2), gpios),
        GPIO_DT_SPEC_GET(PIR_NODE(3), gpios),
        GPIO_DT_SPEC_GET(PIR_NODE(4), gpios),
    };

    for (int i = 0; i < NUM_PAIRS; i++) {
        pairs[i].led = leds[i];
        pairs[i].pir = pirs[i];
        pairs[i].idx = i;

        if (!device_is_ready(pairs[i].led.port) || !device_is_ready(pairs[i].pir.port)) {
            LOG_ERR("GPIO device(s) for pair %d not ready", i);
            return 0;
        }
        err = gpio_pin_configure_dt(&pairs[i].led, GPIO_OUTPUT_INACTIVE);
        if (err) {
            LOG_ERR("LED %d config failed (%d)", i, err);
            return 0;
        }
        err = gpio_pin_configure_dt(&pairs[i].pir, GPIO_INPUT | GPIO_PULL_DOWN);
        if (!err) err = gpio_pin_interrupt_configure_dt(&pairs[i].pir, GPIO_INT_EDGE_BOTH);
        if (err) {
            LOG_ERR("PIR %d config/irq failed (%d)", i, err);
            return 0;
        }
        gpio_init_callback(&pairs[i].pir_cb, pir_isr, BIT(pairs[i].pir.pin));
        gpio_add_callback(pairs[i].pir.port, &pairs[i].pir_cb);
        k_work_init_delayable(&pairs[i].led_off_work, led_off_work_fn);
    }
    LOG_INF("PIR/LED pairs ready");
    while (1) {
        k_sleep(K_FOREVER);
    }
}