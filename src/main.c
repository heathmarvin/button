#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* Use board aliases so it “just works” on the nRF5340 DK */
#define LED0_NODE  DT_ALIAS(led0)
#define SW0_NODE   DT_ALIAS(sw0)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec sw0  = GPIO_DT_SPEC_GET(SW0_NODE,  gpios);

static struct gpio_callback sw0_cb;
static struct k_work_delayable heartbeat_work;

static void heartbeat_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    //gpio_pin_toggle_dt(&led0);
    /* Reschedule for a 500 ms heartbeat */
    k_work_schedule(&heartbeat_work, K_MSEC(500));
}

/* Toggle LED on button press and log it */
static void sw0_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
    gpio_pin_toggle_dt(&led0);
    LOG_INF("Button event → LED toggled");
}

int main(void)
{
    int err;

    if (!device_is_ready(led0.port) || !device_is_ready(sw0.port)) {
        LOG_ERR("GPIO device(s) not ready");
        return 0;
    }

    /* LED output (handles active-low via DT flags) */
    err = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
    if (err) {
        LOG_ERR("LED config failed (%d)", err);
        return 0;
    }

    /* Button input + interrupt on transition to ACTIVE
       (ACTIVE accounts for active-low hardware on the DK) */
    err = gpio_pin_configure_dt(&sw0, GPIO_INPUT);
    if (!err) err = gpio_pin_interrupt_configure_dt(&sw0, GPIO_INT_EDGE_TO_ACTIVE);
    if (err) {
        LOG_ERR("Button config/irq failed (%d)", err);
        return 0;
    }

    gpio_init_callback(&sw0_cb, sw0_isr, BIT(sw0.pin));
    gpio_add_callback(sw0.port, &sw0_cb);
    LOG_INF("Button ready (alias sw0); heartbeat starting");

    /* Start the non-blocking heartbeat */
    k_work_init_delayable(&heartbeat_work, heartbeat_fn);
    k_work_schedule(&heartbeat_work, K_MSEC(500));

    /* Idle forever; work/ISRs do the job */
    while (1) {
        k_sleep(K_FOREVER);
    }
}
