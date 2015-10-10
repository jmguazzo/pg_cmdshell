#include <string.h>
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "run_to_stdout.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif


PGDLLEXPORT Datum pg_cmdshell(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum pg_winshell(PG_FUNCTION_ARGS);


PG_FUNCTION_INFO_V1(pg_cmdshell);
PG_FUNCTION_INFO_V1(pg_winshell);


typedef struct
{
	char** list_result_line;
} pg_cmdshell_context;



Datum pg_shell(PG_FUNCTION_ARGS, BOOL use_cmd_exe){
	FuncCallContext * srf;
	pg_cmdshell_context           * ctx;


	//Convert PG param to char string
	text            * text_command_line = PG_GETARG_TEXT_P(0);
	int               text_command_line_length = VARSIZE(text_command_line) - VARHDRSZ;
	char            * command_line = (char *)palloc(text_command_line_length + 1);
	memcpy(command_line, text_command_line->vl_dat, text_command_line_length);
	command_line[text_command_line_length] = '\0';


	if (SRF_IS_FIRSTCALL())
	{
		srf = SRF_FIRSTCALL_INIT();

		srf->user_fctx = MemoryContextAlloc(srf->multi_call_memory_ctx, sizeof(char**));

		ctx = (pg_cmdshell_context *)srf->user_fctx;

		int nbrLignes = RunRedirectStdout(command_line, use_cmd_exe, &ctx->list_result_line);

		srf->max_calls = nbrLignes;
		srf->call_cntr = 0;
	}

	srf = SRF_PERCALL_SETUP();
	ctx = (char**)srf->user_fctx;



	if (srf->max_calls == -1)
		SRF_RETURN_DONE(srf);

	if (srf->call_cntr < srf->max_calls)
	{
		size_t longueur = strlen(ctx->list_result_line[srf->call_cntr]);
		text * result = cstring_to_text_with_len(ctx->list_result_line[srf->call_cntr], longueur);
		SRF_RETURN_NEXT(srf, (Datum)result);
	}
	else
	{
		SRF_RETURN_DONE(srf);
	}
}

Datum pg_cmdshell(PG_FUNCTION_ARGS){
	return pg_shell(fcinfo, true);
}

Datum pg_winshell(PG_FUNCTION_ARGS){
	return pg_shell(fcinfo, false);
}
