#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define LED0_NODE  DT_ALIAS(led0)
#define PIR0_NODE  DT_NODELABEL(pir0)
#define PIR1_NODE  DT_NODELABEL(pir1)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec pir0 = GPIO_DT_SPEC_GET(PIR0_NODE, gpios);
static const struct gpio_dt_spec pir1 = GPIO_DT_SPEC_GET(PIR1_NODE, gpios);

static struct gpio_callback pir0_cb;
static struct gpio_callback pir1_cb;

static void pir_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
    gpio_pin_toggle_dt(&led0);
    LOG_INF("PIR triggered → LED toggled");
}

int main(void)
{
    int err;

    if (!device_is_ready(led0.port) || !device_is_ready(pir0.port) || !device_is_ready(pir1.port)) {
        LOG_ERR("GPIO device(s) not ready");
        return 0;
    }

    err = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
    if (err) {
        LOG_ERR("LED config failed (%d)", err);
        return 0;
    }

    // PIR0 setup
    err = gpio_pin_configure_dt(&pir0, GPIO_INPUT);
    if (!err) err = gpio_pin_interrupt_configure_dt(&pir0, GPIO_INT_EDGE_TO_ACTIVE);
    if (err) {
        LOG_ERR("PIR0 config/irq failed (%d)", err);
        return 0;
    }
    gpio_init_callback(&pir0_cb, pir_isr, BIT(pir0.pin));
    gpio_add_callback(pir0.port, &pir0_cb);

    // PIR1 setup
    err = gpio_pin_configure_dt(&pir1, GPIO_INPUT);
    if (!err) err = gpio_pin_interrupt_configure_dt(&pir1, GPIO_INT_EDGE_TO_ACTIVE);
    if (err) {
        LOG_ERR("PIR1 config/irq failed (%d)", err);
        return 0;
    }
    gpio_init_callback(&pir1_cb, pir_isr, BIT(pir1.pin));
    gpio_add_callback(pir1.port, &pir1_cb);

    LOG_INF("PIR sensors ready");

    while (1) {
        k_sleep(K_FOREVER);
    }
}