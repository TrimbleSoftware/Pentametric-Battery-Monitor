/*

	mhpmpi.h

*/
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <time.h>

/*
	defines
*/
#define true 1
#define false 0

// pentametric commands
#define PENTAMETRIC_SHORT_READ_COMMAND 0x81
#define PENTAMETRIC_SHORT_WRITE_COMMAND 0x01

#define PENTAMETRIC_CHECKSUM 0xff

// pentametric data value addresses
#define PENTAMETRIC_ADDRESS_BATTERY1_VOLTS 0x01
#define PENTAMETRIC_ADDRESS_BATTERY2_VOLTS 0x02
#define PENTAMETRIC_ADDRESS_AVERAGE_BATTERY1_VOLTS 0x03
#define PENTAMETRIC_ADDRESS_AVERAGE_BATTERY2_VOLTS 0x04
#define PENTAMETRIC_ADDRESS_AMPS1 0x05
#define PENTAMETRIC_ADDRESS_AMPS2 0x06
#define PENTAMETRIC_ADDRESS_AMPS3 0x07
#define PENTAMETRIC_ADDRESS_AVERAGE_AMPS1 0x08
#define PENTAMETRIC_ADDRESS_AVERAGE_AMPS2 0x09
#define PENTAMETRIC_ADDRESS_AVERAGE_AMPS3 0x0a
#define PENTAMETRIC_ADDRESS_AMP_HOURS1 0x0c
#define PENTAMETRIC_ADDRESS_AMP_HOURS2 0x0d
#define PENTAMETRIC_ADDRESS_AMP_HOURS3 0x0f
#define PENTAMETRIC_ADDRESS_CUM_AMP_HOURS1 0x12
#define PENTAMETRIC_ADDRESS_CUM_AMP_HOURS2 0x13
#define PENTAMETRIC_ADDRESS_WATTS1 0x17
#define PENTAMETRIC_ADDRESS_WATTS2 0x18
#define PENTAMETRIC_ADDRESS_WATT_HOURS1 0x15
#define PENTAMETRIC_ADDRESS_WATT_HOURS2 0x16
#define PENTAMETRIC_ADDRESS_BATTERY1_PERCENT_FULL 0x1a
#define PENTAMETRIC_ADDRESS_BATTERY2_PERCENT_FULL 0x1b
#define PENTAMETRIC_ADDRESS_DAYS_SINCE_BATTERY1_CHARGED 0x1c
#define PENTAMETRIC_ADDRESS_DAYS_SINCE_BATTERY2_CHARGED 0x1d
#define PENTAMETRIC_ADDRESS_DAYS_SINCE_BATTERY1_EQUALIZED 0x1e
#define PENTAMETRIC_ADDRESS_DAYS_SINCE_BATTERY2_EQUALIZED 0x1f
#define PENTAMETRIC_ADDRESS_TEMPERATURE 0x19
#define PENTAMETRIC_ADDRESS_SHUNT_LABELS 0xe1
// pentametric config value addresses
#define PENTAMETRIC_ADDRESS_SHUNT_SELECT 0xf6
#define PENTAMETRIC_ADDRESS_FIRMWARE_VERSION 0xf7
// pentametric reset command addresses
#define PENTAMETRIC_ADDRESS_RESET 0x27

// pentametric command codes
#define PENTAMETRIC_CLEAR_AMP_HOURS_1 0x09
#define PENTAMETRIC_CLEAR_AMP_HOURS_2 0x0a
#define PENTAMETRIC_CLEAR_AMP_HOURS_3 0x0b // gussing this is the correct value. it is missing from the documentation
#define PENTAMETRIC_CLEAR_AMP_HOURS 0x0c
#define PENTAMETRIC_CLEAR_WATT_HOURS1 0x11
#define PENTAMETRIC_CLEAR_WATT_HOURS2 0x12
#define PENTAMETRIC_CLEAR_WATT_HOURS 0x13

#define SHUNT1_500A 0x01
#define SHUNT2_500A 0x02
#define SHUNT3_500A 0x04

#define SHUNT1_BATTERY 0x01
#define SHUNT2_BATTERY 0x02
#define SHUNT3_BATTERY 0x04

// sensor bit masks
#define PENTAMETRIC_BATTERY1_VOLTS					0x01   
#define	PENTAMETRIC_BATTERY2_VOLTS					0x02
#define	PENTAMETRIC_AVERAGE_BATTERY1_VOLTS			0x04
#define	PENTAMETRIC_AVERAGE_BATTERY2_VOLTS			0x08
#define PENTAMETRIC_AMPS1							0x10
#define	PENTAMETRIC_AMPS2							0x20 
#define PENTAMETRIC_AMPS3							0x40
#define	PENTAMETRIC_AVERAGE_AMPS1					0x80
#define	PENTAMETRIC_AVERAGE_AMPS2					0x100
#define	PENTAMETRIC_AVERAGE_AMPS3					0x200
#define	PENTAMETRIC_AMP_HOURS1						0x400
#define	PENTAMETRIC_AMP_HOURS2						0x800
#define	PENTAMETRIC_AMP_HOURS3						0x1000
#define	PENTAMETRIC_CUM_AMP_HOURS1					0x2000
#define	PENTAMETRIC_CUM_AMP_HOURS2					0x4000
#define	PENTAMETRIC_WATTS1							0x8000
#define	PENTAMETRIC_WATTS2							0x10000
#define	PENTAMETRIC_WATT_HOURS1						0x20000
#define	PENTAMETRIC_WATT_HOURS2						0x40000
#define	PENTAMETRIC_BATTERY1_PERCENT_FULL			0x80000
#define	PENTAMETRIC_BATTERY2_PERCENT_FULL			0x100000
#define	PENTAMETRIC_DAYS_SINCE_BATTERY1_CHARGED		0x200000
#define	PENTAMETRIC_DAYS_SINCE_BATTERY2_CHARGED		0x400000
#define	PENTAMETRIC_DAYS_SINCE_BATTERY1_EQUALIZED	0x800000
#define	PENTAMETRIC_DAYS_SINCE_BATTERY2_EQUALIZED	0x1000000
#define	PENTAMETRIC_TEMPERATURE						0x2000000

/*
	constants
*/

/*
	typedefs
*/
typedef unsigned char boolean;

/*
	structs
*/
struct args_t
{
	boolean close_tty_file;	// -C switch
	char *device;			// -d value
	boolean write_log;		// -L switch
	boolean reset_amp_hrs;	// -R switch
	uint32_t sensor_mask;	// -s value
	uint16_t sleep_seconds;	// -t value.
};

struct config_t
{
	boolean close_tty_file;
	char device[FILENAME_MAX];
	boolean write_log;
	char log_file_name[FILENAME_MAX];
	boolean reset_amp_hrs;
	uint32_t sensor_mask;
	uint16_t sleep_seconds;
};

/*
	function prototypes
*/
boolean pentametric_short_read (FILE *stream, uint8_t a, uint8_t n, uint8_t *msg);
boolean pentametric_short_write(FILE *stream, uint8_t a, uint8_t n, uint8_t *msg);

int32_t get_format1_value(FILE *stream, uint8_t pentametric_address);
int32_t get_format2_value(FILE *stream, uint8_t pentametric_address);
int32_t get_format2b_value(FILE *stream, uint8_t pentametric_address);
int32_t get_format3_value(FILE *stream, uint8_t pentametric_address);
int32_t get_format3b_value(FILE *stream, uint8_t pentametric_address);
int32_t get_format4_value(FILE *stream, uint8_t pentametric_address);
int32_t get_format5_value(FILE *stream, uint8_t pentametric_address);
int32_t get_format6_value(FILE *stream, uint8_t pentametric_address);
int32_t get_format7_value(FILE *stream, uint8_t pentametric_address);
int32_t get_format8_value(FILE *stream, uint8_t pentametric_address);

uint8_t get_shunt_select(FILE *stream);
uint8_t get_shunt_labels(FILE *stream);
boolean reset_amp_hours(FILE *stream, uint8_t shunt_labels);
uint8_t get_firmware_version(FILE *stream);

int32_t decode_format1(uint8_t *msg);
int32_t decode_format2(uint8_t *msg);
int32_t decode_format2b(uint8_t *msg);
int32_t decode_format3(uint8_t *msg);
int32_t decode_format3b(uint8_t *msg);
int32_t decode_format4(uint8_t *msg);
int32_t decode_format5(uint8_t *msg);
int32_t decode_format6(uint8_t *msg);
int32_t decode_format7(uint8_t *msg);
int16_t  decode_format8(uint8_t *msg);

int set_tty_port(FILE *ttyfile, char *device, char* myname, char *log_file_name, boolean writetolog);
uint32_t get_seconds_since_midnight (void);
void writelog (char *logfilename, char *process_name, char *message);
void display_usage(char *myname);
int get_configuration(struct config_t *config, char *path);