#include "mhpmpi.h"

/********************************************************************
 * get_configuration()
 *
 * read setup parameters from mhpmpi.conf
 * It searches in this sequence:
 * 1. Path to config file including filename given as parameter
 * 2. ./mhpmpi.conf
 * 3. /usr/local/etc/mhpmpi.conf
 * 4. /etc/mhpmpi.conf
 *
 * See file mhpmpi.conf-dist for the format and option names/values
 *
 * input:    config file name with full path - pointer to string
 *
 * output:   struct config populated with valid settings from 
 *           config file 
 *
 * returns:  0 = OK
 *          -1 = no config file or file open error
 *
 ********************************************************************/
int get_configuration(struct config_t *config, char *path)
{
	FILE *fptr;
	char inputline[1000] = "";
	char token[100] = "";
	char val[100] = "";

	// open the config file
	fptr = NULL;
	if (path != NULL)
		fptr = fopen(path, "r"); //try the pathname passed in

	if (fptr == NULL) //then try default search
	{
		if ((fptr = fopen("./mhpmpi.conf", "r")) == NULL)
		{
			if ((fptr = fopen("/usr/local/etc/mhpmpi.conf", "r")) == NULL)
			{
				if ((fptr = fopen("/etc/mhpmpi.conf", "r")) == NULL)
				{
					return(false); // none of the conf files are exist or are readable
				}
			}
		}
	}

	while (fscanf(fptr, "%[^\n]\n", inputline) != EOF)
	{		
		sscanf(inputline, "%[^= \t]%*[ \t=]%s%*[^\n]",  token, val);
		if (token[0] == '#')	// # character starts a comment
			continue;

		if ((strcmp(token,"CLOSE_DEVICE")==0) && (strlen(val) != 0))
		{
			config->close_tty_file = (boolean)atoi(val);
			continue;
		}
		
		if ((strcmp(token,"DEVICE")==0) && (strlen(val) != 0))
		{
			strcpy(config->device,val);
			continue;
		}

		if ((strcmp(token,"WRITE_LOG")==0) && (strlen(val) != 0))
		{
			config->write_log = (boolean)atoi(val);
			continue;
		}

		if ((strcmp(token,"LOG_FILE_NAME")==0) && (strlen(val) != 0))
		{
			strcpy(config->log_file_name,val);
			continue;
		}

		if ((strcmp(token,"RESET_AMP_HRS")==0) && (strlen(val) != 0))
		{
			config->reset_amp_hrs = (boolean)atoi(val);
			continue;
		}

		if ((strcmp(token,"SENSOR_MASK")==0) && (strlen(val) != 0))
		{
			config->sensor_mask = (uint32_t)strtol(val, (char **)NULL, 0);
			continue;
		}

		if ((strcmp(token,"SLEEP_SECONDS")==0) && (strlen(val) != 0))
		{
			config->sleep_seconds = (uint16_t)atoi(val);
			continue;
		}
	}

	return (true);
}
