// SPDX-License-Identifier: GPL-2.0
/*
 *  linux/drivers/tty/serial/next.c
 *
 *  Minimal NeXT serial driver for the Zilog Z8530 SCC.
 *
 *  The PROM/bootloader configures the serial hardware up front. This driver
 *  keeps the transmit path very small, but follows the standard Linux
 *  serial_core API so it can be registered through uart_register_driver().
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/serial_core.h>
#include <linux/timer.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#include <asm/nexthw.h>

#define ZS_PORTB_CTRL	0
#define ZS_PORTA_CTRL	1
#define ZS_DATA_OFFSET	2
#define RR0_TXEMPTY_BIT	2
#define RR0_RXREADY_BIT	0

#define NEXT_SERIAL_NAME	"next_serial"

static void next_serial_put_char(struct uart_port *port, unsigned char ch)
{
	void __iomem *port_ctrl = port->membase + port->line;

	// if (ch == '\n')
	// 	next_serial_put_char(port, '\r');

	while ((readb(port_ctrl) & (1u << RR0_TXEMPTY_BIT)) == 0)
		cpu_relax();
	writeb(ch, port_ctrl + ZS_DATA_OFFSET);
}

static unsigned int next_serial_tx_empty(struct uart_port *port)
{
	void __iomem *port_ctrl = port->membase + port->line;

	return (readb(port_ctrl) & (1u << RR0_TXEMPTY_BIT)) ? TIOCSER_TEMT : 0;
}

static void next_serial_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	/* The NeXT SCC does not expose modem control lines in this minimal driver. */
}

static unsigned int next_serial_get_mctrl(struct uart_port *port)
{
	return TIOCM_DSR | TIOCM_CAR | TIOCM_CTS;
}

static void next_serial_stop_tx(struct uart_port *port)
{
	pr_err("next_serial: next_serial_stop_tx\n");
	/* No DMA/interrupt-driven transmit; polling is enough for now. */
}

static void next_serial_start_tx(struct uart_port *port)
{
	struct tty_port *tport = &port->state->port;
	unsigned char ch;

	pr_err("next_serial: next_serial_start_tx\n");
	while (kfifo_get(&tport->xmit_fifo, &ch))
		next_serial_put_char(port, ch);
}

struct next_serial_rx_timer {
	struct timer_list timer;
	struct uart_port *port;
};

static struct next_serial_rx_timer next_serial_rx_timers[2];
static void next_serial_poll_rx(struct timer_list *t);

static void next_serial_start_rx(struct uart_port *port)
{
	struct next_serial_rx_timer *rx = &next_serial_rx_timers[port->line];

	rx->port = port;
	timer_setup(&rx->timer, next_serial_poll_rx, 0);
	mod_timer(&rx->timer, jiffies + msecs_to_jiffies(1));
}

static void next_serial_receive_chars(struct uart_port *port)
{
	struct tty_port *tport;
	void __iomem *ctrl;
	void __iomem *data;
	unsigned char ch;
	int got = 0;

	if (!port || !port->membase || !port->state)
		return;

	tport = &port->state->port;
	ctrl = port->membase + port->line;
	data = ctrl + ZS_DATA_OFFSET;

	while (readb(ctrl) & (1u << RR0_RXREADY_BIT)) {
		ch = readb(data);
		port->icount.rx++;
		tty_insert_flip_char(tport, ch, TTY_NORMAL);
		got = 1;
	}

	if (got)
		tty_flip_buffer_push(tport);
}

static void next_serial_poll_rx(struct timer_list *t)
{
	struct next_serial_rx_timer *rx = timer_container_of(rx, t, timer);
	struct uart_port *port = rx->port;

	if (!port)
		return;

	next_serial_receive_chars(port);
	mod_timer(&rx->timer, jiffies + msecs_to_jiffies(10));
}

static void next_serial_stop_rx(struct uart_port *port)
{
	struct next_serial_rx_timer *rx;

	pr_err("next_serial: next_serial_stop_rx\n");
	rx = &next_serial_rx_timers[port->line];
	timer_delete_sync(&rx->timer);
}

static void next_serial_break_ctl(struct uart_port *port, int break_state)
{
	/* Break condition support is not needed for the boot-time NeXT debug path. */
}

static int next_serial_startup(struct uart_port *port)
{
	pr_err("next_serial: next_serial_startup\n");
	if (!port->membase) {
		port->membase = ioremap(NEXT_SCC, 0x40);
		if (!port->membase)
			return -ENOMEM;
	}

	next_serial_start_rx(port);

	return 0;
}

static void next_serial_shutdown(struct uart_port *port)
{
	pr_err("next_serial: next_serial_shutdown\n");

	if (port->membase) {
		iounmap(port->membase);
		port->membase = NULL;
	}
}

static void next_serial_set_termios(struct uart_port *port,
					struct ktermios *new,
					const struct ktermios *old)
{
	/* 9600 8N1 is assumed to be configured by the PROM/bootloader. */
	port->uartclk = 9600000;
	tty_termios_encode_baud_rate(new, 9600, 9600);
}

static const char *next_serial_type(struct uart_port *port)
{
	return NEXT_SERIAL_NAME;
}

static void next_serial_config_port(struct uart_port *port, int flags)
{
	pr_err("next_serial: next_serial_config_port\n");
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_UNKNOWN;
}

static int next_serial_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	pr_err("next_serial: next_serial_verify_port\n");
	if (ser->type != PORT_UNKNOWN)
		return -EINVAL;

	return 0;
}

static const struct uart_ops next_serial_ops = {
	.tx_empty	= next_serial_tx_empty,
	.set_mctrl	= next_serial_set_mctrl,
	.get_mctrl	= next_serial_get_mctrl,
	.stop_tx	= next_serial_stop_tx,
	.start_tx	= next_serial_start_tx,
	.start_rx	= next_serial_start_rx,
	.stop_rx	= next_serial_stop_rx,
	.break_ctl	= next_serial_break_ctl,
	.startup	= next_serial_startup,
	.shutdown	= next_serial_shutdown,
	.set_termios	= next_serial_set_termios,
	.type		= next_serial_type,
	.config_port	= next_serial_config_port,
	.verify_port	= next_serial_verify_port,
};

static struct uart_driver next_serial_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= NEXT_SERIAL_NAME,
	.dev_name	= "ttyS",
	.major		= TTY_MAJOR,
	.minor		= 64,
	.nr		= 2,
};

static void next_serial_parent_release(struct device *dev)
{
	/* This object is static and intentionally never freed by the kernel. */
}

static struct device next_serial_parent = {
	.init_name = NEXT_SERIAL_NAME,
	.release = next_serial_parent_release,
};

static struct uart_port next_serial_port_b = {
	.iotype	= UPIO_MEM,
	.flags	= UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE | UPF_IOREMAP,
	.line	= 0,
	.ctrl_id	= 0,
	.dev	= &next_serial_parent,
	.type	= PORT_16550A,
	.irq	= 0,
	.uartclk	= 9600000,
	.fifosize	= 1,
	.ops	= &next_serial_ops,
};

static struct uart_port next_serial_port_a = {
	.iotype	= UPIO_MEM,
	.flags	= UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE | UPF_IOREMAP,
	.line	= 1,
	.ctrl_id	= 0,
	.dev	= &next_serial_parent,
	.type	= PORT_16550A,
	.irq	= 0,
	.uartclk	= 9600000,
	.fifosize	= 1,
	.ops	= &next_serial_ops,
};

static int __init next_serial_init_driver(void)
{
	int ret;

	pr_info("next_serial: init begin\n");
	for (int i = 0; i < ARRAY_SIZE(next_serial_rx_timers); i++) {
		timer_setup(&next_serial_rx_timers[i].timer, next_serial_poll_rx, 0);
		next_serial_rx_timers[i].port = NULL;
	}

	ret = device_register(&next_serial_parent);
	if (ret) {
		pr_err("next_serial: parent device_register failed: %d\n", ret);
		return ret;
	}

	ret = uart_register_driver(&next_serial_driver);
	if (ret) {
		pr_err("next_serial: uart_register_driver failed: %d\n", ret);
		goto err_unregister_parent;
	}

	pr_info("next_serial: registering ports\n");
	ret = uart_add_one_port(&next_serial_driver, &next_serial_port_b);
	if (ret) {
		pr_err("next_serial: uart_add_one_port(B) failed: %d\n", ret);
		goto err_unregister_driver;
	}

	ret = uart_add_one_port(&next_serial_driver, &next_serial_port_a);
	if (ret) {
		pr_err("next_serial: uart_add_one_port(A) failed: %d\n", ret);
		goto err_remove_port_b;
	}

	pr_info("next_serial: init done\n");
	return 0;

err_remove_port_b:
	uart_remove_one_port(&next_serial_driver, &next_serial_port_b);
err_unregister_driver:
	uart_unregister_driver(&next_serial_driver);
err_unregister_parent:
	device_unregister(&next_serial_parent);
	return ret;
}

static void __exit next_serial_exit_driver(void)
{
	pr_info("next_serial: shutdown begin\n");
	for (int i = 0; i < ARRAY_SIZE(next_serial_rx_timers); i++)
		timer_delete_sync(&next_serial_rx_timers[i].timer);
	uart_remove_one_port(&next_serial_driver, &next_serial_port_b);
	uart_remove_one_port(&next_serial_driver, &next_serial_port_a);
	uart_unregister_driver(&next_serial_driver);
	device_unregister(&next_serial_parent);
	pr_info("next_serial: shutdown done\n");
}

module_init(next_serial_init_driver);
module_exit(next_serial_exit_driver);

MODULE_DESCRIPTION("Minimal Linux serial driver for the NeXT Z8530 SCC");
MODULE_AUTHOR("NeXT Linux port");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(TTY_MAJOR);
