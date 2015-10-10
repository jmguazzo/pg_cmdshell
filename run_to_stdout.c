#include <string.h>
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"


void RemoveUnwantedChar(char * file_buffer){
	for (int i = 0; i < 256; i++){
		if (file_buffer[i] == '\n'){
			memmove(file_buffer + i, file_buffer + i + 1, 256 - i - 1);
		}
		else if (file_buffer[i] > 127 || file_buffer[i] < 0){
			file_buffer[i] = 32;
		}
	}
}

//Prepare the command line with information from COMSPEC environment variable ( = "Where is cmd.exe")
//or just the initial_command_line if use_cmd_exe = false
void PrepareCommandLine(char * command_to_run, char * initial_command_line, BOOL use_cmd_exe)
{
	if (use_cmd_exe) {
		strcpy(command_to_run, getenv("comspec"));
		strcat(command_to_run, " /c ");
	}
	strcat(command_to_run, initial_command_line);
}
//based on work from Hector Santos
int RunRedirectStdout(char *command_line, BOOL use_cmd_exe, char ** results)
{
	//Initialize security attributes
	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&sa, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	//Initialize output files
	char temp_folder[MAX_PATH];
	char temp_filename[MAX_PATH];
	GetTempPath(sizeof(temp_folder), temp_folder);
	GetTempFileName(temp_folder, "tmp", 0, temp_filename);

	// setup redirection handles
	// output handle must be WRITE mode, share READ
	// redirect handle must be READ mode, share WRITE
	HANDLE file_output = INVALID_HANDLE_VALUE;
	file_output = CreateFile(temp_filename, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, 0);

	if (file_output == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	HANDLE hRedir = INVALID_HANDLE_VALUE;
	hRedir = CreateFile(temp_filename, GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

	if (hRedir == INVALID_HANDLE_VALUE) {
		CloseHandle(file_output);
		return FALSE;
	}

	// setup startup info, set std out/err handles
	// hide window

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	if (file_output != INVALID_HANDLE_VALUE) {
		si.dwFlags |= STARTF_USESTDHANDLES;
		si.hStdOutput = file_output;
		si.hStdError = file_output;
		si.wShowWindow = SW_HIDE;
	}

	// use the current OS comspec for DOS commands, i.e., DIR
	char cmd[MAX_PATH] = { 0 };

	PrepareCommandLine(&cmd, command_line, use_cmd_exe);


	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	DWORD dwFlags = CREATE_NO_WINDOW;  // NT/2000 only

	if (!CreateProcess(NULL, (char *)cmd, NULL, NULL, TRUE, dwFlags, NULL, NULL, &si, &pi)) {

		int err = GetLastError();  // preserve the CreateProcess error

		if (file_output != INVALID_HANDLE_VALUE) {
			CloseHandle(file_output);
			CloseHandle(hRedir);
		}

		SetLastError(err);
		return FALSE;
	}

	CloseHandle(pi.hThread);

	// wait for process to finish
	do {
		Sleep(1);
	} while (WaitForSingleObject(pi.hProcess, 0) != WAIT_OBJECT_0);


	char** temp_result = NULL;
	temp_result = palloc(1 * sizeof(*temp_result));

	int result_count = 0;

	char empty_buffer[256] = { 0 };
	char file_buffer[256] = { 0 };
	char line_buffer[256] = { 0 };
	int line_buffer_char_count = 0;

	DWORD read_bytes_count;

	while (ReadFile(hRedir, &file_buffer, sizeof(file_buffer) - 1, &read_bytes_count, NULL)) {

		if (read_bytes_count == 0) break;

		RemoveUnwantedChar(&file_buffer);

		int file_buffer_length = strlen(file_buffer);

		for (int i = 0; i < file_buffer_length; i++){

			if (file_buffer[i] != '\r'){
				//while interesting char are found, they are added to the line buffer
				line_buffer[line_buffer_char_count] = file_buffer[i];
				line_buffer_char_count++;
			}
			else{
				//new line. Add current line_buffer to the result array
				result_count++;
				line_buffer_char_count++;
				temp_result = repalloc(temp_result, result_count * sizeof(*temp_result));
				temp_result[result_count - 1] = palloc(line_buffer_char_count * sizeof(char));
				memcpy(temp_result[result_count - 1], line_buffer, line_buffer_char_count);

				//reset line_buffer
				line_buffer_char_count = 0;
				memcpy(line_buffer, empty_buffer, 256);
			}
		}
		//reset file buffer
		memcpy(file_buffer, empty_buffer, 256);
	}

	//if they're still some char left, add them as a new line
	if (line_buffer_char_count != 0){
		result_count++;
		temp_result = repalloc(temp_result, result_count * sizeof(*temp_result));
		temp_result[result_count - 1] = palloc(line_buffer_char_count * sizeof(char));
		memcpy(temp_result[result_count - 1], line_buffer, line_buffer_char_count - 1);
		line_buffer_char_count = 0;
	}

	//close file and stuff
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(file_output);
	CloseHandle(hRedir);
	DeleteFile(temp_filename);

	*results = temp_result;
	return result_count;
}
