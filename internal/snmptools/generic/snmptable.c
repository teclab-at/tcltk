/*
 * snmptable.c - walk a table and print it nicely
 *
 * Update: 1999-10-26 <rs-snmp@revelstone.com>
 * Added ability to use MIB to query tables with non-sequential column OIDs
 * Added code to handle sparse tables
 *
 * Update: 1998-07-17 <jhy@gsu.edu>
 * Added text <special options> to usage().
 */
/**********************************************************************
	Copyright 1997 Niels Baggesen

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies.

I DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
I BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
/*
 * XXX: 
 *      - After two queries, it stops giving results
 */ 
#include <net-snmp/net-snmp-config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if TIME_WITH_SYS_TIME
# ifdef WIN32
#  include <sys/timeb.h>
# else
#  include <sys/time.h>
# endif
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <stdio.h>
#if HAVE_WINSOCK_H
#include <winsock.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <net-snmp/net-snmp-includes.h>

#include "snmptools.h"
#include "util.h"

#define BUFLEN 500

struct column {
    int             width;
    oid             subid;
    char           *label;
    char           *fmt;
}              *column = NULL;

static char   **data = NULL;
static char   **indices = NULL;
static int      index_width = sizeof("index ") - 1;
static int      fields;
static int      entries;
static int      allocated;
static int      end_of_table = 1;
static int      headers_only = 0;
static int      no_headers = 0;
static int      max_width = 0;
static int      column_width = 0;
static int      brief = 0;
static int      show_index = 0;
static const char    *left_justify_flag = "";
static char    *field_separator = NULL;
static char    *table_name;
static oid      name[MAX_OID_LEN];
static size_t   name_length;
static oid      root[MAX_OID_LEN];
static size_t   rootlen;
static int      localdebug;
static int      exitval = 0;
static int      use_getbulk = 1;
static int      max_getbulk = 10;

static void            usage(void);
static int             get_field_names(void);
static int             get_table_entries(netsnmp_session * ss);
static int             getbulk_table_entries(netsnmp_session * ss);
static void            print_table(void);

static int
optProc(int argc, char *const *argv, int opt)
{
    switch (opt) {
    case 'h':
        usage();
        break;
    case 'C':
        /*
         * Handle new '-C' command-specific meta-options 
         */
        while (*optarg) {
            switch (*optarg++) {
            case 'w':
		if (optind < argc) {
		    if (argv[optind]) {
			max_width = atoi(argv[optind]);
			if (max_width == 0) {
			    printres("Bad -Cw option: %s\n", 
				    argv[optind]);
			    return(1);
			}
		    }
		} else {
                    printres("Bad -Cw option: no argument given\n");
		    return(1);
		}
		optind++;
                break;
            case 'c':
		if (optind < argc) {
		    if (argv[optind]) {
			column_width = atoi(argv[optind]);
			if (column_width <= 2) {
			    printres("Bad -Cc option: %s\n", 
				    argv[optind]);
			    return(1);
			}
                        /* Reduce by one for space at end of column */
                        column_width -= 1;
		    }
		} else {
            printres("Bad -Cc option: no argument given\n");
		    return(1);
		}
		optind++;
                break;
            case 'l':
                left_justify_flag = "-";
                break;
            case 'f':
		if (optind < argc) {
		    field_separator = argv[optind];
		} else {
		    printres("Bad -Cf option: no argument given\n");
		    return(1);
		}
		optind++;
                break;
            case 'h':
                headers_only = 1;
                break;
            case 'H':
                no_headers = 1;
                break;
            case 'B':
                use_getbulk = 0;
                break;
            case 'b':
                brief = 1;
                break;
            case 'i':
                show_index = 1;
                break;
            case 'r':
		if (optind < argc) {
		    if (argv[optind]) {
			max_getbulk = atoi(argv[optind]);
			if (max_getbulk == 0) {
			    printres("Bad -Cr option: %s\n", 
				    argv[optind]);
			    return(1);
			}
		    }
		} else {
            printres("Bad -Cr option: no argument given\n");
		    return(1);
		}
		optind++;
                break;
            default:
                printres("Bad option after -C: %c\n", optarg[-1]);
            }
        }
        break;
    }
    return(0);
}

void
usage(void)
{
    printres("USAGE: table ");
    printres(" TABLE-OID\n\n");
	printres("  -h\t\tThis help message\n");
	printres("  -C APPOPTS\t\tSet various application specific behaviours:\n");
    printres("\t\t\t  b:       brief field names\n");
    printres("\t\t\t  B:       do not use GETBULK requests\n");
    printres("\t\t\t  c<NUM>:  print table in columns of <NUM> chars width\n");
    printres("\t\t\t  f<STR>:  print table delimitied with <STR>\n");
    printres("\t\t\t  h:       print only the column headers\n");
    printres("\t\t\t  H:       print no column headers\n");
    printres("\t\t\t  i:       print index values\n");
    printres("\t\t\t  l:       left justify output\n");
    printres("\t\t\t  r<NUM>:  for GETBULK: set max-repeaters to <NUM>\n");
    printres("\t\t\t           for GETNEXT: retrieve <NUM> entries at a time\n");
    printres("\t\t\t  w<NUM>:  print table in parts of <NUM> chars width\n");
}

void
reverse_fields(void)
{
    struct column   tmp;
    int             i;

    for (i = 0; i < fields / 2; i++) {
        memcpy(&tmp, &(column[i]), sizeof(struct column));
        memcpy(&(column[i]), &(column[fields - 1 - i]),
               sizeof(struct column));
        memcpy(&(column[fields - 1 - i]), &tmp, sizeof(struct column));
    }
}

int
snmptable(int argc, char *argv[], netsnmp_session *session, netsnmp_session *ss)
{
    int             arg;
    int             ret;
    int             total_entries = 0;

    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                           NETSNMP_DS_LIB_QUICK_PRINT, 1);

    /*
     * get the common command line arguments 
     */
    arg = snmptools_snmp_parse_args(argc, argv, session, "hC:", optProc);
    if (arg < 0)
        return (arg * -1);

    /*
     * get the initial object and subtree 
     */
    /*
     * specified on the command line 
     */
    if (optind + 1 != argc) {
        printres("Must have exactly one table name\n");
        return(1);
    }

    rootlen = MAX_OID_LEN;
    if (!snmp_parse_oid(argv[optind], root, &rootlen)) {
        snmptools_snmp_perror(argv[optind]);
        return(1);
    }
    localdebug = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                                        NETSNMP_DS_LIB_DUMP_PACKET);

    ret = get_field_names();
    if(ret > 0) return(ret);
    reverse_fields();

#ifndef NETSNMP_DISABLE_SNMPV1
    if (ss->version == SNMP_VERSION_1)
        use_getbulk = 0;
#endif

    do {
        entries = 0;
        allocated = 0;
        if (!headers_only) {
            if (use_getbulk) {
                ret = getbulk_table_entries(ss);
                if(ret > 0) return(ret);
            } else {
                ret = get_table_entries(ss);
                if(ret > 0) return(ret);
            }
        }

        if (exitval) {
            snmp_close(ss);
            SOCK_CLEANUP;
            return exitval;
        }

        if (entries || headers_only)
            print_table();

        if (data) {
            free (data);
            data = NULL;
        }

        if (indices) {
            free (indices);
            indices = NULL;
        }

        total_entries += entries;

    } while (!end_of_table);

    if (total_entries == 0 && !headers_only)
        printres("%s: No entries\n", table_name);

    return 0;
}

void
print_table(void)
{
    int             entry, field, first_field, last_field = 0, width, part = 0;
    char          **dp;
    char            string_buf[SPRINT_MAX_LEN];
    char           *index_fmt = NULL;
    static int      first_pass = 1;

    if (!no_headers && !headers_only && first_pass)
        printres("{%s}\n\n", table_name);

    for (field = 0; field < fields; field++) {
        if (column_width != 0)
            sprintf(string_buf, "{%%%s%d.%ds}", left_justify_flag,
                    column_width + 1, column_width );
        else if (field_separator == NULL)
            sprintf(string_buf, "{%%%s%ds}", left_justify_flag,
                    column[field].width + 1);
        else if (field == 0 && !show_index)
            sprintf(string_buf, "{%%s}");
        else
            sprintf(string_buf, "{%s%%s}", field_separator);
        column[field].fmt = strdup(string_buf);
    }
    if (show_index) {
        if (column_width)
            sprintf(string_buf, "{%%s}\n\n");
        else if (field_separator == NULL)
            sprintf(string_buf, "{%%%s%ds}", left_justify_flag, index_width);
        else
            sprintf(string_buf, "{%%s}");
        index_fmt = strdup(string_buf);
    }

    if (!no_headers && first_pass)
        printres("{\n");
    while (last_field != fields) {
        part++;
        if (part != 1 && !no_headers)
            printres("\nSNMP table %s, part %d\n\n", table_name, part);
        first_field = last_field;
        dp = data;
        if (show_index && !no_headers && !column_width) {
            width = index_width;
            printres(index_fmt, "index");
        } else
            width = 0;
        for (field = first_field; field < fields; field++) {
            if (max_width)
            {
                if (column_width) {
                    if (!no_headers && first_pass) {
                        width += column_width + 1;
                        if (field != first_field && width > max_width) {
                            printres("\n");
                            width = column_width + 1;
                        }
                    }
                }
                else {
                    width += column[field].width + 1;
                    if (field != first_field && width > max_width)
                        break;
                }
            }
            if (!no_headers && first_pass)
                printres(column[field].fmt, column[field].label);
        }
        last_field = field;
        if (!no_headers && first_pass)
            printres("\n}\n\n");
        for (entry = 0; entry < entries; entry++) {
            width = 0;
            if (show_index)
            {
                if (!column_width)
                    width = index_width;
                printres(index_fmt, indices[entry]);
            }
            printres("{\n");
            for (field = first_field; field < last_field; field++) {
                if (column_width && max_width) {
                    width += column_width + 1;
                    if (field != first_field && width > max_width) {
                        printres("\n");
                        width = column_width + 1;
                    }
                }
                printres(column[field].fmt, dp[field] ? dp[field] : "?");
            }
            dp += fields;
            printres("\n}\n\n");
        }
    }

    first_pass = 0;
}

int
get_field_names(void)
{
    char           *buf = NULL, *name_p = NULL;
    size_t          buf_len = 0, out_len = 0;
#ifndef NETSNMP_DISABLE_MIB_LOADING
    struct tree    *tbl = NULL;
#endif /* NETSNMP_DISABLE_MIB_LOADING */
    int             going = 1;

#ifndef NETSNMP_DISABLE_MIB_LOADING
    tbl = get_tree(root, rootlen, get_tree_head());
    if (tbl) {
        tbl = tbl->child_list;
        if (tbl) {
            root[rootlen++] = tbl->subid;
            tbl = tbl->child_list;
        } else {
            root[rootlen++] = 1;
            going = 0;
        }
    }
#endif /* NETSNMP_DISABLE_MIB_LOADING */

    if (sprint_realloc_objid
        ((u_char **)&buf, &buf_len, &out_len, 1, root, rootlen - 1)) {
        table_name = buf;
        buf = NULL;
        buf_len = out_len = 0;
    } else {
        table_name = strdup("[TRUNCATED]");
        out_len = 0;
    }

    fields = 0;
    while (going) {
        fields++;
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (tbl) {
            if (tbl->access == MIB_ACCESS_NOACCESS) {
                fields--;
                tbl = tbl->next_peer;
                if (!tbl) {
                    going = 0;
                }
                continue;
            }
            root[rootlen] = tbl->subid;
            tbl = tbl->next_peer;
            if (!tbl)
                going = 0;
        } else {
#endif /* NETSNMP_DISABLE_MIB_LOADING */
            root[rootlen] = fields;
#ifndef NETSNMP_DISABLE_MIB_LOADING
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        out_len = 0;
        if (sprint_realloc_objid
            ((u_char **)&buf, &buf_len, &out_len, 1, root, rootlen + 1)) {
            name_p = strrchr(buf, '.');
            if (name_p == NULL) {
                name_p = strrchr(buf, ':');
            }
            if (name_p == NULL) {
                name_p = buf;
            } else {
                name_p++;
            }
        } else {
            break;
        }
        if (localdebug) {
            printres("%s %c\n", buf, name_p[0]);
        }
        if ('0' <= name_p[0] && name_p[0] <= '9') {
            fields--;
            break;
        }
        if (fields == 1) {
            column = (struct column *) malloc(sizeof(*column));
        } else {
            column =
                (struct column *) realloc(column,
                                          fields * sizeof(*column));
        }
        column[fields - 1].label = strdup(name_p);
        column[fields - 1].width = strlen(name_p);
        column[fields - 1].subid = root[rootlen];
    }
    if (fields == 0) {
        printres("Was that a table? %s\n", table_name);
        return(1);
    }
    if (name_p) {
        *name_p = 0;
        memmove(name, root, rootlen * sizeof(oid));
        name_length = rootlen + 1;
        name_p = strrchr(buf, '.');
        if (name_p == NULL) {
            name_p = strrchr(buf, ':');
        }
        if (name_p != NULL) {
            *name_p = 0;
        }
    }
    if (brief && fields > 1) {
        char           *f1, *f2;
        int             common = strlen(column[0].label);
        int             field, len;
        for (field = 1; field < fields; field++) {
            f1 = column[field - 1].label;
            f2 = column[field].label;
            while (*f1 && *f1++ == *f2++);
            len = f2 - column[field].label - 1;
            if (len < common)
                common = len;
        }
        if (common) {
            for (field = 0; field < fields; field++) {
                column[field].label += common;
                column[field].width -= common;
            }
        }
    }
    if (buf != NULL) {
        free(buf);
    }
    return(0);
}

int
get_table_entries(netsnmp_session * ss)
{
    int             running = 1;
    netsnmp_pdu    *pdu, *response;
    netsnmp_variable_list *vars;
    int             count;
    int             status;
    int             i;
    int             col;
    char           *buf = NULL;
    size_t          out_len = 0, buf_len = 0;
    char           *cp;
    char           *name_p = NULL;
    char          **dp;
    int             have_current_index;

    /*
     * TODO:
     *   1) Deal with multiple index fields
     *   2) Deal with variable length index fields
     *   3) optimize to remove a sparse column from get-requests
     */

    while (running &&
           ((max_width && !column_width) || (entries < max_getbulk))) {
        /*
         * create PDU for GETNEXT request and add object name to request 
         */
        pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);
        for (i = 1; i <= fields; i++) {
            name[rootlen] = column[i - 1].subid;
            snmp_add_null_var(pdu, name, name_length);
        }

        /*
         * do the request 
         */
        status = snmp_synch_response(ss, pdu, &response);
        if (status == STAT_SUCCESS) {
            if (response->errstat == SNMP_ERR_NOERROR) {
                /*
                 * check resulting variables 
                 */
                vars = response->variables;
                entries++;
                if (entries >= allocated) {
                    if (allocated == 0) {
                        allocated = 10;
                        data =
                            (char **) malloc(allocated * fields *
                                             sizeof(char *));
                        memset(data, 0,
                               allocated * fields * sizeof(char *));
                        if (show_index)
                            indices =
                                (char **) malloc(allocated *
                                                 sizeof(char *));
                    } else {
                        allocated += 10;
                        data =
                            (char **) realloc(data,
                                              allocated * fields *
                                              sizeof(char *));
                        memset(data + entries * fields, 0,
                               (allocated -
                                entries) * fields * sizeof(char *));
                        if (show_index)
                            indices =
                                (char **) realloc(indices,
                                                  allocated *
                                                  sizeof(char *));
                    }
                }
                dp = data + (entries - 1) * fields;
                col = -1;
                end_of_table = 1;       /* assume end of table */
                have_current_index = 0;
                name_length = rootlen + 1;
                char buffer[BUFLEN];
                for (vars = response->variables; vars;
                     vars = vars->next_variable) {
                    col++;
                    name[rootlen] = column[col].subid;
                    if ((vars->name_length < name_length) ||
                        ((int) vars->name[rootlen] != column[col].subid) ||
                        memcmp(name, vars->name,
                               name_length * sizeof(oid)) != 0
                        || vars->type == SNMP_ENDOFMIBVIEW) {
                        /*
                         * not part of this subtree 
                         */
                        if (localdebug) {
                            snprint_variable(buffer, BUFLEN, vars->name,
                                            vars->name_length, vars);
                            printres("%s", buffer);
                            printres(" => ignored\n");
                        }
                        continue;
                    }

                    /*
                     * save index off 
                     */
                    if (!have_current_index) {
                        end_of_table = 0;
                        have_current_index = 1;
                        name_length = vars->name_length;
                        memcpy(name, vars->name,
                               name_length * sizeof(oid));
                        out_len = 0;
                        if (!sprint_realloc_objid
                            ((u_char **)&buf, &buf_len, &out_len, 1, vars->name,
                             vars->name_length)) {
                            break;
                        }
                        i = vars->name_length - rootlen + 1;
                        if (localdebug || show_index) {
                            if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                                              NETSNMP_DS_LIB_EXTENDED_INDEX)) {
                                name_p = strchr(buf, '[');
                            } else {
                                switch (netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                                                          NETSNMP_DS_LIB_OID_OUTPUT_FORMAT)) {
                                case NETSNMP_OID_OUTPUT_MODULE:
				case 0:
                                    name_p = strrchr(buf, ':');
                                    break;
                                case NETSNMP_OID_OUTPUT_SUFFIX:
                                    name_p = buf;
                                    break;
                                case NETSNMP_OID_OUTPUT_FULL:
                                case NETSNMP_OID_OUTPUT_NUMERIC:
                                case NETSNMP_OID_OUTPUT_UCD:
                                    name_p = buf + strlen(table_name)+1;
                                    name_p = strchr(name_p, '.')+1;
                                    break;
				default:
				    printres("Unrecognized -O option: %d\n",
					    netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
							      NETSNMP_DS_LIB_OID_OUTPUT_FORMAT));
				    return(1);
                                }
                                name_p = strchr(name_p, '.') + 1;
                            }
                        }
                        if (localdebug) {
                            printres("Name: %s Index: %s\n", buf, name_p);
                        }
                        if (show_index) {
                            indices[entries - 1] = strdup(name_p);
                            i = strlen(name_p);
                            if (i > index_width)
                                index_width = i;
                        }
                    }

                    if (localdebug && buf) {
                        printres("%s => taken\n", buf);
                    }
                    out_len = 0;
                    sprint_realloc_value((u_char **)&buf, &buf_len, &out_len, 1,
                                         vars->name, vars->name_length,
                                         vars);
                    for (cp = buf; *cp; cp++) {
                        if (*cp == '\n') {
                            *cp = ' ';
                        }
                    }
                    dp[col] = buf;
                    i = out_len;
                    buf = NULL;
                    buf_len = 0;
                    if (i > column[col].width) {
                        column[col].width = i;
                    }
                }

                if (end_of_table) {
                    --entries;
                    /*
                     * not part of this subtree 
                     */
                    if (localdebug) {
                        printres("End of table: %s\n",
                               buf ? (char *) buf : "[NIL]");
                    }
                    running = 0;
                    continue;
                }
            } else {
                /*
                 * error in response, print it 
                 */
                running = 0;
                if (response->errstat == SNMP_ERR_NOSUCHNAME) {
                    printres("End of MIB\n");
                    end_of_table = 1;
                } else {
                    printres("Error in packet.\nReason: %s\n",
                            snmp_errstring(response->errstat));
                    if (response->errindex != 0) {
                        printres("Failed object: ");
                        for (count = 1, vars = response->variables;
                             vars && count != response->errindex;
                             vars = vars->next_variable, count++)
                            /*EMPTY*/;
                        if (vars) {
                            printres(snmptools_print_objid(vars->name, vars->name_length));
                        }
                        printres("\n");
                    }
                    exitval = 2;
                }
            }
        } else if (status == STAT_TIMEOUT) {
            printres("Timeout: No Response from %s\n",
                    ss->peername);
            running = 0;
            exitval = 1;
        } else {                /* status == STAT_ERROR */
            snmptools_snmp_sess_perror("table", ss);
            running = 0;
            exitval = 1;
        }
        if (response)
            snmp_free_pdu(response);
    }
    return(0);
}

int
getbulk_table_entries(netsnmp_session * ss)
{
    int             running = 1;
    netsnmp_pdu    *pdu, *response;
    netsnmp_variable_list *vars, *last_var;
    int             count;
    int             status;
    int             i;
    int             row, col;
    char           *buf = NULL;
    size_t          buf_len = 0, out_len = 0;
    char           *cp;
    char           *name_p = NULL;
    char          **dp;

    while (running) {
        /*
         * create PDU for GETBULK request and add object name to request 
         */
        pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
        pdu->non_repeaters = 0;
        pdu->max_repetitions = max_getbulk;
        snmp_add_null_var(pdu, name, name_length);

        /*
         * do the request 
         */
        status = snmp_synch_response(ss, pdu, &response);
        if (status == STAT_SUCCESS) {
            if (response->errstat == SNMP_ERR_NOERROR) {
                /*
                 * check resulting variables 
                 */
                vars = response->variables;
                last_var = NULL;
                while (vars) {
                    out_len = 0;
                    sprint_realloc_objid((u_char **)&buf, &buf_len, &out_len, 1,
                                         vars->name, vars->name_length);
                    if (vars->type == SNMP_ENDOFMIBVIEW ||
                        memcmp(vars->name, name,
                               rootlen * sizeof(oid)) != 0) {
                        if (localdebug) {
                            printres("%s => end of table\n",
                                   buf ? (char *) buf : "[NIL]");
                        }
                        running = 0;
                        break;
                    }
                    if (localdebug) {
                        printres("%s => taken\n",
                               buf ? (char *) buf : "[NIL]");
                    }
                    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                                              NETSNMP_DS_LIB_EXTENDED_INDEX)) {
                        name_p = strchr(buf, '[');
                    } else {
                        switch (netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                                                  NETSNMP_DS_LIB_OID_OUTPUT_FORMAT)) {
                        case NETSNMP_OID_OUTPUT_MODULE:
			case 0:
                            name_p = strrchr(buf, ':');
                            break;
                        case NETSNMP_OID_OUTPUT_SUFFIX:
                            name_p = buf;
                            break;
                        case NETSNMP_OID_OUTPUT_FULL:
                        case NETSNMP_OID_OUTPUT_NUMERIC:
                        case NETSNMP_OID_OUTPUT_UCD:
                            name_p = buf + strlen(table_name)+1;
                            name_p = strchr(name_p, '.')+1;
                            break;
			default:
			    printres("Unrecognized -O option: %d\n",
				    netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
					              NETSNMP_DS_LIB_OID_OUTPUT_FORMAT));
			    return(1);
                        }
                        name_p = strchr(name_p, '.') + 1;
                    }
                    for (row = 0; row < entries; row++)
                        if (strcmp(name_p, indices[row]) == 0)
                            break;
                    if (row == entries) {
                        entries++;
                        if (entries >= allocated) {
                            if (allocated == 0) {
                                allocated = 10;
                                data =
                                    (char **) malloc(allocated * fields *
                                                     sizeof(char *));
                                memset(data, 0,
                                       allocated * fields *
                                       sizeof(char *));
                                indices =
                                    (char **) malloc(allocated *
                                                     sizeof(char *));
                            } else {
                                allocated += 10;
                                data =
                                    (char **) realloc(data,
                                                      allocated * fields *
                                                      sizeof(char *));
                                memset(data + entries * fields, 0,
                                       (allocated -
                                        entries) * fields *
                                       sizeof(char *));
                                indices =
                                    (char **) realloc(indices,
                                                      allocated *
                                                      sizeof(char *));
                            }
                        }
                        indices[row] = strdup(name_p);
                        i = strlen(name_p);
                        if (i > index_width)
                            index_width = i;
                    }
                    dp = data + row * fields;
                    out_len = 0;
                    sprint_realloc_value((u_char **)&buf, &buf_len, &out_len, 1,
                                         vars->name, vars->name_length,
                                         vars);
                    for (cp = buf; *cp; cp++)
                        if (*cp == '\n')
                            *cp = ' ';
                    for (col = 0; col < fields; col++)
                        if (column[col].subid == vars->name[rootlen])
                            break;
                    dp[col] = buf;
                    i = out_len;
                    buf = NULL;
                    buf_len = 0;
                    if (i > column[col].width)
                        column[col].width = i;
                    last_var = vars;
                    vars = vars->next_variable;
                }
                if (last_var) {
                    name_length = last_var->name_length;
                    memcpy(name, last_var->name,
                           name_length * sizeof(oid));
                }
            } else {
                /*
                 * error in response, print it 
                 */
                running = 0;
                if (response->errstat == SNMP_ERR_NOSUCHNAME) {
                    printres("End of MIB\n");
                } else {
                    printres("Error in packet.\nReason: %s\n",
                            snmp_errstring(response->errstat));
                    if (response->errstat == SNMP_ERR_NOSUCHNAME) {
                        printres("The request for this object identifier failed: ");
                        for (count = 1, vars = response->variables;
                             vars && count != response->errindex;
                             vars = vars->next_variable, count++)
                            /*EMPTY*/;
                        if (vars) {
                            printres(snmptools_print_objid(vars->name, vars->name_length));
                        }
                        printres("\n");
                    }
                    exitval = 2;
                }
            }
        } else if (status == STAT_TIMEOUT) {
            printres("Timeout: No Response from %s\n",
                    ss->peername);
            running = 0;
            exitval = 1;
        } else {                /* status == STAT_ERROR */
            snmptools_snmp_sess_perror("table", ss);
            running = 0;
            exitval = 1;
        }
        if (response)
            snmp_free_pdu(response);
    }
    return(0);
}
