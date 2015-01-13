/*

mhpmpi.c

meteohub "plug-in" weather station to read and log data from a 
Bogart Engineering Pentametric Battery Monitor PM-100-C RS-232 computer interface

Written:	26-Jun-2012 by Fred Trimble ftt@smtcpa.com

Modified:	20-Dec-2012 by Fred Trimble ftt@smtcpa.com
			Ver 1.2 Corrected bug with battery temperature when the Pentametric returns a negitive value by
			changing decode_format8() function to return a signed 8-bit int instead of an unsigned 8-bit int.

Modified:	1-Feb-2012 by Fred Trimble ftt@smtcpa.com
			Ver 1.3 Added .conf file reading to allow program options to be stored in a seperate configuration file.

Modified:	27-Feb-2013 by Fred Trimble ftt@smtcpa.com
			Ver 1.4 Added .conf reading default to same location as program excutable (argv[0])
			
Modified:	10-Sept-2013 by Fred Trimble ftt@smtcpa.com
			Ver 1.41 Fixed temp sign conversion in decode_format8 function 
			
Modified:	25-Nov-2013 by Fred Trimble ftt@smtcpa.com
			Ver 1.42 Fixed 2's compliment negitive temp conversion in decode_format8 function 

Modified:	20-Sep-2014 by Fred Trimble ftt@smtcpa.com
			Ver 1.43 Added boundry sleep logic to make polling occur on even boundeies of polling value 
			
			Bogart Pentametric programming documentation:
http://www.bogartengineering.com/sites/default/files/docs/PentaMetricRS232SerialSpecWeb6-11.pdf

some sample code:
http://osaether.wordpress.com/2009/07/16/the-pentametric-has-arrived/ // examples here are incorrect in decoding charging vs. discharging values.
http://4ms.org/projects/wp-content/uploads/2010/03/solarpumppanel.c

meteohub plugin weatherstation spec:
http://www.meteohub.de/files/meteohub-v4.7en.pdf

debian linux programming man pages:
http://linux.die.net/man/3/termios

*/
//#define DEBUG

#include "mhpmpi.h"
#define VERSION "1.43"

/*
main program
*/
int main (int argc, char *argv[])
{
	char log_file_name[FILENAME_MAX] = "";
	char config_file_name[FILENAME_MAX] = "";
	strcpy(config_file_name, argv[0]);
	strcat(config_file_name, ".conf");
	static const char *optString = "Cd:h?LRs:t:";

	struct config_t config;

	// set default values for command line/config options
	config.close_tty_file = false;
	strcpy(config.device,"");
	config.write_log = false;
	strcpy(log_file_name, argv[0]);
	strcat(log_file_name,".log");
	strcpy(config.log_file_name, log_file_name);
	config.reset_amp_hrs = false;
	config.sensor_mask =  // default sensor bit mask is for a one battery system w/o equalizing (an AGM battery bank)
		PENTAMETRIC_BATTERY1_VOLTS |   
		/*
		PENTAMETRIC_BATTERY2_VOLTS | 
		*/
		PENTAMETRIC_AVERAGE_BATTERY1_VOLTS |
		/*
		PENTAMETRIC_AVERAGE_BATTERY2_VOLTS |
		*/
		PENTAMETRIC_AMPS1 | 
		PENTAMETRIC_AMPS2 |
		PENTAMETRIC_AMPS3 |
		PENTAMETRIC_AVERAGE_AMPS1 |
		PENTAMETRIC_AVERAGE_AMPS2 |
		PENTAMETRIC_AVERAGE_AMPS3 |
		PENTAMETRIC_AMP_HOURS1 |
		PENTAMETRIC_AMP_HOURS2 |
		PENTAMETRIC_AMP_HOURS3 |
		PENTAMETRIC_CUM_AMP_HOURS1 |
		PENTAMETRIC_CUM_AMP_HOURS2 |
		PENTAMETRIC_WATTS1 |
		PENTAMETRIC_WATTS2 |
		PENTAMETRIC_WATT_HOURS1 |
		PENTAMETRIC_WATT_HOURS2 |
		PENTAMETRIC_BATTERY1_PERCENT_FULL |
		/*
		PENTAMETRIC_BATTERY2_PERCENT_FULL |
		*/
		PENTAMETRIC_DAYS_SINCE_BATTERY1_CHARGED |
		/*
		PENTAMETRIC_DAYS_SINCE_BATTERY2_CHARGED |
		PENTAMETRIC_DAYS_SINCE_BATTERY1_EQUALIZED |
		PENTAMETRIC_DAYS_SINCE_BATTERY2_EQUALIZED |
		*/
		PENTAMETRIC_TEMPERATURE
		;
	config.sleep_seconds = 60; // 1 min is default sleep time;


	FILE *ttyfile;

	uint32_t mh_data_id = 0;
	const char mh_data_fmt[] = "data%d %d\n";
	uint32_t mh_temp_id = 0;
	const char mh_temp_fmt[] = "t%d %d\n";

	uint8_t shunt_select = 0;
	const char a100[] = "100A"; // desc for 100A shunt
	const char a500[] = "500A"; // desc for 500A shunt
	uint8_t shunt_labels = 0;
	const char aBattery[] = "Battery"; // desc for Battery (dis-charge) shunt
	const char aNonBattery[] = "Non-Battery"; // desc for Source (charge) shunt
	char *message_buffer;
	int set_tty_error_code = 0;
	time_t seconds_since_midnight = 0;

	// get cofig options
	if(!get_configuration(&config, config_file_name))
	{
		fprintf(stderr,"\nno readable .conf file found, using values from command line arguments");
	}

	// get command line options, will override values read from .conf file
	int opt = 0;	
	while ((opt = getopt(argc, argv ,optString)) != -1)
	{
		switch(opt)
		{
		case 'C':
			config.close_tty_file = true;
			break;
		case 'd':
			strcpy(config.device, optarg);
			break;
		case 'h':
		case '?':
			display_usage(argv[0]);
			break;
		case 'L':
			config.write_log = true;
			break;
		case 'R':
			config.reset_amp_hrs = true;
			break;
		case 's':
			config.sensor_mask = (uint32_t)strtol(optarg, (char **)NULL, 0);
			break;
		case 't':
			config.sleep_seconds = (uint16_t)atoi(optarg);
			break;

		}
	}

	message_buffer = (char *)malloc(sizeof(char) * 256);
	if (message_buffer == NULL)
	{
		fprintf(stderr, "can't allocate dynamic memory for buffers\n");
		return -2;
	}

	if(strlen(config.device) == 0) // can't run when no device is specified
	{
		display_usage(argv[0]);
		return -1;
	}


#ifdef DEBUG
	sprintf(message_buffer, "sensor bitmask = 0x%x", config.sensor_mask);
	writelog(config.log_file_name, argv[0], message_buffer);
#endif

	ttyfile = fopen(config.device, "ab+");

	if (!isatty(fileno(ttyfile)))
	{
		if(config.write_log)
		{
			sprintf(message_buffer, "%s is not a tty", config.device);
			writelog(config.log_file_name, argv[0], message_buffer);
		}
		return 1;
	}

	// set tty port
	if((set_tty_error_code = set_tty_port(ttyfile, config.device, argv[0], config.log_file_name, config.write_log)))
	{
		if(config.write_log)
		{
			sprintf(message_buffer,"Error setting serial port: %d", set_tty_error_code);
			writelog(config.log_file_name, argv[0], message_buffer);
		}
		return 2;
	}

	// log pentametric firmware version
	if(config.write_log)
	{
		sprintf(message_buffer,"Pentametric Firmware version: V%-.1f", get_firmware_version(ttyfile) / 10.0); 
		writelog(config.log_file_name, argv[0], message_buffer);
	}

	// log pentametric shunt configutation
	shunt_select = get_shunt_select(ttyfile);
	if(config.write_log)
	{	
		sprintf(message_buffer,"Pentametric Shunt Select: Shunt-1 %s, Shunt-2 %s, Shunt-3 %s", 
			(shunt_select & SHUNT1_500A? a500: a100),
			(shunt_select & SHUNT2_500A? a500: a100),
			(shunt_select & SHUNT3_500A? a500: a100)); 
		writelog(config.log_file_name, argv[0], message_buffer);
	}

	// log pentametric shunt labels
	shunt_labels = get_shunt_labels(ttyfile);
	if(config.write_log)
	{	
		sprintf(message_buffer, "Pentametric Shunt Labels: Shunt-1 %s, Shunt-2 %s, Shunt-3 %s",
			(shunt_labels & SHUNT1_BATTERY? aBattery: aNonBattery),
			(shunt_labels & SHUNT2_BATTERY? aBattery: aNonBattery),
			(shunt_labels & SHUNT3_BATTERY? aBattery: aNonBattery));
		writelog(config.log_file_name, argv[0], message_buffer);

		sprintf(message_buffer, "Started Pentametric data logging main loop. Polling at %d sec intervals.", config.sleep_seconds);
		writelog(config.log_file_name, argv[0], message_buffer);
	}
	
	seconds_since_midnight = get_seconds_since_midnight();
	if(config.sleep_seconds - (seconds_since_midnight % config.sleep_seconds) > 0)
	{
		sprintf(message_buffer,"Initial sleep: %ld", config.sleep_seconds - (seconds_since_midnight % config.sleep_seconds));
		writelog(config.log_file_name, argv[0], message_buffer);
		sleep(config.sleep_seconds - (seconds_since_midnight % config.sleep_seconds)); // start polling on an even boundry of the specified polling interval
	}
	do
	{
		// 0 - battery 1 volts
		if (config.sensor_mask & PENTAMETRIC_BATTERY1_VOLTS)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format1_value(ttyfile, PENTAMETRIC_ADDRESS_BATTERY1_VOLTS));
		// 1 - battery 2 volts
		if (config.sensor_mask & PENTAMETRIC_BATTERY2_VOLTS)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format1_value(ttyfile, PENTAMETRIC_ADDRESS_BATTERY2_VOLTS));
		// 2 - average battery 1 volts 
		if (config.sensor_mask & PENTAMETRIC_AVERAGE_BATTERY1_VOLTS)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format1_value(ttyfile, PENTAMETRIC_ADDRESS_AVERAGE_BATTERY1_VOLTS));
		// 3 - average battery 2 volts
		if (config.sensor_mask & PENTAMETRIC_AVERAGE_BATTERY2_VOLTS)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format1_value(ttyfile, PENTAMETRIC_ADDRESS_AVERAGE_BATTERY2_VOLTS));
		// 4 - amps 1
		if (config.sensor_mask & PENTAMETRIC_AMPS1)
		{
			if(shunt_select & SHUNT1_500A) // 500A shunt?
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2_value(ttyfile, PENTAMETRIC_ADDRESS_AMPS1));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_AMPS1)); //500A shunt is only good  to 1/10 amp
		}
		// 5 - amps 2
		if (config.sensor_mask & PENTAMETRIC_AMPS2)
		{
			if(shunt_select & SHUNT2_500A) // 500A shunt?
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2_value(ttyfile, PENTAMETRIC_ADDRESS_AMPS2));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_AMPS2)); //500A shunt is only good  to 1/10 amp
		}
		// 6 - amps 3
		if (config.sensor_mask & PENTAMETRIC_AMPS3)
		{
			if(shunt_select & SHUNT3_500A) // 500A shunt?
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2_value(ttyfile, PENTAMETRIC_ADDRESS_AMPS3));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_AMPS3)); //500A shunt is only good  to 1/10 amp
		}
		// 7 - average amps 1
		if (config.sensor_mask & PENTAMETRIC_AVERAGE_AMPS1)
		{
			if(shunt_select & SHUNT1_500A) // 500A shunt?
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2_value(ttyfile, PENTAMETRIC_ADDRESS_AVERAGE_AMPS1));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_AVERAGE_AMPS1));
		}
		// 8 - average amps 2
		if (config.sensor_mask & PENTAMETRIC_AVERAGE_AMPS2)
		{
			if(shunt_select & SHUNT2_500A) // 500A shunt?
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2_value(ttyfile, PENTAMETRIC_ADDRESS_AVERAGE_AMPS2));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_AVERAGE_AMPS2));
		}
		// 9 - average amps 3
		if (config.sensor_mask & PENTAMETRIC_AVERAGE_AMPS3)
		{
			if(shunt_select & SHUNT3_500A) // 500A shunt?
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2_value(ttyfile, PENTAMETRIC_ADDRESS_AVERAGE_AMPS3));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_AVERAGE_AMPS3));
		}
		// 10 - amp hours 1
		if (config.sensor_mask & PENTAMETRIC_AMP_HOURS1)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_AMP_HOURS1));
		// 11 - amp hours 2
		if (config.sensor_mask & PENTAMETRIC_AMP_HOURS2)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_AMP_HOURS2));
		// 12 - amp hours 3
		if (config.sensor_mask & PENTAMETRIC_AMP_HOURS3)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format4_value(ttyfile, PENTAMETRIC_ADDRESS_AMP_HOURS3));
		// 13 - cum amp hours 1
		if (config.sensor_mask & PENTAMETRIC_CUM_AMP_HOURS1)
		{
			if(shunt_select & SHUNT1_500A) // 500A shunt?			
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2b_value(ttyfile, PENTAMETRIC_ADDRESS_CUM_AMP_HOURS1));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3b_value(ttyfile, PENTAMETRIC_ADDRESS_CUM_AMP_HOURS1));
		}
		// 14 - cum amp hours 2
		if (config.sensor_mask & PENTAMETRIC_CUM_AMP_HOURS2)
		{
			if(shunt_select & SHUNT2_500A) // 500A shunt?			
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2b_value(ttyfile, PENTAMETRIC_ADDRESS_CUM_AMP_HOURS2));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3b_value(ttyfile, PENTAMETRIC_ADDRESS_CUM_AMP_HOURS2));
		}
		// 15 - watts 1
		if (config.sensor_mask & PENTAMETRIC_WATTS1)
		{
			if(shunt_select & SHUNT1_500A) // 500A shunt?
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2_value(ttyfile, PENTAMETRIC_ADDRESS_WATTS1));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_WATTS1));
		}
		// 16 - watts 2
		if (config.sensor_mask & PENTAMETRIC_WATTS2)
		{
			if(shunt_select & SHUNT2_500A) // 500A shunt?
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format2_value(ttyfile, PENTAMETRIC_ADDRESS_WATTS2));
			else
				fprintf(stdout, mh_data_fmt, mh_data_id++, get_format3_value(ttyfile, PENTAMETRIC_ADDRESS_WATTS2));
		}
		// 17 - watts hours 1
		if (config.sensor_mask & PENTAMETRIC_WATT_HOURS1)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format5_value(ttyfile, PENTAMETRIC_ADDRESS_WATT_HOURS1));
		// 18 - watts hours 2
		if (config.sensor_mask & PENTAMETRIC_WATT_HOURS2)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format5_value(ttyfile, PENTAMETRIC_ADDRESS_WATT_HOURS2));
		// 19 - battery 1 percent full
		if (config.sensor_mask & PENTAMETRIC_BATTERY1_PERCENT_FULL)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format6_value(ttyfile, PENTAMETRIC_ADDRESS_BATTERY1_PERCENT_FULL));		
		// 20 - battery 2 percent full
		if (config.sensor_mask & PENTAMETRIC_BATTERY2_PERCENT_FULL)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format6_value(ttyfile, PENTAMETRIC_ADDRESS_BATTERY2_PERCENT_FULL));		
		// 21 - days since battery 1 charged
		if (config.sensor_mask & PENTAMETRIC_DAYS_SINCE_BATTERY1_CHARGED)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format7_value(ttyfile, PENTAMETRIC_ADDRESS_DAYS_SINCE_BATTERY1_CHARGED));		
		// 22 - days since battery 2 charged
		if (config.sensor_mask & PENTAMETRIC_DAYS_SINCE_BATTERY2_CHARGED)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format7_value(ttyfile, PENTAMETRIC_ADDRESS_DAYS_SINCE_BATTERY2_CHARGED));		
		// 23 - days since battery 1 equalized
		if (config.sensor_mask & PENTAMETRIC_DAYS_SINCE_BATTERY1_EQUALIZED)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format7_value(ttyfile, PENTAMETRIC_ADDRESS_DAYS_SINCE_BATTERY1_EQUALIZED));		
		// 24 - days since battery 2 equalized
		if (config.sensor_mask & PENTAMETRIC_DAYS_SINCE_BATTERY2_EQUALIZED)
			fprintf(stdout, mh_data_fmt, mh_data_id++, get_format7_value(ttyfile, PENTAMETRIC_ADDRESS_DAYS_SINCE_BATTERY2_EQUALIZED));		
		// 25 - temperature
		if (config.sensor_mask & PENTAMETRIC_TEMPERATURE)
			fprintf(stdout, mh_temp_fmt, mh_temp_id++, get_format8_value(ttyfile, PENTAMETRIC_ADDRESS_TEMPERATURE));

		mh_data_id = 0;
		mh_temp_id = 0;
		fflush(stdout);

		if(config.close_tty_file) // close tty file
			fclose(ttyfile);

		seconds_since_midnight = get_seconds_since_midnight();

		if((86400 - seconds_since_midnight) >= config.sleep_seconds)
		{
			sleep(config.sleep_seconds - (seconds_since_midnight % config.sleep_seconds)); // sleep just the right amount to keep on boundry
			//sleep(config.sleep_seconds); // sleep
		}
		else
		{
			if((86400 - seconds_since_midnight) <= config.sleep_seconds)
			{
				sleep(86400 - seconds_since_midnight); // sleep just right amount until midnight
			}

			if(config.reset_amp_hrs)
			{
				if(config.write_log)
				{
					sprintf(message_buffer, "Resetting non-battery shunt Amp Hours at %lld seconds after 00:00:00", (int64_t)seconds_since_midnight);
					writelog(config.log_file_name, argv[0], message_buffer);
				}

				if(config.close_tty_file) // open tty back up
				{
					ttyfile = fopen(config.device, "ab+");
					// set tty port
					if((set_tty_error_code = set_tty_port(ttyfile, config.device, argv[0], config.log_file_name, config.write_log)))
					{
						if(config.write_log)
						{
							sprintf(message_buffer,"Error setting serial port: %d", set_tty_error_code);
							writelog(config.log_file_name, argv[0], message_buffer);
						}
						return 2;
					}
				}

				// send command to pentametric to reset all non-battery amp hour values to zero just after midnight local time
				if(!reset_amp_hours(ttyfile, shunt_labels))
				{
					if(config.write_log)
					{
						sprintf(message_buffer,"Error resetting Pentametric Amp Hour values for non-battery shunts");
						writelog(config.log_file_name, argv[0], message_buffer);
					}
				}	
				else
				{
					sprintf(message_buffer,"Reset Pentametric Amp Hour values for non-battery shunts");
					writelog(config.log_file_name, argv[0], message_buffer);
				}

				if(config.close_tty_file) // close tty file
					fclose(ttyfile);
			}
		}

		if(config.close_tty_file) // open tty back up
		{
			ttyfile = fopen(config.device, "ab+");
			// set tty port
			if((set_tty_error_code = set_tty_port(ttyfile, config.device, argv[0], config.log_file_name, config.write_log)))
			{
				if(config.write_log)
				{
					sprintf(message_buffer,"Error setting serial port: %d", set_tty_error_code);
					writelog(config.log_file_name, argv[0], message_buffer);
				}
				return 2;
			}
		}
	}
	while(!feof(ttyfile));

	fclose(ttyfile);
	free (message_buffer);

	return 0;
}

/*
function bodies
*/

boolean pentametric_short_read(FILE *stream, uint8_t a, uint8_t n, uint8_t *msg)
{
	uint8_t cs, i;

#ifdef DEBUG
	fprintf(stderr,"short_read command = 0x%hx\n", a);
	fprintf(stderr,"short_read byte count = 0x%hx\n", n);
#endif

	fputc(PENTAMETRIC_SHORT_READ_COMMAND, stream);
	fputc(a, stream);
	fputc(n, stream);

	cs = PENTAMETRIC_SHORT_READ_COMMAND + a + n;

#ifdef DEBUG
	fprintf(stderr,"short_read command checksum = 0x%hx\n", (uint8_t)(cs + ~cs)); // should be 0xff
#endif

	fputc(~cs, stream); // remainder needed to add up to PENTAMETRIC_CHECKSUM
	cs = 0;

	for(i=0;i<n;i++)
	{
		msg[i] = fgetc(stream);
#ifdef DEBUG
		fprintf(stderr, "short_read data byte %d = 0x%hx\n", i, msg[i]);
#endif
		cs += (uint8_t)msg[i];
	}

	cs += (uint8_t)fgetc(stream); // get packet cheksum

#ifdef DEBUG
	fprintf(stderr,"short_read data checksum = 0x%hx\n", cs);
	if (cs != PENTAMETRIC_CHECKSUM)
		fprintf(stderr, "short_read chacksum error for command 0x%x\n", a);
#endif

	return (cs == PENTAMETRIC_CHECKSUM);
}

boolean pentametric_short_write(FILE *stream, uint8_t a, uint8_t n, uint8_t *msg)
{
	uint8_t cs, pm_cs, i;

	fputc(PENTAMETRIC_SHORT_WRITE_COMMAND, stream);
	fputc(a, stream);
	fputc(n, stream);
	cs = PENTAMETRIC_SHORT_WRITE_COMMAND + a + n;

	for(i=0;i<n;i++)
	{
		fputc(msg[i], stream);
		cs += (uint8_t)msg[i];
#ifdef DEBUG
		fprintf(stderr, "short_write byte: 0x%hx\n", msg[i]);
#endif
	}

	fputc(~(cs & PENTAMETRIC_CHECKSUM), stream); // loworder byte remainder needed to add up to PENTAMETRIC_CHECKSUM
#ifdef DEBUG
	fprintf(stderr, "short_write data checksum = 0x%x\n", (~(cs & PENTAMETRIC_CHECKSUM)) & PENTAMETRIC_CHECKSUM);
#endif
	pm_cs = (uint8_t)fgetc(stream);
#ifdef DEBUG
	fprintf(stderr, "short_write pentametric checksum: 0x%x\n", pm_cs);
#endif
	return(((~(uint8_t)(cs & PENTAMETRIC_CHECKSUM)) & PENTAMETRIC_CHECKSUM) == pm_cs);
}

int32_t get_format1_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[2];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format1(msg);
	else
		return -SHRT_MAX;
}

int32_t get_format2_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[3];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format2(msg);
	else
		return -SHRT_MAX;
}

int32_t get_format2b_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[3];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format2b(msg);
	else
		return -SHRT_MAX;
}

int32_t get_format3_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[3];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format3(msg);
	else
		return -SHRT_MAX;
}

int32_t get_format3b_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[3];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format3b(msg);
	else
		return -SHRT_MAX;
}

int32_t get_format4_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[4];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format4(msg);
	else
		return -SHRT_MAX;
}

int32_t get_format5_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[4];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format5(msg);
	else
		return -SHRT_MAX;
}

int32_t get_format6_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[1];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format6(msg);
	else
		return -SHRT_MAX;
}

int32_t get_format7_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[2];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format7(msg);
	else
		return -SHRT_MAX;
}

int32_t get_format8_value(FILE *stream, uint8_t pentametric_address)
{
	uint8_t msg[1];
	if(pentametric_short_read(stream, pentametric_address, sizeof(msg), msg))
		return decode_format8(msg);
	else
		return -SHRT_MAX;
}

// read pentametric shunt configuration
uint8_t get_shunt_select(FILE *stream)
{
	uint8_t msg[4];
	uint8_t tmp8u = 0;

	if(pentametric_short_read(stream, PENTAMETRIC_ADDRESS_SHUNT_SELECT, sizeof(msg), msg))
	{
		// pack 3 byte 1 bit bitmasks into 1 byte 3 bit bitmask for all three shunts
		if(msg[0] & 0x10) // is shunt 1 500 Amp?
			tmp8u = SHUNT1_500A;

		if(msg[1] & 0x10) // is shunt 2 500 Amp?
			tmp8u |= SHUNT2_500A;

		if(msg[2] & 0x10) // is shunt 3 500 Amp?
			tmp8u |= SHUNT3_500A;
	}
	else
		tmp8u = 0x80; // set high bit if read failed

	return tmp8u;
}

// read pentametric shunt labels
uint8_t get_shunt_labels(FILE *stream)
{
	uint8_t msg[3]; // 3 bytes
	uint8_t tmp8u = 0;

	if(pentametric_short_read(stream, PENTAMETRIC_ADDRESS_SHUNT_LABELS, sizeof(msg), msg))
	{
#ifdef DEBUG
		fprintf(stderr, "Shunt Labels raw value, byte 0: 0x%hx byte 1: 0x%hx byte 2: 0x%hx\n", msg[0], msg[1], msg[2]);
#endif
		// pack 3 byte 4 bit bitmasks into 1 byte 3 bit bitmask for all three shunts
		if(((msg[0] & 0x05) == 0x05) || ((msg[0] & 0x06) == 0x06)) // amps1 shunt is for battery or battery1?
			tmp8u = SHUNT1_BATTERY;

		if(((msg[1] & 0x05) == 0x05) || ((msg[1] & 0x06) == 0x06)) // amps2 shunt is for battery or battery1?
			tmp8u |= SHUNT2_BATTERY;

		if(((msg[2] & 0x05) == 0x05) || ((msg[2] & 0x06) == 0x06)) // amps3 shunt is for battery or battery1?
			tmp8u |= SHUNT3_BATTERY;
	}
	else
		tmp8u = 0x80; // set high bit if read failed
#ifdef DEBUG
	fprintf(stderr, "Packed shunt labels value: 0x%hx\n", tmp8u);
#endif
	return tmp8u;
}

// reset pentametric amp hours for non-battery shunts
boolean reset_amp_hours(FILE *stream, uint8_t shunt_labels)
{
	boolean return_value = false;
	uint8_t msg[1];

	if(!(shunt_labels & SHUNT1_BATTERY)) // reset shunt1 amps and watt hrs
	{
		msg[0] = PENTAMETRIC_CLEAR_AMP_HOURS_1;
		return_value = pentametric_short_write(stream, PENTAMETRIC_ADDRESS_RESET, sizeof(msg), msg);
		msg[0] = PENTAMETRIC_CLEAR_WATT_HOURS1;
		return_value |= pentametric_short_write(stream, PENTAMETRIC_ADDRESS_RESET, sizeof(msg), msg);
	}
	if(!(shunt_labels & SHUNT2_BATTERY)) // reset shunt2 amps and watt hrs
	{
		msg[0] = PENTAMETRIC_CLEAR_AMP_HOURS_2;
		return_value = pentametric_short_write(stream, PENTAMETRIC_ADDRESS_RESET, sizeof(msg), msg);
		msg[0] = PENTAMETRIC_CLEAR_WATT_HOURS2;
		return_value |= pentametric_short_write(stream, PENTAMETRIC_ADDRESS_RESET, sizeof(msg), msg);
	}
	if(!(shunt_labels & SHUNT3_BATTERY)) // reset shunt3 amps hrs
	{
		msg[0] = PENTAMETRIC_CLEAR_AMP_HOURS_3;
		return_value |= pentametric_short_write(stream, PENTAMETRIC_ADDRESS_RESET, sizeof(msg), msg);
	}
	return return_value;
}

// read pentametric firmware version
uint8_t get_firmware_version(FILE *stream)
{
	uint8_t msg[1];
	uint8_t tmp8u = 0;

	if(pentametric_short_read(stream, PENTAMETRIC_ADDRESS_FIRMWARE_VERSION, sizeof(msg), msg))
	{
		tmp8u = msg[0];
	}
	return tmp8u;
}

// decode Volts
int32_t decode_format1(uint8_t *msg)
{
	uint16_t tmp16u;

	// pack byte data into 16 bits
	tmp16u = (uint16_t)msg[1];
	tmp16u <<= 8;
	tmp16u |= (uint16_t)msg[0];

	tmp16u &= 0x7ff;      // get low 11 bits

	return (tmp16u * 5) ; // divide by 20 for pentametric value then muliply by 100 for meteohub data value = multiply raw pentametric value by 5
}

// decode Amps for 500 Amp shunt
int32_t decode_format2(uint8_t *msg)
{
	return (((int)(decode_format3(msg) / 10)) * 10); // remove 0.01 digit as resolution is only 0.1 Amp for 500 Amp shunt
}

// decode cumulative Amp hours 500A shunt
int32_t decode_format2b(uint8_t *msg)
{
	return decode_format2 (msg) * 100; // same as format2 but * 100
}

// decode Amps for 100 Amp shunt
int32_t decode_format3(uint8_t *msg)
{

	int32_t  tmp32 = 0;
	uint32_t tmp32u = 0;
#ifdef DEBUG
	fprintf(stderr, "decode_format3 raw data = msg[0]0x%hx, msg[1]0x%hx, msg[2]0x%hx\n", msg[0], msg[1], msg[2]);
#endif
	// pack byte data into 24 bits
	tmp32u = (uint32_t)msg[2];
	tmp32u <<= 8;
	tmp32u |= (uint32_t)msg[1];
	tmp32u <<= 8;
	tmp32u |= (uint32_t)msg[0];
#ifdef DEBUG
	fprintf(stderr, "decode_format3.tmp32u after packing = 0x%x\n", tmp32u);
#endif
	if(tmp32u & 0x800000) // bit 23 set? (means a "charging" value coming from the pentametric)
	{
#ifdef DEBUG
		fprintf(stderr, "decode_format3 found bit 23 set\n");
#endif	
		tmp32u = ~tmp32u; // invert all bits
		tmp32u &= 0x7fffff;  // get bits 22-0, leave bit 32 behind
#ifdef DEBUG
		fprintf(stderr, "decode_format3.tmp32u after inverting and masking bits 22-0 = 0x%x\n", tmp32u);
#endif
		tmp32 = (uint32_t)tmp32u;
	}
	else // bit 23 is not set (means a "discharging" value coming from the pentametric
		tmp32 = -(uint32_t)tmp32u; // and negate.

#ifdef DEBUG
	fprintf(stderr, "decode_format3.tmp32 return value = 0x%x\n", tmp32);
#endif	
	return tmp32; // already in 1/100 units so don't * 100 for meteohub
}

// decode cumulative Amp hours 100A shunt
int32_t decode_format3b(uint8_t *msg)
{
	return decode_format3 (msg) * 100; // same as format3 but * 100
}
// decode Amp hours 3 only
int32_t decode_format4(uint8_t *msg)
{

	int32_t  tmp32 = 0;
	uint32_t tmp32u = 0;

	// pack byte data into 32 bits
	tmp32u = (uint32_t)msg[3];
	tmp32u <<= 8;
	tmp32u |= (uint32_t)msg[2];
	tmp32u <<= 8;
	tmp32u |= (uint32_t)msg[1];
	tmp32u <<= 8;
	tmp32u |= (uint32_t)msg[0];

	if(tmp32u & 0x8000000) // bit 31 set? (means a "charging" value coming from the pentametric)
	{
		tmp32u = ~tmp32u; // invert all bits
		tmp32u &= 0x7fffff80; // get bits 30-7
		tmp32u >>= 7; // shift right to strip bits 0-6
		tmp32 = (uint32_t)tmp32u; 
	}
	else // bit 31 is not set (means a "discharging" value coming from the pentametric
	{
		tmp32u &= 0x7fffff80; // get bits 30-7
		tmp32u >>= 7; // shift right to strip bits 0-6
		tmp32 = -(uint32_t)tmp32u; // and negate
	}
	return tmp32; // already in 1/100 units so don't * 100 for meteohub
}

// decode Watt hours
int32_t decode_format5(uint8_t *msg)
{

	int32_t  tmp32 = 0;
	uint32_t tmp32u = 0;

	// pack byte data into 32 bits
	tmp32u = (uint32_t)msg[3];
	tmp32u <<= 8;
	tmp32u |= (uint32_t)msg[2];
	tmp32u <<= 8;
	tmp32u |= (uint32_t)msg[1];
	tmp32u <<= 8;
	tmp32u |= (uint32_t)msg[0];

	if(tmp32u & 0x8000000) // bit 31 set? (means a "charging" value coming from the pentametric)
	{
		tmp32u = ~tmp32u; // invert all bits
		tmp32 = (uint32_t)tmp32u ; 
	}
	else // bit 31 is not set (means a "discharging" value coming from the pentametric
		tmp32 = -(uint32_t)tmp32u; // and negate

	return tmp32; // already in 1/100 units so don't * 100 for meteohub
}

// decode Percent battery full
int32_t decode_format6(uint8_t *msg)
{
	return (((uint32_t)msg[0]) * 100); // one data byte, convert to int and * 100 for meteohub data value
}

// decode Days since battery charged
int32_t decode_format7(uint8_t *msg)
{
	int32_t  tmp32 = 0;
	uint32_t tmp32u = 0;

	// pack byte data into 16 bits
	tmp32u = (uint32_t)msg[1];
	tmp32u <<= 8;
	tmp32u |= (uint32_t)msg[0];

	tmp32 = (int32_t)tmp32u;

	return tmp32; // already in 1/100 units so don't * 100 for meteohub
}

// decode Temp in deg C
int16_t decode_format8(uint8_t *msg)
{

#ifdef DEBUG
		fprintf(stderr, "decode_format8 raw value 0x%hx\n", msg[0]);
#endif
// FTRIMBLE 10-Sept-2013 Changed logic to get sign correct on temps
// FTRIMBLE 24-Nov-2013  Fixed 2's complimnet conversion for negitive numbers
	return (((int8_t)msg[0]) * 10); // temps in meteohub are pentametric raw value * 10
}


// set serial port to communicate with pentametric
int set_tty_port(FILE *ttyfile, char *device, char* myname, char *log_file_name, boolean writetolog)
{
	struct termios config;
	char *message_buffer;

	message_buffer = (char *)malloc(sizeof(char) * 256);
	if (message_buffer == NULL)
	{
		fprintf(stderr, "can't allocate dynamic memory for buffers\n");
		return -1;
	}

	if (tcgetattr(fileno(ttyfile), &config) < 0)
	{
		if(writetolog)
		{
			sprintf(message_buffer,"could not get termios attributes for %s", device);
			writelog(log_file_name, myname, message_buffer);
		}
		return -2;
	}

	// set serial port to 2400 baud, 8 bits, no parity
	config.c_iflag = 0;
	config.c_oflag = 0;
	config.c_cflag = CLOCAL | CREAD;
	config.c_lflag = 0;
	cfmakeraw(&config);
	cfsetispeed(&config,B2400);
	cfsetospeed(&config,B2400);
	config.c_cc[VMIN] = 1;
	config.c_cc[VTIME] = 0;


	if(tcsetattr(fileno(ttyfile), TCSANOW, &config) < 0)
	{
		if(writetolog)
		{
			sprintf(message_buffer, "could not set termios attributes for %s", device);
			writelog(log_file_name, myname, message_buffer);
		}
		return -3;
	}
	else
	{
		if(writetolog)
		{
			sprintf(message_buffer, "Set serial port on device %s to 2400 Baud", device);
			writelog(log_file_name, myname, message_buffer);
		}
		return 0;
	}
}

// get seconds since midnight local time
uint32_t get_seconds_since_midnight (void)
{
	time_t t;
	struct tm *localtm;

	t = time(NULL);
	localtm = localtime(&t);

	return localtm->tm_sec + localtm->tm_min * 60 + localtm->tm_hour * 3600;
}

// wrire formatted messages to a log file named in logfilename
void writelog (char *logfilename, char *process_name, char *message)
{
	char timestamp[25];
	time_t t;
	struct tm *localtm;
	FILE *stream;

	t = time(NULL);
	localtm = localtime(&t);

	strftime(timestamp, sizeof(timestamp), "%d.%m.%Y %T", localtm);

	stream = fopen(logfilename, "a");
	fprintf(stream, "%s (%s): %s.\n", process_name, timestamp, message);
	fprintf(stderr, "%s (%s): %s.\n", process_name, timestamp, message);
	fclose(stream);
}

void display_usage(char *myname)
{
	fprintf(stderr, "mhpmpi Version %s - Meteohub Plug-In for Bogart Engineering Pentametric PM-100-C RS-232 computer interface.\n", VERSION);
	fprintf(stderr, "Usage: %s -d tty_device [-C] [-L] [-R] [-s sensor_mask] [-t sleep_time]\n", myname);
	fprintf(stderr, "  -d tty_device  /dev/tty[x] device name where USB to Serial adapeter is connected.\n");
	fprintf(stderr, "  -C             Close/reopen tty device between polls.\n");
	fprintf(stderr, "  -L             Write messages to log file.\n");
	fprintf(stderr, "  -R             Reset Amp Hours at midnight for Shunts labeled as non-Battery.\n");
	fprintf(stderr, "  -s sensor_mask Bitmask value in hex (0x00) or decimal format to identify\n");
	fprintf(stderr, "                 which Pentametric data values to log as Meteohub sensors.\n");
	fprintf(stderr, "  -t sleep_time  Number of seconds to sleep between polling the Pentametric.\n");
	exit(EXIT_FAILURE);
}
