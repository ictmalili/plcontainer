/* Postgres Headers */
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "executor/spi.h"
#include "commands/trigger.h"

/* PLContainer Headers */
#include "common/comm_channel.h"
#include "common/messages/messages.h"
#include "message_fns.h"
#include "sqlhandler.h"
#include "containers.h"
#include "plc_typeio.h"
#include "plcontainer.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(plcontainer_call_handler);

static Datum plcontainer_call_hook(PG_FUNCTION_ARGS);
static plcontainer_result plcontainer_get_result(FunctionCallInfo  fcinfo,
                                                 plcProcInfo      *pinfo);
static Datum plcontainer_process_result(FunctionCallInfo    fcinfo,
                                        plcProcInfo        *pinfo);
static void plcontainer_process_exception(error_message);
static void plcontainer_process_sql(sql_msg msg, plcConn* conn);
static void plcontainer_process_log(log_message);

Datum plcontainer_call_handler(PG_FUNCTION_ARGS) {
    Datum datumreturn = (Datum) 0;
    int ret;

    /* TODO: handle trigger requests as well */
    if (CALLED_AS_TRIGGER(fcinfo)) {
        elog(ERROR, "triggers aren't supported");
        return datumreturn;
    }

    /* save caller's context */
    pl_container_caller_context = CurrentMemoryContext;

    /* Create a new memory context and switch to it */
    ret = SPI_connect();
    if (ret != SPI_OK_CONNECT)
        elog(ERROR, "[plcontainer] SPI connect error: %d (%s)", ret,
             SPI_result_code_string(ret));

    datumreturn = plcontainer_call_hook(fcinfo);

    /* Return to old memory context */
    ret = SPI_finish();
    if (ret != SPI_OK_FINISH)
        elog(ERROR, "[plcontainer] SPI finish error: %d (%s)", ret,
             SPI_result_code_string(ret));

    return datumreturn;
}

static Datum plcontainer_call_hook(PG_FUNCTION_ARGS) {
    Datum                     result = (Datum) 0;
    plcProcInfo              *pinfo;
    bool                      bFirstTimeCall = true;
    FuncCallContext *volatile funcctx = NULL;
    MemoryContext             oldcontext = NULL;

    /* By default we return NULL */
    fcinfo->isnull = true;

    /* Get procedure info from cache or compose it based on catalog */
    pinfo = get_proc_info(fcinfo);

    /* If we have a set-retuning function */
    if (fcinfo->flinfo->fn_retset) {
        /* First Call setup */
        if (SRF_IS_FIRSTCALL()) {
            funcctx = SRF_FIRSTCALL_INIT();
        } else {
            bFirstTimeCall = false;
        }

        /* Every call setup */
        funcctx = SRF_PERCALL_SETUP();
        Assert(funcctx != NULL);

        /* SRF initializes special context shared between function calls */
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        /* If we processed all the rows we can return immediately */
        if (!bFirstTimeCall && pinfo->resrow >= pinfo->result->rows) {
            free_result(pinfo->result);
            SRF_RETURN_DONE(funcctx);
        }
    }

    /* First time call for SRF or just a call of scalar function */
    if (bFirstTimeCall) {
        pinfo->resrow = 0;
        pinfo->result = plcontainer_get_result(fcinfo, pinfo);
    }

    /* Process the result message from client */
    result = plcontainer_process_result(fcinfo, pinfo);
    pinfo->resrow += 1;

    if (fcinfo->flinfo->fn_retset) {
        MemoryContextSwitchTo(oldcontext);
        SRF_RETURN_NEXT(funcctx, result);
    } else {
        free_result(pinfo->result);
    }

    return result;
}

static plcontainer_result plcontainer_get_result(FunctionCallInfo  fcinfo,
                                                 plcProcInfo      *pinfo) {
    char        *name;
    callreq      req;
    plcConn     *conn;
    int          message_type;
    int          shared = 0;
    plcontainer_result result = NULL;

    req = plcontainer_create_call(fcinfo, pinfo);
    name = parse_container_meta(req->proc.src, &shared);
    conn = find_container(name);
    if (conn == NULL) {
        conn = start_container(name, shared);
    }
    pfree(name);

    if (conn != NULL) {
        plcontainer_channel_send(conn, (message)req);

        while (1) {
            int     res = 0;
            message answer;

            res = plcontainer_channel_receive(conn, &answer);
            if (res < 0) {
                elog(ERROR, "Error receiving data from the client, %d", res);
                break;
            }

            message_type = answer->msgtype;
            switch (message_type) {
                case MT_RESULT:
                    result = (plcontainer_result)answer;
                    break;
                case MT_EXCEPTION:
                    plcontainer_process_exception((error_message)answer);
                    break;
                case MT_SQL:
                    plcontainer_process_sql((sql_msg)answer, conn);
                    break;
                case MT_LOG:
                    plcontainer_process_log((log_message)answer);
                    break;
                default:
                    elog(ERROR, "Received unhandled message with type id %d "
                    "from client", message_type);
                    break;
            }

            if (message_type != MT_SQL && message_type != MT_LOG)
                break;
        }
    }
    return result;
}

/*
 * Processing client results message
 */
static Datum plcontainer_process_result(FunctionCallInfo    fcinfo,
                                        plcProcInfo        *pinfo) {
    Datum              result = (Datum) 0;
    plcontainer_result resmsg = pinfo->result;

    if (resmsg->cols > 1) {
        elog(ERROR, "Functions returning multiple columns are not supported yet");
        return result;
    }

    if (pinfo->resrow >= resmsg->rows) {
        elog(ERROR, "Trying to access result row %d of the %d-rows result set",
                    pinfo->resrow, resmsg->rows);
        return result;
    }

    if (resmsg->data[pinfo->resrow][0].isnull == 0) {
        fcinfo->isnull = false;
        result = pinfo->rettype.infunc(resmsg->data[pinfo->resrow][0].value, &pinfo->rettype);
    }

    return result;
}

/*
 * Processing client log message
 */
static void plcontainer_process_log(log_message log) {
    elog(log->level, "%s", log->message);
}

/*
 * Processing client SQL query message
 */
static void plcontainer_process_sql(sql_msg msg, plcConn* conn) {
    message res;
    res = handle_sql_message(msg);
    if (res != NULL)
        plcontainer_channel_send(conn, (message)res);
}

/*
 * Processing client exception message
 */
static void plcontainer_process_exception(error_message msg) {
    elog(ERROR, "exception occurred: \n %s \n %s", msg->message, msg->stacktrace);
}