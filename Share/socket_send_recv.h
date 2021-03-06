//This module holds functions that wrap winsock2.h api functions for the application-level communication protocol.
//includes formating for communication messeges, which is project-specific.


#ifndef SOCKET_SEND_RECV_H
#define SOCKET_SEND_RECV_H

#include <winsock2.h>

#include "HardCodedData.h"

#pragma comment(lib, "ws2_32.lib")


typedef enum { TRNS_FAILED, TRNS_DISCONNECTED, TRNS_SUCCEEDED } TransferResult_t;
 


int recv_and_extract_communication_message(SOCKET sd, char** communication_message, char* messeage_type, char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS], int client_or_server,
	int* write_from_offset, char log_file_name[MAX_LENGTH_OF_PATH_TO_A_FILE]);
	
int send_message(SOCKET sd, const char* messeage_type, char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS], int client_or_server, int *write_from_offset, char log_file_name[MAX_LENGTH_OF_PATH_TO_A_FILE]);

void free_communication_message_and_parameters(char* communication_message, char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS], char* messeage_type);

int extract_parameters_from_communication_message(char* communication_message, char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS], char* messeage_type);
TransferResult_t recv_communication_message(SOCKET sd, char** communication_message);
int get_size_of_communication_message(char* communication_message);
int format_communication_message(const char* messeage_type, char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS], char** communication_message);
void init_parameter_array(char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS]);

TransferResult_t SendBuffer(const char* Buffer, int BytesToSend, SOCKET sd);

int set_time_out_to_recv_calls(SOCKET accept_socket, int timeout);

#endif