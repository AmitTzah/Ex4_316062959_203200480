


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "socket_send_recv.h"
#include "service_thread.h"
#include "file_IO.h"
#include "server_game_loop.h"


bool containsDigit(int number, int digit);
int check_if_move_has_finished_the_game(char* player_guess, int* game_has_finished);

extern shared_server_resources resources_struct;
extern HANDLE ghMutex;
extern HANDLE mutex_to_sync_threads_when_waiting_for_players;
extern HANDLE event_for_syncing_threads_in_game_loop;


//checks if move has finished the game and writes to shared_server_resources accordingly to let the other thread know.
//if some api api function fails, return ERROR_CODE, otherwise 0. 
int check_if_move_has_finished_the_game(char* player_guess, int* game_has_finished) {

	int game_number_read;
	int next_number;
	int should_guess_boom = 0;


	if (ERROR_CODE == read_write_common_resources_protected(5, 0, -1, NULL, game_has_finished, NULL, -1)) {

		return ERROR_CODE;

	}

	if (read_write_common_resources_protected(4, 0, -1, NULL, &game_number_read, NULL, -1) == ERROR_CODE) {

		return ERROR_CODE;

	}
	next_number = game_number_read + 1;

	//check if next_number has digit BOOM_NUMBER
	if (containsDigit(next_number, BOOM_NUMBER) == true) {
		should_guess_boom = 1;


	}

	//check if next_number is divisble by BOOM_NUMBER
	if ((next_number % BOOM_NUMBER) == 0) {

		should_guess_boom = 1;

	}

	//if entered "boom"
	if (strcmp(player_guess, "boom") == 0) {

		if (should_guess_boom == 0) {
			*game_has_finished = 1;

			if (read_write_common_resources_protected(5, 1, *game_has_finished, NULL, NULL, NULL, -1) == ERROR_CODE) {

				return ERROR_CODE;

			}

			return SUCCESS_CODE;

		}


		else {
			*game_has_finished = 0;
			if (read_write_common_resources_protected(5, 1, *game_has_finished, NULL, NULL, NULL, -1) == ERROR_CODE) {

				return ERROR_CODE;

			}
			return SUCCESS_CODE;

		}


	}

	//entered number
	else {

		if (should_guess_boom == 1) {
			*game_has_finished = 1;
			if (read_write_common_resources_protected(5, 1, *game_has_finished, NULL, NULL, NULL, -1) == ERROR_CODE) {

				return ERROR_CODE;

			}
			return SUCCESS_CODE;

		}

		if (strtol(player_guess, NULL, 10) != next_number) {
			//wrong number
			*game_has_finished = 1;
			if (read_write_common_resources_protected(5, 1, *game_has_finished, NULL, NULL, NULL, -1) == ERROR_CODE) {

				return ERROR_CODE;

			}
			return SUCCESS_CODE;

		}

		*game_has_finished = 0;
		if (read_write_common_resources_protected(5, 1, *game_has_finished, NULL, NULL, NULL, -1) == ERROR_CODE) {

			return ERROR_CODE;

		}
		return SUCCESS_CODE;

	}




}

//based on https://stackoverflow.com/questions/46803064/determine-if-a-number-contains-a-digit-for-class-assignment/46803249
bool containsDigit(int number, int digit)
{
	while (number != 0)
	{
		int curr_digit = number % 10;
		if (curr_digit == digit) return true;
		number /= 10;
	}

	return false;
}

//if some api api function fails, return ERROR_CODE, otherwise 0. 
int server_game_loop(SOCKET accept_socket, int* num_of_player, char client_name[MAX_LENGH_OF_CLIENT_NAME]) {
	char* communication_message = NULL;
	char* parameters_array[MAX_NUM_OF_MESSAGE_PARAMETERS];
	char message_type[MAX_LENGH_OF_MESSAGE_TYPE];
	int my_client_turn;
	char other_client_name[MAX_LENGH_OF_CLIENT_NAME];
	char* p_other_client_name = other_client_name;
	int game_has_finished = 0;

	//get other client's name
	if (*num_of_player == 1) {
		read_write_common_resources_protected(2, 0, -1, NULL, NULL, &p_other_client_name, -1);

	}

	else {

		read_write_common_resources_protected(1, 0, -1, NULL, NULL, &p_other_client_name, -1);

	}

	//check if this thread should make the first move
	if (*num_of_player == 1) {
		my_client_turn = 1;

	}

	else {
		//Other client should make the first move.
		my_client_turn = 0;
	}

	//while game is still on
	while (1) {

		if (my_client_turn == 1) {

			//Send turn switch to client with this client's name.
			parameters_array[0] = client_name;
			if (ERROR_CODE == send_message(accept_socket, TURN_SWITCH, parameters_array)) {
				return ERROR_CODE;

			}

			//Ask client for move
			if (ERROR_CODE == send_message(accept_socket, SERVER_MOVE_REQUEST, parameters_array)) {
				return ERROR_CODE;

			}

			//Recv from client CLIENT_PLAYER_MOVE
			if (ERROR_CODE == recv_and_extract_communication_message(accept_socket, &communication_message, message_type, parameters_array)) {

				return ERROR_CODE;

			}

			//get next number
			int current_game_number;
			read_write_common_resources_protected(4, 0, -1, NULL, &current_game_number, NULL, -1);
			int next_number = current_game_number + 1;

			//update current_player_move in shared resources
			read_write_common_resources_protected(6, 1, -1, parameters_array[0], NULL, NULL, -1);

			//check move
			//update game_has_finished in shared resources accordingly
			if (ERROR_CODE == check_if_move_has_finished_the_game(parameters_array[0], &game_has_finished)) {
				return ERROR_CODE;
			}

			// if game hasn't ended, update he game_number with the next number..
			if (game_has_finished == 0) {


				if (read_write_common_resources_protected(4, 1, next_number, NULL, NULL, NULL, -1) == ERROR_CODE) {

					return ERROR_CODE;

				}
			}


			//Next turn is not mine.
			my_client_turn = 0;

			//finished turn, signal to the other thread that is waiting.
			if (SetEvent(event_for_syncing_threads_in_game_loop) == 0) {
				printf("SetEvent() failed in server\n");

				return ERROR_CODE;
			}


			//send_game view to client according to result
			parameters_array[1] = parameters_array[0];
			parameters_array[0] = client_name;


			if (game_has_finished == 1) {

				parameters_array[2] = "END";
			}
			else {
				parameters_array[2] = "CONT";
			}

			if (ERROR_CODE == send_message(accept_socket, GAME_VIEW, parameters_array)) {
				return ERROR_CODE;

			}

			free(communication_message);
			free(parameters_array[1]);

		}

		else {
			//Other client turn
			//send the other client's name.

			//Send turn switch to client with this client's name.
			parameters_array[0] = other_client_name;
			if (ERROR_CODE == send_message(accept_socket, TURN_SWITCH, parameters_array)) {
				return ERROR_CODE;

			}
			//next turn is mine
			my_client_turn = 1;

			WaitForSingleObject(event_for_syncing_threads_in_game_loop, INFINITE);
			//send game_view to client according to other player's move 

			parameters_array[0] = other_client_name;
			read_write_common_resources_protected(6, 0, -1, NULL, NULL, &(parameters_array[1]), -1);
			read_write_common_resources_protected(5, 0, -1, NULL, &game_has_finished, NULL, -1);
			if (game_has_finished == 1) {

				parameters_array[2] = "END";
			}
			else {
				parameters_array[2] = "CONT";
			}

			if (ERROR_CODE == send_message(accept_socket, GAME_VIEW, parameters_array)) {
				return ERROR_CODE;

			}



		}


	}

	return SUCCESS_CODE;
}
