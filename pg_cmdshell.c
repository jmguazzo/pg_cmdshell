#include <string.h>
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "run_to_stdout.h"


#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif


char * get_char_from_text_pg_arg(PG_FUNCTION_ARGS, int arg_id){

	text            * content_as_text = PG_GETARG_TEXT_P(arg_id);
	int               content_as_text_length = VARSIZE(content_as_text) - VARHDRSZ;
	char            * content = (char *)palloc(content_as_text_length + 1);

	memcpy(content, content_as_text->vl_dat, content_as_text_length);

	content[content_as_text_length] = '\0';

	return content;
}

PGDLLEXPORT Datum pg_cmdshell(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum pg_winshell(PG_FUNCTION_ARGS);


PG_FUNCTION_INFO_V1(pg_cmdshell);
PG_FUNCTION_INFO_V1(pg_winshell);



typedef struct
{
	char** list_result_line;
} pg_cmdshell_context;



Datum pg_shell(PG_FUNCTION_ARGS, BOOL use_cmd_exe){

	if (!superuser()){
		ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), "Only a superuser can execute this extension."));
	}

	FuncCallContext       * fn_call_context;
	pg_cmdshell_context   * context;

	if (SRF_IS_FIRSTCALL())
	{
		
		MemoryContext old_context;

		char * command_line = get_char_from_text_pg_arg(fcinfo, 0);

		fn_call_context = SRF_FIRSTCALL_INIT();
		old_context = MemoryContextSwitchTo(fn_call_context->multi_call_memory_ctx);

		context = palloc(sizeof(pg_cmdshell_context));

		int line_count = exec_with_redirect_from_stdout(command_line, use_cmd_exe, &context->list_result_line);

		fn_call_context->user_fctx = context;
		fn_call_context->max_calls = line_count;
		fn_call_context->call_cntr = 0;

		MemoryContextSwitchTo(old_context);

		pfree(command_line);

	}

	fn_call_context = SRF_PERCALL_SETUP();
	context = (pg_cmdshell_context*)fn_call_context->user_fctx;


	if (fn_call_context->max_calls == -1)
		SRF_RETURN_DONE(fn_call_context);

	if (fn_call_context->call_cntr < fn_call_context->max_calls)
	{
		int actual_result_length = (int)strlen(context->list_result_line[fn_call_context->call_cntr]);

		text * actual_result = cstring_to_text_with_len(context->list_result_line[fn_call_context->call_cntr], actual_result_length);

		SRF_RETURN_NEXT(fn_call_context, (Datum)actual_result);
	}
	else
	{
		SRF_RETURN_DONE(fn_call_context);
	}
}


//Execute actions after "cmd.exe /c " (See COMSPEC)
Datum pg_cmdshell(PG_FUNCTION_ARGS){
	return pg_shell(fcinfo, true);
}

//Execute actions in the windows shell
Datum pg_winshell(PG_FUNCTION_ARGS){
	return pg_shell(fcinfo, false);
}
