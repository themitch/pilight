/*
	Copyright (C) 2013 CurlyMo

	This file is part of pilight.

    pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

    pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>

#include "pilight.h"
#include "common.h"
#include "settings.h"
#include "log.h"
#include "options.h"
#include "socket.h"
#include "json.h"
#include "protocol.h"
#include "ssdp.h"

typedef enum {
	WELCOME,
	IDENTIFY,
	REJECT,
	SEND
} steps_t;

int main(int argc, char **argv) {

	log_file_disable();
	log_shell_enable();
	log_level_set(LOG_NOTICE);

	progname = malloc(13);
	strcpy(progname, "pilight-send");

	struct options_t *options = NULL;
	struct ssdp_list_t *ssdp_list = NULL;

	int sockfd = 0;
    char *recvBuff = NULL;
    char *message;
	char *args = NULL;
	steps_t steps = WELCOME;

	/* Hold the name of the protocol */
	char protobuffer[25] = "\0";
	/* Does this protocol exists */
	int match = 0;

	/* Do we need to print the help */
	int help = 0;
	/* Do we need to print the version */
	int version = 0;
	/* Do we need to print the protocol help */
	int protohelp = 0;

	char *server = NULL;
	unsigned short port = 0;

	/* Hold the final protocol struct */
	protocol_t *protocol = NULL;

	JsonNode *json = json_mkobject();
	JsonNode *code = json_mkobject();

	/* Define all CLI arguments of this program */
	options_add(&options, 'H', "help", no_value, 0, NULL);
	options_add(&options, 'V', "version", no_value, 0, NULL);
	options_add(&options, 'p', "protocol", has_value, 0, NULL);
	options_add(&options, 'S', "server", has_value, 0, "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	options_add(&options, 'P', "port", has_value, 0, "[0-9]{1,4}");

	/* Initialize protocols */
	protocol_init();

	/* Get the protocol to be used */
	while (1) {
		int c;
		c = options_parse(&options, argc, argv, 0, &args);

		if(c == -1 || c == -2)
			break;
		switch(c) {
			case 'p':
				if(strlen(args) == 0) {
					logprintf(LOG_ERR, "options '-p' and '--protocol' require an argument");
					exit(EXIT_FAILURE);
				} else {
					strcpy(protobuffer, args);
				}
			break;
			case 'V':
				version = 1;
			break;
			case 'H':
				help = 1;
			break;
			case 'S':
				server = realloc(server, strlen(args)+1);
				strcpy(server, args);
			break;
			case 'P':
				port = (unsigned short)atoi(args);
			break;
			default:;
		}
	}

	/* Check if a protocol was given */
	if(strlen(protobuffer) > 0 && strcmp(protobuffer,"-V") != 0) {
		if(strlen(protobuffer) > 0 && version) {
			printf("-p and -V cannot be combined\n");
		} else {
			struct protocols_t *pnode = protocols;
			/* Retrieve the used protocol */
			while(pnode) {
				/* Check if the protocol exists */
				protocol = pnode->listener;
				if(protocol_device_exists(protocol, protobuffer) == 0 && match == 0 && protocol->createCode != NULL) {
					match=1;
					/* Check if the protocol requires specific CLI arguments
					   and merge them with the main CLI arguments */
					if(protocol->options && help == 0) {
						options_merge(&options, &protocol->options);
					} else if(help == 1) {
						protohelp=1;
					}
					break;
				}
				pnode = pnode->next;
			}
			/* If no protocols matches the requested protocol */
			if(!match) {
				logprintf(LOG_ERR, "this protocol is not supported");
			}
		}
	}

	/* Store all CLI arguments for later usage
	   and also check if the CLI arguments where
	   used correctly by the user. This will also
	   fill all necessary values in the options struct */
	while(1) {
		int c;
		c = options_parse(&options, argc, argv, 2, &args);

		if(c == -1)
			break;
		if(c == -2) {
			if(match == 1) {
				protohelp = 1;
			} else {
				help = 1;
			}
		break;
		}
	}

	/* Display help or version information */
	if(version == 1) {
		printf("%s %s\n", progname, VERSION);
		goto close;
	} else if(help == 1 || protohelp == 1 || match == 0) {
		if(protohelp == 1 && match == 1 && protocol->printHelp)
			printf("Usage: %s -p %s [options]\n", progname, protobuffer);
		else
			printf("Usage: %s -p protocol [options]\n", progname);
		if(help == 1) {
			printf("\t -H --help\t\t\tdisplay this message\n");
			printf("\t -V --version\t\t\tdisplay version\n");
			printf("\t -p --protocol=protocol\t\tthe protocol that you want to control\n");
			printf("\t -S --server=x.x.x.x\t\tconnect to server address\n");
			printf("\t -P --port=xxxx\t\t\tconnect to server port\n");
		}
		if(protohelp == 1 && match == 1 && protocol->printHelp) {
			printf("\n\t[%s]\n", protobuffer);
			protocol->printHelp();
		} else {
			printf("\nThe supported protocols are:\n");
			struct protocols_t *pnode = protocols;
			/* Retrieve the used protocol */
			while(pnode) {
				protocol = pnode->listener;
				if(protocol->createCode) {
					while(protocol->devices) {
						printf("\t %s\t\t",protocol->devices->id);
						if(strlen(protocol->devices->id)<7)
							printf("\t");
						if(strlen(protocol->devices->id)<15)
							printf("\t");
						printf("%s\n", protocol->devices->desc);
						protocol->devices = protocol->devices->next;
					}
				}
				pnode = pnode->next;
			}
		}
		goto close;
	}

	int itmp;
	/* Check if we got sufficient arguments from this protocol */
	struct options_t *tmp = options;
	while(tmp) {
		if(strlen(tmp->name) > 0) {
			/* Only send the CLI arguments that belong to this protocol, the protocol name
			and those that are called by the user */
			if((options_get_id(&protocol->options, tmp->name, &itmp) == 0)
			&& strlen(tmp->value) > 0) {
				if(isNumeric(tmp->value) == EXIT_SUCCESS) {
					json_append_member(code, tmp->name, json_mknumber(atoi(tmp->value)));
				} else {
					json_append_member(code, tmp->name, json_mkstring(tmp->value));
				}
			}
			if(strcmp(tmp->name, "protocol") == 0 && strlen(tmp->value) > 0) {
				JsonNode *jprotocol = json_mkarray();
				json_append_element(jprotocol, json_mkstring(tmp->value));
				json_append_member(code, "protocol", jprotocol);
			}
		}
		tmp = tmp->next;
	}

	if(protocol->createCode(code) == 0) {
		if(protocol->message) {
			json_delete(protocol->message);
		}
		if(server && port > 0) {
			if((sockfd = socket_connect(server, port)) == -1) {
				logprintf(LOG_ERR, "could not connect to pilight-daemon");
				goto close;
			}
		} else if(ssdp_seek(&ssdp_list) == -1) {
			logprintf(LOG_ERR, "no pilight ssdp connections found");
			goto close;
		} else {
			if((sockfd = socket_connect(ssdp_list->ip, ssdp_list->port)) == -1) {
				logprintf(LOG_ERR, "could not connect to pilight-daemon");
				goto close;
			}
			sfree((void *)&ssdp_list);
		}

		while(1) {
			if(steps > WELCOME) {
				/* Clear the receive buffer again and read the welcome message */
				if((recvBuff = socket_read(sockfd))) {
					json = json_decode(recvBuff);
					json_find_string(json, "message", &message);
				} else {
					goto close;
				}
				usleep(100);
			}
			switch(steps) {
				case WELCOME:
					socket_write(sockfd, "{\"message\":\"client sender\"}");
					steps=IDENTIFY;
				case IDENTIFY:
					if(strcmp(message, "accept client") == 0) {
						steps=SEND;
					}
					if(strcmp(message, "reject client") == 0) {
						steps=REJECT;
					}
				case SEND:
					json_delete(json);
					json = json_mkobject();
					json_append_member(json, "message", json_mkstring("send"));
					json_append_member(json, "code", code);
					char *output = json_stringify(json, NULL);
					socket_write(sockfd, output);
					sfree((void *)&output);
					goto close;
				break;
				case REJECT:
				default:
					goto close;
				break;
			}
		}
	}
close:
	if(json) {
		json_delete(json);
	}
	if(sockfd) {
		socket_close(sockfd);
	}
	if(server) {
		sfree((void *)&server);
	}
	log_shell_disable();
	protocol_gc();
	options_delete(options);
	options_gc();
	log_gc();
	sfree((void *)&progname);

return EXIT_SUCCESS;
}
