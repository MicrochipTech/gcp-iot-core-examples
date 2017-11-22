/**
 * \file
 * \brief  Google Cloud Platform IOT Core Example (SAMD21/SAMW25)
 *
 * \copyright (c) 2017 Microchip Technology Inc. and its subsidiaries.
 *            You may use this software and any derivatives exclusively with
 *            Microchip products.
 *
 * \page License
 * 
 * (c) 2017 Microchip Technology Inc. and its subsidiaries. You may use this
 * software and any derivatives exclusively with Microchip products.
 * 
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
 * EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
 * WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
 * PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
 * WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.
 * 
 * IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
 * INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
 * WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
 * BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
 * FULLEST EXTENT ALLOWED BY LAW, MICROCHIPS TOTAL LIABILITY ON ALL CLAIMS IN
 * ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 * 
 * MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
 * TERMS.
 */

#include "asf.h"
#include "config.h"
#include "time_utils.h"

#if !SAM0
#include "genclk.h"
#endif

/* Tasks */
#include "wifi_task.h"
#include "client_task.h"
#include "sensor_task.h"
#include "atca_kit_client.h"

/* Paho Client Timer */
#include "timer_interface.h"

#if SAM0
/* Module globals for SAM0 */
struct usart_module  cdc_uart_module;
struct tc_module     tc3_inst;
struct rtc_module    rtc_instance;
#endif

/**
 * \brief Configure UART console.
 */
static void configure_console(void)
{
#if SAM0
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 115200;

	stdio_serial_init(&cdc_uart_module, EDBG_CDC_MODULE, &usart_conf);
	usart_enable(&cdc_uart_module);
#elif SAM
	const usart_serial_options_t uart_serial_options = {
    	.baudrate =		CONF_UART_BAUDRATE,
    	.charlength =	CONF_UART_CHAR_LENGTH,
    	.paritytype =	CONF_UART_PARITY,
    	.stopbits =		CONF_UART_STOP_BITS,
	};

	/* Configure UART console. */
	sysclk_enable_peripheral_clock(CONSOLE_UART_ID);
	stdio_serial_init(CONF_UART, &uart_serial_options);
#endif
}

/* Initialize an RTC (or 1 second timer) */
static void configure_rtc(void)
{
#if SAM0
    struct rtc_count_config config_rtc_count;
    rtc_count_get_config_defaults(&config_rtc_count);
    config_rtc_count.continuously_update = true;

    rtc_count_init(&rtc_instance, RTC, &config_rtc_count);

    rtc_count_enable(&rtc_instance);
#elif SAM
	pmc_switch_sclk_to_32kxtal(PMC_OSC_XTAL);

	while (!pmc_osc_is_ready_32kxtal());

    rtc_set_hour_mode(RTC, 0);
#endif
}

void update_timers(void)
{
    wifi_timer_update();
    client_timer_update();
    TimerCallback();
    atca_kit_timer_update();
}

/* Timer callback function */
#if SAM0
static void periodic_timer_cb(struct tc_module *const module)
{
    update_timers();
}
#elif SAM
void TC0_Handler(void)
{
	if ((tc_get_status(TC0, 0) & TC_SR_CPCS) == TC_SR_CPCS)
    {
        update_timers();
	}
}
#endif

/* Configure a periodic timer for driving various counters */
static void configure_periodic_timer(void)
{
    uint32_t counts;

#if SAM0
    struct tc_config config_tc;
    tc_get_config_defaults(&config_tc);

    /* For ease connect to the 32kHz source rather than the PLL */
    config_tc.clock_source = GCLK_GENERATOR_1;
    config_tc.wave_generation = TC_WAVE_GENERATION_MATCH_FREQ;

    /* Calculate the match count required */
    counts = TIMER_UPDATE_PERIOD * system_gclk_gen_get_hz(config_tc.clock_source);
    counts /= 1000;

    /* Set the match value that will trigger the interrupt */
    config_tc.counter_16_bit.compare_capture_channel[0] = counts;

    /* Set up the module */
    tc_init(&tc3_inst, TC3, &config_tc);

    /* Set up the interrupt */
    tc_register_callback(&tc3_inst, periodic_timer_cb, TC_CALLBACK_OVERFLOW);
    tc_enable_callback(&tc3_inst, TC_CALLBACK_OVERFLOW);

    /* Enable the timer */
    tc_enable(&tc3_inst);
#elif SAM
    /* Enable the peripheral */
    pmc_enable_periph_clk(ID_TC0);
    
    /* Switch the Programmable Clock module PCK3 (TC Module) source clock to 
        the Slow Clock, with no pre-scaler */
    pmc_switch_pck_to_sclk(PMC_PCK_3, GENCLK_PCK_PRES_1);
    
    /* Enable Programmable Clock module PCK3 (TC Module) */
    pmc_enable_pck(PMC_PCK_3);

    /* Configure TC0 channel 0 to waveform mode (periodic count) */
    tc_init(TC0, 0, TC_CMR_TCCLKS_TIMER_CLOCK5 // Waveform Clock Selection
                 | TC_CMR_WAVE // Waveform mode is enabled
                 | TC_CMR_ACPC_CLEAR // RC Compare Effect: clear
                 | TC_CMR_CPCTRG ); // UP mode with automatic trigger on RC

    /* Configure waveform frequency and duty cycle.  */
    counts = TIMER_UPDATE_PERIOD * OSC_SLCK_32K_XTAL_HZ;
    counts /= 1000;

    /* Configure TC0 channel 0 reset counts */
    tc_write_rc(TC0, 0, counts);

    /* Enable the reset compare interrupt */
    NVIC_EnableIRQ(TC0_IRQn);
    tc_enable_interrupt(TC0, 0, TC_IER_CPCS);

    /* Start the timer */
    tc_start(TC0, 0);
#endif
}

#if BOARD == SAMG55_XPLAINED_PRO
static void configure_ext3(void)
{
    static twi_options_t ext3_twi_options;

    flexcom_enable(FLEXCOM4);
    flexcom_set_opmode(FLEXCOM4, FLEXCOM_TWI);

    ext3_twi_options.master_clk = sysclk_get_cpu_hz();
    ext3_twi_options.speed = 100000;
    ext3_twi_options.smbus = 0;

    twi_master_init(TWI4, &ext3_twi_options);
}
#endif

/**
 * \brief Main application function.
 *
 * Application entry point.
 *
 * \return program return value.
 */
int main(void)
{
    /* Initialize the board. */
#if SAM0
	system_init();
#elif SAM
    sysclk_init();
	board_init();
#endif

    /* Enable basic drivers */
    delay_init();

#if SAM0
    /* Enable Interrupts for Cortex-M0 */
    system_interrupt_enable_global();
#endif

#if BOARD == SAMG55_XPLAINED_PRO
    /* Enable the I2C interface on the EXT1 header */
    configure_ext3();
#endif

	/* Initialize the UART console. */
	configure_console();

    /* Initialize the RTC */
    configure_rtc();

    /* Set the local configuration for the cryptographic device being used */
    config_crypto();

    /* Initialize a periodic timer */
    configure_periodic_timer();

    /* Initialize the USB HID interface */
    usb_hid_init();

    /* Print a diagnostic message to the console */
    DEBUG_PRINTF("Starting Example...\r\n");

    config_print_public_key();

    for(;;)
    {
        /* Handle WIFI state machine */
        wifi_task();

        /* Handle Data Interface */
        atca_kit_main_handler();

        /* Allows the kit protocol interface to have exclusive control
         of the I2C bus when it needs it */
        if(!atca_kit_lock())
        {
            /* Handle Client State Machine */
            client_task();

            /* Handle Sensor State Machine */
            sensor_task();
        }
    }

	return 0;
}
