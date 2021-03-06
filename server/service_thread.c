//includes functions and structs related to operation of a worker thread(which is project specific). 


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>


#include "socket_send_recv.h"
#include "service_thread.h"
#include "file_IO.h"
#include "server_game_loop.h"


int approve_client_request(SOCKET accept_socket, char client_name[MAX_LENGH_OF_CLIENT_NAME], int* write_from_offset_to_log_file, char thread_log_file_name[MAX_LENGTH_OF_THREAD_LOG_FILE_NAME]);
int send_main_menu_to_client_and_try_to_connect_with_another_player(SOCKET accept_socket, int* num_of_player, char client_name[MAX_LENGH_OF_CLIENT_NAME], int* write_from_offset_to_log_file, char thread_log_file_name[MAX_LENGTH_OF_THREAD_LOG_FILE_NAME]);
int check_if_player_connected_first_and_update_num_of_players(int* num_of_player, char client_name[MAX_LENGH_OF_CLIENT_NAME]);
int check_if_two_players_are_ready_to_play_protected(SOCKET accept_socket, int* num_of_player, char client_name[MAX_LENGH_OF_CLIENT_NAME], int* write_from_offset_to_log_file, char thread_log_file_name[MAX_LENGTH_OF_THREAD_LOG_FILE_NAME]);

int initialize_share_resources_to_zero();

extern shared_server_resources resources_struct;
extern HANDLE ghMutex;
extern HANDLE mutex_to_sync_threads_when_waiting_for_players;
extern HANDLE event_for_syncing_threads_in_game_loop;
extern SYNCHRONIZATION_BARRIER barrier;



DWORD ServiceThread(SOCKET* t_socket) {
	SOCKET accept_socket = *t_socket;
	int num_of_player;
	char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS];
	char client_name[MAX_LENGH_OF_CLIENT_NAME];

	int write_from_offset_to_log_file = 0;
	char thread_log_file_name[MAX_LENGTH_OF_THREAD_LOG_FILE_NAME];
	int error = 0;
		
	if (approve_client_request(accept_socket, client_name, &write_from_offset_to_log_file, thread_log_file_name) == ERROR_CODE) {

		return ERROR_CODE;
	}
	
	//while thread is still operational, client can enter games.
	
	while (1) {
		if (send_main_menu_to_client_and_try_to_connect_with_another_player(accept_socket, &num_of_player, client_name, &write_from_offset_to_log_file, thread_log_file_name) == ERROR_CODE) {
			error = 1;
			goto thread_cleanup;

		}

		//Connected with a second player!
		//wait untill both threads have reached this point, using a sync barrier object.
		EnterSynchronizationBarrier(&barrier, 0);


		if (ERROR_CODE == send_message(accept_socket, GAME_STARTED, parameters_array, 1, &write_from_offset_to_log_file, thread_log_file_name)) {
			error = 1;
			goto thread_cleanup;

		}


		//enter game_loop
		if (ERROR_CODE == server_game_loop(accept_socket, &num_of_player, client_name, &write_from_offset_to_log_file, thread_log_file_name)) {

			error = 1;
			goto thread_cleanup;

		}

		//game ended for both clients!

		if (ERROR_CODE == initialize_share_resources_to_zero()) {

			error = 1;
			goto thread_cleanup;

		}
	}

thread_cleanup:


	if (error == 1) {

		printf("Player disconnected. Exiting.\n");
		//write to log file
		WinWriteToFile(thread_log_file_name, "Player disconnected. Exiting.\n" , strlen("Player disconnected. Exiting.\n"), write_from_offset_to_log_file);
		write_from_offset_to_log_file += strlen("Player disconnected. Exiting.\n");

		return ERROR_CODE;
	}
	return SUCCESS_CODE;

}

//get client request, extract username into the argument client_name, send back SERVER_APPROVED.
//if some api api function fails, return ERROR_CODE, otherwise 0. 
int approve_client_request(SOCKET accept_socket, char client_name[MAX_LENGH_OF_CLIENT_NAME], int* write_from_offset_to_log_file, char thread_log_file_name[MAX_LENGTH_OF_THREAD_LOG_FILE_NAME]) {
	char* communication_message = NULL;
	char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS];
	char message_type[MAX_LENGH_OF_MESSAGE_TYPE];

	//first get the CLIENT_REQUEST
	init_parameter_array(parameters_array);

	if (recv_communication_message(accept_socket, &communication_message) == TRNS_FAILED)
	{
		printf("Error occuerd in server receving data, error num : % ld\n", WSAGetLastError());
		return ERROR_CODE;

	}

	if (ERROR_CODE == extract_parameters_from_communication_message(communication_message, parameters_array, message_type)) {

		return ERROR_CODE;
	}

	//get the thread_log_file_name
	strcpy_s(client_name, MAX_LENGH_OF_CLIENT_NAME, parameters_array[0]);
	strcpy_s(thread_log_file_name, MAX_LENGTH_OF_THREAD_LOG_FILE_NAME, "Thread_log_");
	strcat_s(thread_log_file_name, MAX_LENGTH_OF_THREAD_LOG_FILE_NAME, client_name);
	strcat_s(thread_log_file_name, MAX_LENGTH_OF_THREAD_LOG_FILE_NAME, ".txt");

	//print to file
	WinWriteToFile(thread_log_file_name, "received from client-", strlen("received from client-"), *write_from_offset_to_log_file);
	*write_from_offset_to_log_file += strlen("received from client-");
	WinWriteToFile(thread_log_file_name, communication_message, get_size_of_communication_message(communication_message), *write_from_offset_to_log_file);
	*write_from_offset_to_log_file += get_size_of_communication_message(communication_message);

	free_communication_message_and_parameters(communication_message, parameters_array, message_type);

	//send back SERVER_APPROVED

	if (ERROR_CODE == send_message(accept_socket, SERVER_APPROVED, parameters_array,1, write_from_offset_to_log_file, thread_log_file_name)) {
		return ERROR_CODE;

	}

	return 0;

}


//if some api api function fails, return ERROR_CODE, otherwise 0. 
int send_main_menu_to_client_and_try_to_connect_with_another_player(SOCKET accept_socket, int* num_of_player, char client_name[MAX_LENGH_OF_CLIENT_NAME], int* write_from_offset_to_log_file, char thread_log_file_name[MAX_LENGTH_OF_THREAD_LOG_FILE_NAME]) {
	char* communication_message = NULL;
	char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS];
	char message_type[MAX_LENGH_OF_MESSAGE_TYPE];


	//send main menu message to client

if (ERROR_CODE == send_message(accept_socket, SERVER_MAIN_MENU, parameters_array, 1, write_from_offset_to_log_file, thread_log_file_name)) {
	return ERROR_CODE;

}

//wait for user response infinite time.
set_time_out_to_recv_calls(accept_socket, INFINITE);

//recv CLIENT_DISCONNECT or CLIENT_VERSUS

if (ERROR_CODE == recv_and_extract_communication_message(accept_socket, &communication_message, message_type, parameters_array, 1, write_from_offset_to_log_file, thread_log_file_name)) {

	return ERROR_CODE;

}

//reconfigure back to 15000 ms
set_time_out_to_recv_calls(accept_socket, WAIT_FOR_RESPONSE);

//if CLIENT_DISCONNECT
if (strcmp(message_type, CLIENT_DISCONNECT) == 0) {

	closesocket(accept_socket);
	free_communication_message_and_parameters(communication_message, parameters_array, message_type);

	return ERROR_CODE;
}

free_communication_message_and_parameters(communication_message, parameters_array, message_type);

// recv CLIENT_VERSUS!
//client chose to play!
if (check_if_player_connected_first_and_update_num_of_players(num_of_player, client_name) == ERROR_CODE) {

	return ERROR_CODE;
}

//read num_of_players_ready_to_play;
int num_of_players_ready_to_play;
read_write_common_resources_protected(3, 0, -1, NULL, &num_of_players_ready_to_play, NULL, -1);

//if no one else if watiting to play, wait.
if (num_of_players_ready_to_play == 1) {
	//wait for another client to connect
	Sleep(WAIT_FOR_RESPONSE);
}

//if 2 players are ready
return check_if_two_players_are_ready_to_play_protected(accept_socket, num_of_player, client_name, write_from_offset_to_log_file, thread_log_file_name);

}


//This function should be called whenever a thread needs to access the shared database, which is the global struct resources_struct;
// Read or write mutex-protected.
//if an int argument is not needed, -1 should be passed. 
//If char* argument is not needed, NULL should be passed.
//Mmeory for data read should be allocated in caller.
// if increase_or_decrease_by_one==1 , the int parmeter to access will be increased by 1. If 0, will be decrease by 1.
//if some api api function fails, return ERROR_CODE, otherwise 0. 
//read_or_write 0 for read, 1 fore write
int read_write_common_resources_protected(int index_of_parameter_to_access, int read_or_write, int int_data_to_write, char* name_str_to_write, int* int_read,
										char **name_str_read, int increase_or_decrease_by_one) {
	DWORD dwWaitResult;

	dwWaitResult = WaitForSingleObject(
		ghMutex,    // handle to mutex
		INFINITE);  // no time-out interval

	if (dwWaitResult == WAIT_OBJECT_0) {

		//read or write to database

		//if asked to read
		if (read_or_write == 0) {

				if (index_of_parameter_to_access == 0) {
					*int_read = resources_struct.first_arrived;
				}

				//read player1 name
				else if (index_of_parameter_to_access == 1) {
					strcpy_s(*name_str_read, MAX_LENGH_OF_CLIENT_NAME, resources_struct.player_1_name);

				}

				//read player2 name
				else if (index_of_parameter_to_access == 2) {
					strcpy_s(*name_str_read, MAX_LENGH_OF_CLIENT_NAME, resources_struct.player_2_name);

				}

				else if (index_of_parameter_to_access == 3) {
					
					*int_read = resources_struct.num_of_players_ready_to_play;
				}


				else if (index_of_parameter_to_access == 4) {

					*int_read = resources_struct.game_number;
				}

				else if (index_of_parameter_to_access == 5) {

					*int_read = resources_struct.game_has_ended;
				}

				else if (index_of_parameter_to_access == 6) {
					*name_str_read = (char*)malloc(sizeof(char) * (strlen(resources_struct.current_player_move) + 1));
					
					if (*name_str_read == NULL) {
						printf("Memory allocation failed in read_write_common_resources_protected()\n");
						return ERROR_CODE;


					}
					else {
						strcpy_s(*name_str_read, strlen(resources_struct.current_player_move) + 1, resources_struct.current_player_move);
					}
					
				}


				else {

				}

			}

		else {
			//asked to write

			if (index_of_parameter_to_access == 3) {

					if (increase_or_decrease_by_one != -1) {
						if (increase_or_decrease_by_one == 0) {

							resources_struct.num_of_players_ready_to_play++;
						}

						else {

							resources_struct.num_of_players_ready_to_play--;
						}

					}
				}

			else if (index_of_parameter_to_access == 0) {

				resources_struct.first_arrived = int_data_to_write;
			}

			else if (index_of_parameter_to_access == 1) {

				strcpy_s(resources_struct.player_1_name, MAX_LENGH_OF_CLIENT_NAME, name_str_to_write);

			}

			else if (index_of_parameter_to_access == 2) {

				strcpy_s(resources_struct.player_2_name, MAX_LENGH_OF_CLIENT_NAME, name_str_to_write);

			}

			else if (index_of_parameter_to_access == 4) {

				resources_struct.game_number = int_data_to_write;

			}

			else if (index_of_parameter_to_access == 5) {

				resources_struct.game_has_ended = int_data_to_write;

			}

			else if (index_of_parameter_to_access == 6) {

				free(resources_struct.current_player_move);
				resources_struct.current_player_move = (char*)malloc(sizeof(char) * (strlen(name_str_to_write) + 1));
				if(resources_struct.current_player_move == NULL) {
					printf("Memory allocation failed in read_write_common_resources_protected()\n");
					return ERROR_CODE;

					
				}
				else {
					strcpy_s(resources_struct.current_player_move, strlen(name_str_to_write)+1, name_str_to_write);
				}
			
			}

			else
			{

			}

		}
			
	}

		
	else {

		printf("WaitForSingleObject failed in read_write_common_resources_protected()\n");

		return ERROR_CODE;
	}

	// Release ownership of the mutex object
	if (!ReleaseMutex(ghMutex))
	{
			printf("Release mutex failed!");
			return ERROR_CODE;
	}

	return SUCCESS_CODE;

}


//Also writes the name of the client to player_1_name if arrived first, otherwise to player_2_name
//if some api api function fails, return ERROR_CODE, otherwise 0. 
int check_if_player_connected_first_and_update_num_of_players(int* num_of_player, char client_name[MAX_LENGH_OF_CLIENT_NAME]) {
	
	DWORD dwWaitResult;

	dwWaitResult = WaitForSingleObject(
		ghMutex,    // handle to mutex
		INFINITE);  // no time-out interval

	if (dwWaitResult == WAIT_OBJECT_0) {
	
			//read write protected
			if (resources_struct.first_arrived == 0) {
				resources_struct.first_arrived = 1;
				*num_of_player = 1;
				strcpy_s(resources_struct.player_1_name, MAX_LENGH_OF_CLIENT_NAME, client_name);

			}

			else {

				*num_of_player = 2;
				strcpy_s(resources_struct.player_2_name, MAX_LENGH_OF_CLIENT_NAME, client_name);

			}
			
			resources_struct.num_of_players_ready_to_play++;

		}

	else {

		printf("WaitForSingleObject failed in check_if_two_players_are_ready_to_play_protected()\n");

		return ERROR_CODE;
	}


	if (!ReleaseMutex(ghMutex))
	{
		printf("Release mutex failed!");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;

}

//this function checks if two threads are ready to play.
//If no, calls revursively to send_main_menu_to_client_and_try_to_connect_with_another_player()
int check_if_two_players_are_ready_to_play_protected(SOCKET accept_socket, int* num_of_player, char client_name[MAX_LENGH_OF_CLIENT_NAME], int* write_from_offset_to_log_file, char thread_log_file_name[MAX_LENGTH_OF_THREAD_LOG_FILE_NAME]) {

	DWORD dwWaitResult;

	dwWaitResult = WaitForSingleObject(
		mutex_to_sync_threads_when_waiting_for_players,    // handle to mutex
		INFINITE);  // no time-out interval

	if (dwWaitResult == WAIT_OBJECT_0) {

	char* communication_message = NULL;
	char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS];

	int num_of_players_ready_to_play;
	//read num_of_players_ready_to_play;
	read_write_common_resources_protected(3, 0, -1, NULL, &num_of_players_ready_to_play, NULL, -1);

	if (num_of_players_ready_to_play == NUM_OF_WORKER_THREADS) {

		//can start the game, go to game loop
	}
	else {

		if (ERROR_CODE == send_message(accept_socket, SERVER_NO_OPPONENTS, parameters_array, 1, write_from_offset_to_log_file, thread_log_file_name)) {
			//num_of_players_ready_to_play--
			read_write_common_resources_protected(3, 1, -1, NULL, NULL, NULL, 1);

			return ERROR_CODE;

		}

		//decrease num_of_players_ready_to_play by 1
		read_write_common_resources_protected(3, 1, -1, NULL, NULL, NULL, 1);

		//set first_arrived to 0
		int first_arrived_int_data_to_write = 0;
		read_write_common_resources_protected(0, 1, first_arrived_int_data_to_write, NULL, NULL, NULL, -1);


		if (!ReleaseMutex(mutex_to_sync_threads_when_waiting_for_players))
		{
			printf("Release mutex failed!");
			return ERROR_CODE;
		}
		return send_main_menu_to_client_and_try_to_connect_with_another_player(accept_socket, num_of_player, client_name, write_from_offset_to_log_file, thread_log_file_name);
	}

	}

	else {

		printf("WaitForSingleObject failed in check_if_two_players_are_ready_to_play_protected()\n");

		return ERROR_CODE;
	}


	if (!ReleaseMutex(mutex_to_sync_threads_when_waiting_for_players))
	{
		printf("Release mutex failed!");
		return ERROR_CODE;
	}


	return SUCCESS_CODE;

}

//Should be called before starting a new game
int initialize_share_resources_to_zero() {
	
	DWORD dwWaitResult;

	dwWaitResult = WaitForSingleObject(
		ghMutex,    // handle to mutex
		INFINITE);  // no time-out interval

	if (dwWaitResult == WAIT_OBJECT_0) {

		resources_struct.game_has_ended = 0;
		resources_struct.game_number = 0;
		resources_struct.first_arrived = 0;
		resources_struct.num_of_players_ready_to_play = 0;
		resources_struct.current_player_move = NULL;

	}

	else {

		printf("WaitForSingleObject failed in initialize_share_resources_to_zero()\n");

		return ERROR_CODE;
	}


	if (!ReleaseMutex(ghMutex))
	{
		printf("Release mutex failed!\n");
		return ERROR_CODE;
	}
	return SUCCESS_CODE;

}



