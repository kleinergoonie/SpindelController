#include <math.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "pico/critical_section.h"
#include "hardware/watchdog.h"

#include "config.h"
#include "display_st7789.h"
#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
#include "encoder_mt6835.h"
#elif ENCODER_TYPE_SELECT == ENCODER_TYPE_AS5600
#include "encoder_as5600.h"
#else
#error "Unsupported ENCODER_TYPE_SELECT"
#endif
#include "motor_control.h"
#include "remora_iface.h"
#include "safety.h"
#include "sensor_adc.h"
#include "spindle_ctrl.h"
#include "touch_input.h"
#include "ws2812_status.h"

typedef struct {
	float kp;
	float ki;
	float kd;
	float integral;
	float prev_error;
} pid_state_t;

static pid_state_t g_pid = {
	.kp = PID_KP_DEFAULT,
	.ki = PID_KI_DEFAULT,
	.kd = PID_KD_DEFAULT,
	.integral = 0.0f,
	.prev_error = 0.0f,
};

static float g_speed_setpoint_rpm = 0.0f;
static float g_speed_measured_rpm = 0.0f;
#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
static uint32_t g_prev_angle_raw = 0u;
#else
static uint16_t g_prev_angle_raw = 0u;
#endif
static absolute_time_t g_prev_sample_time;
static uint32_t g_last_runtime_autozero_ms = 0u;
static volatile bool g_fault_latched = false;
static volatile bool g_encoder_fault = false;
static volatile uint32_t g_last_encoder_valid_ms = 0u;
static volatile uint8_t g_control_mode_requested = CONTROL_MODE_DEFAULT;
static volatile uint8_t g_control_mode_effective = CONTROL_MODE_DEFAULT;
static volatile bool g_position_mode_speed_inhibit = false;
static float g_position_setpoint_deg = 0.0f;
static float g_position_measured_deg = 0.0f;

/* Serial-driven MT6835 streaming state */
static volatile bool g_serial_streaming = false;
static uint32_t g_last_serial_stream_ms = 0u;
static const uint32_t SERIAL_STREAM_INTERVAL_MS = 200u; /* ms */
static uint32_t g_last_alive_report_ms = 0u;
static uint32_t g_last_mt6835_report_ms = 0u;

typedef struct {
	float setpoint_rpm;
	float measured_rpm;
	float duty;
	float torque_nm;
	bool enabled;
	bool fault_active;
	uint32_t status_bits;
	uint8_t motor_mode;
	uint8_t remote_ip[4];
	uint16_t remote_port;
} ui_snapshot_t;

static critical_section_t g_ui_lock;
static ui_snapshot_t g_ui_snapshot;
static critical_section_t g_netcfg_lock;
static volatile bool g_netcfg_pending = false;
static uint8_t g_netcfg_ip[4];
static uint16_t g_netcfg_port = REMORA_SERVER_PORT;

static void ui_snapshot_write(const ui_snapshot_t* src) {
	if (!src) {
		return;
	}

	critical_section_enter_blocking(&g_ui_lock);
	g_ui_snapshot = *src;
	critical_section_exit(&g_ui_lock);
}

static ui_snapshot_t ui_snapshot_read(void) {
	ui_snapshot_t snapshot;
	critical_section_enter_blocking(&g_ui_lock);
	snapshot = g_ui_snapshot;
	critical_section_exit(&g_ui_lock);
	return snapshot;
}

static void netcfg_request_set(const uint8_t ip[4], uint16_t port) {
	if (!ip || (port == 0u)) {
		return;
	}

	critical_section_enter_blocking(&g_netcfg_lock);
	g_netcfg_ip[0] = ip[0];
	g_netcfg_ip[1] = ip[1];
	g_netcfg_ip[2] = ip[2];
	g_netcfg_ip[3] = ip[3];
	g_netcfg_port = port;
	g_netcfg_pending = true;
	critical_section_exit(&g_netcfg_lock);
}

static bool netcfg_request_take(uint8_t ip_out[4], uint16_t* port_out) {
	bool has_request = false;

	critical_section_enter_blocking(&g_netcfg_lock);
	if (g_netcfg_pending) {
		if (ip_out) {
			ip_out[0] = g_netcfg_ip[0];
			ip_out[1] = g_netcfg_ip[1];
			ip_out[2] = g_netcfg_ip[2];
			ip_out[3] = g_netcfg_ip[3];
		}
		if (port_out) {
			*port_out = g_netcfg_port;
		}
		g_netcfg_pending = false;
		has_request = true;
	}
	critical_section_exit(&g_netcfg_lock);

	return has_request;
}

static void core1_ui_main(void) {
	display_st7789_init();
	touch_input_init();

	bool edit_mode = false;
	uint8_t edit_octet = 0u;
	uint8_t edit_ip[4] = {
		REMORA_SERVER_IP_0,
		REMORA_SERVER_IP_1,
		REMORA_SERVER_IP_2,
		REMORA_SERVER_IP_3,
	};
	uint16_t edit_port = REMORA_SERVER_PORT;

	const uint32_t ui_interval_ms = 1000u / UI_UPDATE_HZ;
	absolute_time_t next_ui = make_timeout_time_ms(ui_interval_ms);

	while (true) {
		touch_event_t touch;
		if (touch_input_poll(&touch) && touch.just_pressed) {
			if (!edit_mode) {
				if (touch.y >= 126u) {
					edit_mode = true;
					const ui_snapshot_t snapshot = ui_snapshot_read();
					edit_ip[0] = snapshot.remote_ip[0];
					edit_ip[1] = snapshot.remote_ip[1];
					edit_ip[2] = snapshot.remote_ip[2];
					edit_ip[3] = snapshot.remote_ip[3];
					edit_port = snapshot.remote_port;
				}
			} else if (touch.y >= 154u && touch.y < 188u) {
				if (touch.x < 93u) {
					if (edit_ip[edit_octet] < 255u) {
						++edit_ip[edit_octet];
					}
				} else if (touch.x < 186u) {
					if (edit_ip[edit_octet] > 0u) {
						--edit_ip[edit_octet];
					}
				} else {
					netcfg_request_set(edit_ip, edit_port);
					edit_mode = false;
				}
			} else if (touch.y >= 126u && touch.y < 154u) {
				const uint16_t segment = (uint16_t)(DISPLAY_WIDTH / 4u);
				uint8_t selected = (uint8_t)(touch.x / segment);
				if (selected > 3u) {
					selected = 3u;
				}
				edit_octet = selected;
			}
		}

		if (absolute_time_diff_us(get_absolute_time(), next_ui) <= 0) {
			const ui_snapshot_t snapshot = ui_snapshot_read();
			display_st7789_update_status(
				snapshot.setpoint_rpm,
				snapshot.measured_rpm,
				snapshot.duty,
				snapshot.torque_nm,
				snapshot.enabled,
				snapshot.fault_active,
				snapshot.status_bits,
				snapshot.motor_mode
			);

			const uint8_t* shown_ip = edit_mode ? edit_ip : snapshot.remote_ip;
			const uint16_t shown_port = edit_mode ? edit_port : snapshot.remote_port;
			display_st7789_update_network(shown_ip, shown_port, edit_mode, edit_octet);

			next_ui = make_timeout_time_ms(ui_interval_ms);
		}

		sleep_ms(1);
	}
}

static void encoder_init(void) {
#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
	mt6835_init();
#else
	as5600_init();
#endif
}

static bool encoder_read_rpm(float* rpm_out) {
	if (!rpm_out) {
		return false;
	}

	const absolute_time_t now = get_absolute_time();
	const float dt_sec = (float)absolute_time_diff_us(g_prev_sample_time, now) / 1000000.0f;

#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
	mt6835_sample_t sample;
	if (!mt6835_read_sample(&sample) || !sample.crc_valid) {
		return false;
	}

	*rpm_out = mt6835_compute_rpm(g_prev_angle_raw, sample.angle_raw, dt_sec);
	g_position_measured_deg = mt6835_raw_to_degrees(sample.angle_raw);
	g_prev_angle_raw = sample.angle_raw;
#else
	as5600_sample_t sample;
	if (!as5600_read_sample(&sample)) {
		return false;
	}

	*rpm_out = as5600_compute_rpm(g_prev_angle_raw, sample.angle_raw, dt_sec);
	g_position_measured_deg = as5600_raw_to_degrees(sample.angle_raw);
	g_prev_angle_raw = sample.angle_raw;
#endif

	g_prev_sample_time = now;
	return true;
}

static bool encoder_read_initial_angle(void) {
#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
	mt6835_sample_t sample;
	if (mt6835_read_sample(&sample) && sample.crc_valid) {
		g_prev_angle_raw = sample.angle_raw;
		g_position_measured_deg = mt6835_raw_to_degrees(sample.angle_raw);
		return true;
	}
#else
	as5600_sample_t sample;
	if (as5600_read_sample(&sample)) {
		g_prev_angle_raw = sample.angle_raw;
		g_position_measured_deg = as5600_raw_to_degrees(sample.angle_raw);
		return true;
	}
#endif

	return false;
}

static uint32_t get_millis(void) {
	return to_ms_since_boot(get_absolute_time());
}

static float clampf(float v, float lo, float hi) {
	if (v < lo) {
		return lo;
	}
	if (v > hi) {
		return hi;
	}
	return v;
}

static float slew_towards(float current, float target, float max_step) {
	if (target > current + max_step) {
		return current + max_step;
	}
	if (target < current - max_step) {
		return current - max_step;
	}
	return target;
}

static float torque_for_display(float current_a, motor_mode_t mode) {
	const float torque_abs = fabsf(current_a) * MOTOR_TORQUE_CONSTANT_NM_PER_A;

#if TORQUE_DISPLAY_MODE == TORQUE_DISPLAY_ABS
	(void)mode;
	return torque_abs;
#else
	if (mode == MOTOR_MODE_REVERSE) {
		return -torque_abs;
	}
	return torque_abs;
#endif
}

static float normalize_angle_deg(float deg) {
	while (deg >= 360.0f) {
		deg -= 360.0f;
	}
	while (deg < 0.0f) {
		deg += 360.0f;
	}
	return deg;
}

static float shortest_angle_error_deg(float target_deg, float current_deg) {
	float error = normalize_angle_deg(target_deg) - normalize_angle_deg(current_deg);
	if (error > 180.0f) {
		error -= 360.0f;
	} else if (error < -180.0f) {
		error += 360.0f;
	}
	return error;
}

static float pid_update(pid_state_t* pid, float setpoint, float measured, float dt) {
	const float error = setpoint - measured;
	pid->integral += error * dt;

	// Keep integral bounded so startup or stall events do not lock output high.
	pid->integral = clampf(pid->integral, -5000.0f, 5000.0f);

	const float derivative = (dt > 0.0f) ? (error - pid->prev_error) / dt : 0.0f;
	pid->prev_error = error;

	return pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
}

static void pid_reset(pid_state_t* pid) {
	if (!pid) {
		return;
	}
	pid->integral = 0.0f;
	pid->prev_error = 0.0f;
}

static void control_loop_work(void) {
	// Placeholder if needed; control runs on repeating timer.
}

static bool control_loop_cb(struct repeating_timer* timer) {
	(void)timer;

	const uint32_t now_ms = get_millis();
	float rpm = 0.0f;
	if (encoder_read_rpm(&rpm)) {
		g_speed_measured_rpm = rpm;
		g_last_encoder_valid_ms = now_ms;
		g_encoder_fault = false;
	} else if ((now_ms - g_last_encoder_valid_ms) > ENCODER_TIMEOUT_MS) {
		g_encoder_fault = true;
	}

	const safety_state_t* safety = safety_get_state();
	const bool hard_fault =
		safety->estop_active ||
		safety->overcurrent ||
		safety->overtemp ||
		safety->watchdog_timeout ||
		g_encoder_fault;
	if (hard_fault) {
		g_fault_latched = true;
	}

	remora_command_t cmd;
	if (remora_iface_get_latest_command(&cmd)) {
		g_control_mode_requested = cmd.control_mode;
		g_position_setpoint_deg = normalize_angle_deg(cmd.position_setpoint_deg);

		if (g_control_mode_requested == CONTROL_MODE_POSITION) {
			if (fabsf(g_speed_measured_rpm) >= POSITION_DISABLE_RPM_THRESHOLD) {
				g_position_mode_speed_inhibit = true;
			} else if (fabsf(g_speed_measured_rpm) < POSITION_ENABLE_RPM_THRESHOLD) {
				g_position_mode_speed_inhibit = false;
			}
		}
		g_control_mode_effective =
			(g_control_mode_requested == CONTROL_MODE_POSITION && g_position_mode_speed_inhibit)
				? CONTROL_MODE_SPEED
				: g_control_mode_requested;

		const float requested_rpm = clampf(cmd.speed_setpoint_rpm, SPINDLE_RPM_MIN, SPINDLE_RPM_MAX);
		const float target_rpm = (cmd.enable && !cmd.brake_cmd) ? requested_rpm : 0.0f;
		const float max_step = SPINDLE_RAMP_RPM_PER_S / (float)CONTROL_LOOP_HZ;
		g_speed_setpoint_rpm = slew_towards(g_speed_setpoint_rpm, target_rpm, max_step);

		// Fault latch can only be acknowledged by explicit disable after fault condition disappeared.
		if (g_fault_latched && !hard_fault && !cmd.enable) {
			g_fault_latched = false;
		}

		const bool should_stop = hard_fault || g_fault_latched || !cmd.enable || cmd.brake_cmd;
		if (should_stop) {
			pid_reset(&g_pid);
			motor_control_set_duty(0.0f);
			motor_control_set_mode((hard_fault || cmd.brake_cmd) ? MOTOR_MODE_BRAKE : MOTOR_MODE_COAST);
			motor_control_set_enable(false);
			spindle_ctrl_set_normalized(0.0f);
		} else {
			float duty = 0.0f;
			if (g_control_mode_effective == CONTROL_MODE_POSITION) {
				const float err_deg = shortest_angle_error_deg(g_position_setpoint_deg, g_position_measured_deg);
				if (fabsf(err_deg) <= POSITION_DEADBAND_DEG) {
					duty = 0.0f;
				} else {
					duty = clampf(err_deg * POSITION_KP_DEFAULT, -POSITION_MAX_DUTY, POSITION_MAX_DUTY);
				}
			} else {
				const float out = pid_update(&g_pid, g_speed_setpoint_rpm, g_speed_measured_rpm, 1.0f / CONTROL_LOOP_HZ);
				duty = clampf(out / SPINDLE_RPM_MAX, -1.0f, 1.0f);
				if (cmd.direction && duty > 0.0f) {
					duty = -duty;
				}
			}

			motor_control_set_duty(duty);
			motor_control_set_enable(true);
			spindle_ctrl_set_normalized(fabsf(duty));
		}
	}

	remora_feedback_t feedback = {
		.speed_measured_rpm = g_speed_measured_rpm,
		.status_bits =
			(safety->estop_active ? (1u << STATUS_BIT_ESTOP) : 0u) |
			(safety->overcurrent ? (1u << STATUS_BIT_OVERCURRENT) : 0u) |
			(safety->overtemp ? (1u << STATUS_BIT_OVERTEMP) : 0u) |
			(safety->watchdog_timeout ? (1u << STATUS_BIT_WATCHDOG) : 0u) |
			(g_fault_latched ? (1u << STATUS_BIT_FAULT_LATCHED) : 0u) |
			(g_encoder_fault ? (1u << STATUS_BIT_ENCODER_FAULT) : 0u) |
			(g_position_mode_speed_inhibit ? (1u << STATUS_BIT_POSITION_DISABLED) : 0u),
	};
	remora_iface_publish_feedback(&feedback);

	return true;
}

int main(void) {
	stdio_init_all();
	sleep_ms(200);

	printf("SpindelController: init start\n");

	if (watchdog_caused_reboot()) {
		printf("Watchdog reboot detected\n");
	}

#if HARDWARE_WATCHDOG_ENABLE
	watchdog_enable(100, true);
#else
	watchdog_disable();
#endif

	motor_control_init();
	spindle_ctrl_init();
	spindle_ctrl_set_normalized(0.0f);
	ws2812_status_init();
	encoder_init();
	safety_init();
	safety_kick_watchdog(get_millis());
	sensor_adc_init();
	const float acs_zero = sensor_adc_calibrate_zero(ACS712_AUTOZERO_SAMPLES, ACS712_AUTOZERO_DELAY_US);
	printf("ACS712 zero calibrated: %.1f counts\n", acs_zero);
	remora_iface_init();

	critical_section_init(&g_ui_lock);
	critical_section_init(&g_netcfg_lock);
	g_ui_snapshot = (ui_snapshot_t){0};
	multicore_launch_core1(core1_ui_main);

	g_prev_sample_time = get_absolute_time();
	const bool encoder_init_ok = encoder_read_initial_angle();
	g_last_encoder_valid_ms = get_millis();
	g_encoder_fault = !encoder_init_ok;

	struct repeating_timer control_timer;
	add_repeating_timer_us(-(1000000 / CONTROL_LOOP_HZ), control_loop_cb, NULL, &control_timer);

	g_last_runtime_autozero_ms = get_millis();

	while (true) {
		uint8_t endpoint_ip[4];
		uint16_t endpoint_port = 0u;
		if (netcfg_request_take(endpoint_ip, &endpoint_port)) {
			(void)remora_iface_set_server_endpoint(endpoint_ip, endpoint_port);
		}

		remora_iface_poll();

		const uint32_t now_ms = get_millis();
		const float current_a = sensor_adc_read_current_amps();
		safety_update(current_a, 0.0f, now_ms);
		const safety_state_t* safety = safety_get_state();
		const bool hard_fault =
			safety->estop_active ||
			safety->overcurrent ||
			safety->overtemp ||
			safety->watchdog_timeout ||
			g_encoder_fault;
		const bool fault_active = hard_fault || g_fault_latched;
		const uint32_t status_bits =
			(safety->estop_active ? (1u << STATUS_BIT_ESTOP) : 0u) |
			(safety->overcurrent ? (1u << STATUS_BIT_OVERCURRENT) : 0u) |
			(safety->overtemp ? (1u << STATUS_BIT_OVERTEMP) : 0u) |
			(safety->watchdog_timeout ? (1u << STATUS_BIT_WATCHDOG) : 0u) |
			(g_fault_latched ? (1u << STATUS_BIT_FAULT_LATCHED) : 0u) |
			(g_encoder_fault ? (1u << STATUS_BIT_ENCODER_FAULT) : 0u) |
			(g_position_mode_speed_inhibit ? (1u << STATUS_BIT_POSITION_DISABLED) : 0u);
		const motor_mode_t motor_mode = motor_control_get_mode();
		uint8_t remote_ip[4] = {0u, 0u, 0u, 0u};
		uint16_t remote_port = REMORA_SERVER_PORT;
		remora_iface_get_server_endpoint(remote_ip, &remote_port);

		const ui_snapshot_t ui_snapshot = {
			.setpoint_rpm = g_speed_setpoint_rpm,
			.measured_rpm = g_speed_measured_rpm,
			.duty = motor_control_get_duty(),
			.torque_nm = torque_for_display(current_a, motor_mode),
			.enabled = motor_control_get_enable(),
			.fault_active = fault_active,
			.status_bits = status_bits,
			.motor_mode = (uint8_t)motor_mode,
			.remote_ip = { remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3] },
			.remote_port = remote_port,
		};
		ui_snapshot_write(&ui_snapshot);

		const uint32_t remora_rx_age_ms = remora_iface_get_rx_age_ms(now_ms);
		ws2812_status_update(status_bits, fault_active, motor_control_get_enable(), now_ms, remora_rx_age_ms);

#if ACS712_RUNTIME_AUTOZERO_ENABLE
		// Re-zero ACS712 only when machine is idle to avoid learning load current as offset.
		if (fabsf(g_speed_setpoint_rpm) < 1.0f &&
			fabsf(current_a) <= ACS712_RUNTIME_AUTOZERO_MAX_ABS_A &&
			(now_ms - g_last_runtime_autozero_ms) >= ACS712_RUNTIME_AUTOZERO_INTERVAL_MS) {
			const float runtime_zero = sensor_adc_calibrate_zero(
				ACS712_RUNTIME_AUTOZERO_SAMPLES,
				ACS712_RUNTIME_AUTOZERO_DELAY_US
			);
			g_last_runtime_autozero_ms = now_ms;
			printf("ACS712 runtime autozero: %.1f counts\n", runtime_zero);
		}
#endif

#if HARDWARE_WATCHDOG_ENABLE
		watchdog_update();
#endif
		safety_kick_watchdog(get_millis());

#if SERIAL_ALIVE_REPORT_ENABLE
		if ((now_ms - g_last_alive_report_ms) >= SERIAL_ALIVE_REPORT_INTERVAL_MS) {
			printf("ALIVE t=%lums rpm_set=%.1f rpm=%.1f en=%u mode_req=%u mode_eff=%u fault=%u bits=0x%08lX\n",
				(unsigned long)now_ms,
				(double)g_speed_setpoint_rpm,
				(double)g_speed_measured_rpm,
				(unsigned)motor_control_get_enable(),
				(unsigned)g_control_mode_requested,
				(unsigned)g_control_mode_effective,
				(unsigned)fault_active,
				(unsigned long)status_bits);
			g_last_alive_report_ms = now_ms;
		}
#endif

		// Serial command handling (non-blocking): 'S' start stream, 's' stop, 'R' one-shot snapshot
		int ch = getchar_timeout_us(0);
		if (ch >= 0) {
			char c = (char)ch;
			switch (c) {
				case 'S':
					g_serial_streaming = true;
					g_last_serial_stream_ms = now_ms;
					printf("SERIAL STREAM START\n");
					break;
				case 's':
					g_serial_streaming = false;
					printf("SERIAL STREAM STOP\n");
					break;
				case 'R':
#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
					mt6835_report_registers();
#endif
					printf("SERIAL SNAPSHOT\n");
					break;
				default:
					break;
			}
		}

		// If serial streaming requested, emit periodic register reports regardless of MT6835_REGISTER_REPORT_ENABLE
		if (g_serial_streaming) {
			if ((now_ms - g_last_serial_stream_ms) >= SERIAL_STREAM_INTERVAL_MS) {
#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
				mt6835_report_registers();
#endif
				g_last_serial_stream_ms = now_ms;
			}
		}

#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
#if MT6835_REGISTER_REPORT_ENABLE
		if ((now_ms - g_last_mt6835_report_ms) >= MT6835_REGISTER_REPORT_INTERVAL_MS && !g_serial_streaming) {
			mt6835_report_registers();
			g_last_mt6835_report_ms = now_ms;
		}
#endif
#endif

		sleep_ms(1);
	}
}
