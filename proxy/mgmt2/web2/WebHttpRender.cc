/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

/****************************************************************************
 *
 *  WebHttpRender.cc - html rendering/assembly
 *
 * 
 ****************************************************************************/

#include "ink_platform.h"

#include "ink_hash_table.h"
#include "ink_snprintf.h"
#include "I_Version.h"
#include "SimpleTokenizer.h"

#include "WebGlobals.h"
#include "WebCompatibility.h"
#include "WebHttp.h"
#include "WebHttpRender.h"
#include "WebHttpSession.h"
#include "WebHttpTree.h"
#include "WebOverview.h"

#include "WebMgmtUtils.h"
#include "MgmtUtils.h"

#include "LocalManager.h"

#include "INKMgmtAPI.h"
#include "CfgContextUtils.h"
#include "MgmtSocket.h"
#include "WebConfigRender.h"

#include "MultiFile.h"
#include "../tools/ConfigAPI.h"
#if defined(OEM)
#include "CfgContextManager.h"
#include "CoreAPI.h"
#endif

//-------------------------------------------------------------------------
// defines
//-------------------------------------------------------------------------

#define MAX_TMP_BUF_LEN       1024
#define MAX_ARGS              10
#define NO_RECORD             "loading..."

//-------------------------------------------------------------------------
// types
//-------------------------------------------------------------------------

typedef int (*WebHttpDisplayHandler) (WebHttpContext * whc, char *tag, char *arg);

//-------------------------------------------------------------------------
// globals
//-------------------------------------------------------------------------

static InkHashTable *g_display_bindings_ht = 0;
extern InkHashTable *g_display_config_ht;

//-------------------------------------------------------------------------
// forward declarations
//-------------------------------------------------------------------------


//-------------------------------------------------------------------------
// substitute_language
//-------------------------------------------------------------------------

int
substitute_language(WebHttpContext * whc, char *tag)
{
  return HtmlRndrText(whc->response_bdy, whc->lang_dict_ht, (HtmlId) tag);
}

//-------------------------------------------------------------------------
// WebHttpGetTopLevelRndrFile_Xmalloc
//-------------------------------------------------------------------------
char *
WebHttpGetTopLevelRndrFile_Xmalloc(WebHttpContext * whc)
{
  char *file = NULL;

  if (whc->top_level_render_file) {
    file = xstrdup(whc->top_level_render_file);
  } else if (whc->request->getFile()) {
    file = xstrdup(whc->request->getFile());
  }
  return file;
}

//-------------------------------------------------------------------------
// WebHttpGetIntFromQuery
//-------------------------------------------------------------------------
void
WebHttpGetIntFromQuery(WebHttpContext * whc, char *tag, int *active_id)
{
  char *active_str;

  if (whc->query_data_ht && ink_hash_table_lookup(whc->query_data_ht, tag, (void **) &active_str)) {
    *active_id = atoi(active_str);
  } else {
    *active_id = 0;
  }
}

//-------------------------------------------------------------------------
// handle_alarm_object
//-------------------------------------------------------------------------

static int
handle_alarm_object(WebHttpContext * whc, char *tag, char *arg)
{
  overviewGenerator->generateAlarmsTable(whc);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_alarm_summary_object
//-------------------------------------------------------------------------

static int
handle_alarm_summary_object(WebHttpContext * whc, char *tag, char *arg)
{
  overviewGenerator->generateAlarmsSummary(whc);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_config_table_object
//-------------------------------------------------------------------------
// Displays rules of config file in table format. The arg specifies
// the "/configure/f_xx_config.ink" of the config file; this arg is used
// to determine which file table to render. Each of the 
// writeXXConfigTable function writes the html for displaying all the rules 
// by using an INKCfgContext to read all the rules and converting 
// each rule into a row of the table.
static int
handle_config_table_object(WebHttpContext * whc, char *tag, char *arg)
{
  INKFileNameT type;
  int err = WEB_HTTP_ERR_OKAY;

  // arg == f_xxx_config.ink (aka. HTML_XXX_CONFIG_FILE)
  if (ink_hash_table_lookup(g_display_config_ht, arg, (void **) &type)) {
    switch (type) {
    case INK_FNAME_CACHE_OBJ:
      err = writeCacheConfigTable(whc);
      break;
    case INK_FNAME_FILTER:
      err = writeFilterConfigTable(whc);
      break;
    case INK_FNAME_FTP_REMAP:
      err = writeFtpRemapConfigTable(whc);
      break;
    case INK_FNAME_HOSTING:
      err = writeHostingConfigTable(whc);
      break;
    case INK_FNAME_ICP_PEER:
      err = writeIcpConfigTable(whc);
      break;
    case INK_FNAME_IP_ALLOW:
      err = writeIpAllowConfigTable(whc);
      break;
    case INK_FNAME_MGMT_ALLOW:
      err = writeMgmtAllowConfigTable(whc);
      break;
    case INK_FNAME_NNTP_ACCESS:
      err = writeNntpAccessConfigTable(whc);
      break;
    case INK_FNAME_NNTP_SERVERS:
      err = writeNntpServersConfigTable(whc);
      break;
    case INK_FNAME_PARENT_PROXY:
      err = writeParentConfigTable(whc);
      break;
    case INK_FNAME_PARTITION:
      err = writePartitionConfigTable(whc);
      break;
    case INK_FNAME_REMAP:
      err = writeRemapConfigTable(whc);
      break;
    case INK_FNAME_SOCKS:
      err = writeSocksConfigTable(whc);
      break;
    case INK_FNAME_SPLIT_DNS:
      err = writeSplitDnsConfigTable(whc);
      break;
    case INK_FNAME_UPDATE_URL:
      err = writeUpdateConfigTable(whc);
      break;
    case INK_FNAME_VADDRS:
      err = writeVaddrsConfigTable(whc);
      break;
    default:
      break;
    }
  } else {
    mgmt_log(stderr, "[handle_config_table_object] invalid config file configurator %s\n", arg);
    err = WEB_HTTP_ERR_FAIL;
  }

  return err;
}

//-------------------------------------------------------------------------
// handle_help_config_link
//-------------------------------------------------------------------------
static int
handle_help_config_link(WebHttpContext * whc, char *tag, char *arg)
{
  INKFileNameT type;
  char *ink_file = NULL;

  if (ink_hash_table_lookup(whc->query_data_ht, HTML_CONFIG_FILE_TAG, (void **) &ink_file) ||
      ink_hash_table_lookup(whc->post_data_ht, HTML_CONFIG_FILE_TAG, (void **) &ink_file)) {

    // look up the INKFileNameT type
    if (ink_hash_table_lookup(g_display_config_ht, ink_file, (void **) &type)) {
      switch (type) {
      case INK_FNAME_CACHE_OBJ:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_CACHE, strlen(HTML_HELP_LINK_CACHE));
        break;
      case INK_FNAME_FILTER:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_FILTER, strlen(HTML_HELP_LINK_FILTER));
        break;
      case INK_FNAME_FTP_REMAP:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_FTP_REMAP, strlen(HTML_HELP_LINK_FTP_REMAP));
        break;
      case INK_FNAME_HOSTING:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_HOSTING, strlen(HTML_HELP_LINK_HOSTING));
        break;
      case INK_FNAME_ICP_PEER:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_ICP, strlen(HTML_HELP_LINK_ICP));
        break;
      case INK_FNAME_IP_ALLOW:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_IP_ALLOW, strlen(HTML_HELP_LINK_IP_ALLOW));
        break;
      case INK_FNAME_MGMT_ALLOW:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_MGMT_ALLOW, strlen(HTML_HELP_LINK_MGMT_ALLOW));
        break;
      case INK_FNAME_NNTP_ACCESS:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_NNTP_ACCESS, strlen(HTML_HELP_LINK_NNTP_ACCESS));
        break;
      case INK_FNAME_NNTP_SERVERS:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_NNTP_SERVERS, strlen(HTML_HELP_LINK_NNTP_SERVERS));
        break;
      case INK_FNAME_PARENT_PROXY:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_PARENT, strlen(HTML_HELP_LINK_PARENT));
        break;
      case INK_FNAME_PARTITION:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_PARTITION, strlen(HTML_HELP_LINK_PARTITION));
        break;
      case INK_FNAME_REMAP:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_REMAP, strlen(HTML_HELP_LINK_REMAP));
        break;
      case INK_FNAME_SOCKS:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_SOCKS, strlen(HTML_HELP_LINK_SOCKS));
        break;
      case INK_FNAME_SPLIT_DNS:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_SPLIT_DNS, strlen(HTML_HELP_LINK_SPLIT_DNS));
        break;
      case INK_FNAME_UPDATE_URL:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_UPDATE, strlen(HTML_HELP_LINK_UPDATE));
        break;
      case INK_FNAME_VADDRS:
        whc->response_bdy->copyFrom(HTML_HELP_LINK_VADDRS, strlen(HTML_HELP_LINK_VADDRS));
        break;
      default:
        break;
      }
    }
  } else {
    mgmt_log(stderr, "[handle_help_config_link] failed to get top_level_render_file");
  }

  return WEB_HTTP_ERR_OKAY;
}


//-------------------------------------------------------------------------
// handle_dynamic_javascript
//-------------------------------------------------------------------------
// This creates the Javascript Rule object and its properties; 
// Must open the config file and see how many rules; create a javascript Rule 
// object so that it can be inserted into the ruleList object; writes the 
// functions which must interact between the config data form. All
// the config file specific javascript goes here. This javascript is actually
// stored in the "/configure/f_XXX_config.ink" file.    
// 
static int
handle_dynamic_javascript(WebHttpContext * whc, char *tag, char *arg)
{
  int err = WEB_HTTP_ERR_OKAY;
  INKFileNameT type;
  char *ink_file;
  char *ink_file_path = NULL;
  char *file_buf = NULL;
  int file_size;

  // the configurator page can be invoked from two places so it 
  // can retreive the "filename" information from 2 possible places:
  // 1) as a GET request (HTML_SUBMIT_CONFIG_DISPLAY) when click on "Edit file" button 
  // 2) refreshing page after clicking  "Apply" button (HTML_SUBMIT_UPDATE_CONFIG) 
  if (ink_hash_table_lookup(whc->query_data_ht, HTML_CONFIG_FILE_TAG, (void **) &ink_file) ||
      ink_hash_table_lookup(whc->post_data_ht, HTML_CONFIG_FILE_TAG, (void **) &ink_file)) {

    ink_file_path = WebHttpAddDocRoot_Xmalloc(whc, ink_file);
    file_buf = 0;
    if (WebFileImport_Xmalloc(ink_file_path, &file_buf, &file_size) != WEB_HTTP_ERR_OKAY) {
      goto Lnot_found;
    }
    // copy file's contents into html buffer
    whc->response_bdy->copyFrom(file_buf, strlen(file_buf));

    // look up the INKFileNameT type
    if (ink_hash_table_lookup(g_display_config_ht, ink_file, (void **) &type)) {
      switch (type) {
      case INK_FNAME_CACHE_OBJ:
        err = writeCacheRuleList(whc->response_bdy);
        break;
      case INK_FNAME_FILTER:
        err = writeFilterRuleList(whc->response_bdy);
        break;
      case INK_FNAME_FTP_REMAP:
        err = writeFtpRemapRuleList(whc->response_bdy);
        break;
      case INK_FNAME_HOSTING:
        err = writeHostingRuleList(whc->response_bdy);
        break;
      case INK_FNAME_ICP_PEER:
        err = writeIcpRuleList(whc->response_bdy);
        break;
      case INK_FNAME_IP_ALLOW:
        err = writeIpAllowRuleList(whc->response_bdy);
        break;
      case INK_FNAME_MGMT_ALLOW:
        err = writeMgmtAllowRuleList(whc->response_bdy);
        break;
      case INK_FNAME_NNTP_ACCESS:
        err = writeNntpAccessRuleList(whc->response_bdy);
        break;
      case INK_FNAME_NNTP_SERVERS:
        err = writeNntpServersRuleList(whc->response_bdy);
        break;
      case INK_FNAME_PARENT_PROXY:
        err = writeParentRuleList(whc->response_bdy);
        break;
      case INK_FNAME_PARTITION:
        err = writePartitionRuleList(whc->response_bdy);
        break;
      case INK_FNAME_REMAP:
        err = writeRemapRuleList(whc->response_bdy);
        break;
      case INK_FNAME_SOCKS:
        err = writeSocksRuleList(whc->response_bdy);
        break;
      case INK_FNAME_SPLIT_DNS:
        err = writeSplitDnsRuleList(whc->response_bdy);
        break;
      case INK_FNAME_UPDATE_URL:
        err = writeUpdateRuleList(whc->response_bdy);
        break;
      case INK_FNAME_VADDRS:
        err = writeVaddrsRuleList(whc->response_bdy);
        break;
      default:
        break;
      }

      goto Ldone;
    }
  }

Lnot_found:                    // missing file
  if (ink_file_path)
    mgmt_log(stderr, "[handle_dynamic_javascript] requested file not found (%s)", ink_file_path);
  whc->response_hdr->setStatus(STATUS_NOT_FOUND);
  WebHttpSetErrorResponse(whc, STATUS_NOT_FOUND);
  err = WEB_HTTP_ERR_REQUEST_ERROR;
  goto Ldone;

Ldone:
  if (ink_file_path)
    xfree(ink_file_path);
  if (file_buf)
    xfree(file_buf);
  return err;
}


//-------------------------------------------------------------------------
// handle_config_input_form
//-------------------------------------------------------------------------
// Writes the html for the section of the Config File Editor that requires 
// user input/modifications (eg. has the INSERT, MODIFY.... buttons).
// Corresponds to the "writeConfigForm" tag. 
// Each config file has different fields so each form will have different 
// fields on the form (refer to data in corresponding Ele structs). 
static int
handle_config_input_form(WebHttpContext * whc, char *tag, char *arg)
{
  char *ink_file;
  char *frecord;
  INKFileNameT type;
  int err = WEB_HTTP_ERR_OKAY;

  if (ink_hash_table_lookup(whc->query_data_ht, HTML_CONFIG_FILE_TAG, (void **) &ink_file) ||
      ink_hash_table_lookup(whc->post_data_ht, HTML_CONFIG_FILE_TAG, (void **) &ink_file)) {

    if (ink_hash_table_lookup(g_display_config_ht, ink_file, (void **) &type)) {

      // need to have the file's record name on the Config File Editor page
      // so we can check if restart is required when users "Apply" the change
      if (ink_hash_table_lookup(whc->query_data_ht, "frecord", (void **) &frecord)) {
        HtmlRndrInput(whc->response_bdy, HTML_CSS_NONE, HTML_TYPE_HIDDEN, "frecord", frecord, NULL, NULL);
        ink_hash_table_delete(whc->query_data_ht, "frecord");
        xfree(frecord);
      } else if (ink_hash_table_lookup(whc->post_data_ht, "frecord", (void **) &frecord)) {
        HtmlRndrInput(whc->response_bdy, HTML_CSS_NONE, HTML_TYPE_HIDDEN, "frecord", frecord, NULL, NULL);
        ink_hash_table_delete(whc->post_data_ht, "frecord");
        xfree(frecord);
      }

      switch (type) {
      case INK_FNAME_CACHE_OBJ:
        err = writeCacheConfigForm(whc);
        break;
      case INK_FNAME_FILTER:
        err = writeFilterConfigForm(whc);
        break;
      case INK_FNAME_FTP_REMAP:
        err = writeFtpRemapConfigForm(whc);
        break;
      case INK_FNAME_HOSTING:
        err = writeHostingConfigForm(whc);
        break;
      case INK_FNAME_ICP_PEER:
        err = writeIcpConfigForm(whc);
        break;
      case INK_FNAME_IP_ALLOW:
        err = writeIpAllowConfigForm(whc);
        break;
      case INK_FNAME_MGMT_ALLOW:
        err = writeMgmtAllowConfigForm(whc);
        break;
      case INK_FNAME_NNTP_ACCESS:
        err = writeNntpAccessConfigForm(whc);
        break;
      case INK_FNAME_NNTP_SERVERS:
        err = writeNntpServersConfigForm(whc);
        break;
      case INK_FNAME_PARENT_PROXY:
        err = writeParentConfigForm(whc);
        break;
      case INK_FNAME_PARTITION:
        err = writePartitionConfigForm(whc);
        break;
      case INK_FNAME_REMAP:
        err = writeRemapConfigForm(whc);
        break;
      case INK_FNAME_SOCKS:
        err = writeSocksConfigForm(whc);
        break;
      case INK_FNAME_SPLIT_DNS:
        err = writeSplitDnsConfigForm(whc);
        break;
      case INK_FNAME_UPDATE_URL:
        err = writeUpdateConfigForm(whc);
        break;
      case INK_FNAME_VADDRS:
        err = writeVaddrsConfigForm(whc);
        break;
      default:
        break;
      }
    } else {
      mgmt_log(stderr, "[handle_config_input_form] invalid config file configurator %s\n", ink_file);
      err = WEB_HTTP_ERR_FAIL;
    }
  }

  return err;
}



//-------------------------------------------------------------------------
// HtmlRndrSelectList
//-------------------------------------------------------------------------
// Creates a select list where the options are the strings passed in 
// the options array. Assuming the value and text of the option are the same. 
int
HtmlRndrSelectList(textBuffer * html, char *listName, char *options[], int numOpts)
{
  if (!listName || !options)
    return WEB_HTTP_ERR_FAIL;

  HtmlRndrSelectOpen(html, HTML_CSS_BODY_TEXT, listName, 1);

  for (int i = 0; i < numOpts; i++) {
    HtmlRndrOptionOpen(html, options[i], false);
    html->copyFrom(options[i], strlen(options[i]));     // this is option text
    HtmlRndrOptionClose(html);
  }

  HtmlRndrSelectClose(html);

  return WEB_HTTP_ERR_OKAY;
}



//-------------------------------------------------------------------------
// handle_file_edit
//-------------------------------------------------------------------------

static int
handle_file_edit(WebHttpContext * whc, char *tag, char *arg)
{
  Rollback *rb;
  char target_file[FILE_NAME_MAX + 1];
  char *formatText;

  if (arg && varStrFromName(arg, target_file, FILE_NAME_MAX)) {
    if (configFiles->getRollbackObj(target_file, &rb)) {
      textBuffer *output = whc->response_bdy;
      textBuffer *file;
      version_t version;
      char version_str[MAX_VAR_LENGTH + 5];     // length of version + file record
      char checksum[MAX_CHECKSUM_LENGTH + 1];

      rb->acquireLock();
      version = rb->getCurrentVersion();
      if (rb->getVersion_ml(version, &file) != OK_ROLLBACK) {
        file = NULL;
      }
      rb->releaseLock();
      if (file) {
        ink_snprintf(version_str, sizeof(version_str), "%d:%s", version, arg);
        HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_HIDDEN, "file_version", version_str, NULL, NULL);
        fileCheckSum(file->bufPtr(), file->spaceUsed(), checksum, sizeof(checksum));
        HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_HIDDEN, "file_checksum", checksum, NULL, NULL);
        HtmlRndrTextareaOpen(output, HTML_CSS_NONE, 70, 15, HTML_WRAP_OFF, "file_contents", false);
        formatText = substituteForHTMLChars(file->bufPtr());
        output->copyFrom(formatText, strlen(formatText));
        HtmlRndrTextareaClose(output);
        delete file;
        delete[]formatText;
      } else {
        mgmt_log(stderr, "[handle_file_edit] could not acquire/edit file [%s]", target_file);
        goto Lerror;
      }
    } else {
      mgmt_log(stderr, "[handle_file_edit] could not acquire/edit file [%s]", target_file);
      goto Lerror;
    }
  } else {
    mgmt_log(stderr, "[handle_file_edit] file record not found %s", arg);
    goto Lerror;
  }
  return WEB_HTTP_ERR_OKAY;
Lerror:
  whc->response_hdr->setStatus(STATUS_INTERNAL_SERVER_ERROR);
  WebHttpSetErrorResponse(whc, STATUS_INTERNAL_SERVER_ERROR);
  return WEB_HTTP_ERR_REQUEST_ERROR;
}

//-------------------------------------------------------------------------
// handle_include
//-------------------------------------------------------------------------

static int
handle_include(WebHttpContext * whc, char *tag, char *arg)
{
  if (arg) {
    return WebHttpRender(whc, arg);
  } else {
    mgmt_log(stderr, "[handle_include] no argument passed to <@%s ...>", tag);
    whc->response_hdr->setStatus(STATUS_NOT_FOUND);
    WebHttpSetErrorResponse(whc, STATUS_NOT_FOUND);
    return WEB_HTTP_ERR_REQUEST_ERROR;
  }
}

//-------------------------------------------------------------------------
// handle_include_cgi
//-------------------------------------------------------------------------

static int
handle_include_cgi(WebHttpContext * whc, char *tag, char *arg)
{
  int err = WEB_HTTP_ERR_OKAY;
  char *cgi_path;

  if (arg) {
    whc->response_hdr->setCachable(0);
    whc->response_hdr->setStatus(STATUS_OK);
    whc->response_hdr->setContentType(TEXT_HTML);
    cgi_path = WebHttpAddDocRoot_Xmalloc(whc, arg);
    err = spawn_cgi(whc, cgi_path, NULL, false, false);
    xfree(cgi_path);
  } else {
    mgmt_log(stderr, "[handle_include_cgi] no argument passed to <@%s ...>", tag);
  }
  return err;
}

//-------------------------------------------------------------------------
// handle_overview_object
//-------------------------------------------------------------------------

static int
handle_overview_object(WebHttpContext * whc, char *tag, char *arg)
{
  overviewGenerator->generateTable(whc);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_overview_details_object
//-------------------------------------------------------------------------

static int
handle_overview_details_object(WebHttpContext * whc, char *tag, char *arg)
{
  int err;
  if (whc->request_state & WEB_HTTP_STATE_MORE_DETAIL)
    // if currently showing more detail, render link to show less
    err = WebHttpRender(whc, "/monitor/m_overview_details_less.ink");
  else
    err = WebHttpRender(whc, "/monitor/m_overview_details_more.ink");
  return err;
}

//-------------------------------------------------------------------------
// handle_post_data
//-------------------------------------------------------------------------

static int
handle_post_data(WebHttpContext * whc, char *tag, char *arg)
{
  char *value;
  if (arg && whc->post_data_ht) {
    if (ink_hash_table_lookup(whc->post_data_ht, arg, (void **) &value)) {
      if (value) {
        whc->response_bdy->copyFrom(value, strlen(value));
      }
    }
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_query
//-------------------------------------------------------------------------

static int
handle_query(WebHttpContext * whc, char *tag, char *arg)
{
  char *value;
  if (arg && whc->query_data_ht) {
    if (ink_hash_table_lookup(whc->query_data_ht, arg, (void **) &value) && value) {
      whc->response_bdy->copyFrom(value, strlen(value));
    } else {
      mgmt_log(stderr, "[handle_query] invalid argument (%s) passed to <@%s ...>", arg, tag);
    }
  } else {
    mgmt_log(stderr, "[handle_query] no argument passed to <@%s ...>", tag);
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_record
//-------------------------------------------------------------------------

static int
handle_record(WebHttpContext * whc, char *tag, char *arg)
{
  char record_value[MAX_VAL_LENGTH];
  char *record_value_safe;
  char *dummy;

  if (arg != NULL) {
    if (whc->submit_warn_ht && ink_hash_table_lookup(whc->submit_warn_ht, arg, (void **) &dummy)) {
      if (whc->post_data_ht && ink_hash_table_lookup(whc->post_data_ht, arg, (void **) &record_value_safe)) {
        // copy in the value; use double quotes if there is nothing to copy
        if (record_value_safe)
          whc->response_bdy->copyFrom(record_value_safe, strlen(record_value_safe));
        else
          whc->response_bdy->copyFrom("\"\"", 2);
      }
    } else {
      if (!varStrFromName(arg, record_value, MAX_VAL_LENGTH)) {
        ink_strncpy(record_value, NO_RECORD, sizeof(record_value));
      }
      record_value_safe = substituteForHTMLChars(record_value);
      // copy in the value; use double quotes if there is nothing to copy
      if (*record_value_safe != '\0')
        whc->response_bdy->copyFrom(record_value_safe, strlen(record_value_safe));
      else
        whc->response_bdy->copyFrom("\"\"", 2);
      delete[]record_value_safe;
    }
  } else {
    mgmt_log(stderr, "[handle_record] no argument passed to <@%s ...>", tag);
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_record_version
//-------------------------------------------------------------------------

static int
handle_record_version(WebHttpContext * whc, char *tag, char *arg)
{
  int id;
  char id_str[256];
  id = RecGetRecordUpdateCount(RECT_CONFIG);
  if (id < 0) {
    mgmt_log(stderr, "[handle_record_version] unable to CONFIG records update count");
    return WEB_HTTP_ERR_OKAY;
  }
  //fix me --> lmgmt->record_data->pid
  ink_snprintf(id_str, sizeof(id_str), "%ld:%d", lmgmt->record_data->pid, id);
  whc->response_bdy->copyFrom(id_str, strlen(id_str));
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_summary_object
//-------------------------------------------------------------------------

static int
handle_summary_object(WebHttpContext * whc, char *tag, char *arg)
{

  char dateBuf[40];
  char tmpBuf[256];
  time_t uptime_secs, d, h, m, s;

  time_t upTime;

  textBuffer *output = whc->response_bdy;
  MgmtHashTable *dict_ht = whc->lang_dict_ht;

  if (lmgmt->proxy_running == 1) {
    HtmlRndrText(output, dict_ht, HTML_ID_STATUS_ACTIVE);
    HtmlRndrBr(output);
    upTime = lmgmt->proxy_started_at;
    uptime_secs = time(NULL) - upTime;

    d = uptime_secs / (60 * 60 * 24);
    uptime_secs -= d * (60 * 60 * 24);
    h = uptime_secs / (60 * 60);
    uptime_secs -= h * (60 * 60);
    m = uptime_secs / (60);
    uptime_secs -= m * (60);
    s = uptime_secs;

    char *r = ink_ctime_r(&upTime, dateBuf);
    if (r != NULL) {
      HtmlRndrText(output, dict_ht, HTML_ID_UP_SINCE);
      ink_snprintf(tmpBuf, sizeof(tmpBuf), ": %s (%ld:%02ld:%02ld:%02ld)", dateBuf, d, h, m, s);
      output->copyFrom(tmpBuf, strlen(tmpBuf));
      HtmlRndrBr(output);
    }
  } else {
    HtmlRndrText(output, dict_ht, HTML_ID_STATUS_INACTIVE);
    HtmlRndrBr(output);
  }

  HtmlRndrText(output, dict_ht, HTML_ID_CLUSTERING);
  output->copyFrom(": ", 2);
  switch (lmgmt->ccom->cluster_type) {
  case FULL_CLUSTER:
    HtmlRndrText(output, dict_ht, HTML_ID_ENABLED);
    break;
  case MGMT_CLUSTER:
    HtmlRndrText(output, dict_ht, HTML_ID_MANAGEMENT_ONLY);
    break;
  case NO_CLUSTER:
    HtmlRndrText(output, dict_ht, HTML_ID_OFF);
    break;
  case CLUSTER_INVALID:
  default:
    HtmlRndrText(output, dict_ht, HTML_ID_UNKNOWN);
    break;
  }
  HtmlRndrBr(output);

  return WEB_HTTP_ERR_OKAY;

}

//-------------------------------------------------------------------------
// handle_tab_object
//-------------------------------------------------------------------------

static int
handle_tab_object(WebHttpContext * whc, char *tag, char *arg)
{
  int err = WEB_HTTP_ERR_OKAY;
  int active_mode;

  // render main tab object
  WebHttpGetIntFromQuery(whc, "mode", &active_mode);
  if ((err = WebHttpRenderTabs(whc->response_bdy, active_mode)) != WEB_HTTP_ERR_OKAY) {
    mgmt_log(stderr, "[handle_tab_object] failed to render mode tabs");
  }
  return err;
}

//-------------------------------------------------------------------------
// handle_html_tab_object
//-------------------------------------------------------------------------

static int
handle_html_tab_object(WebHttpContext * whc, char *tag, char *arg)
{
  char *file = NULL;
  int err = WEB_HTTP_ERR_OKAY;
  int active_tab;

  // render item tab object
  if ((file = WebHttpGetTopLevelRndrFile_Xmalloc(whc))) {
    WebHttpGetIntFromQuery(whc, "tab", &active_tab);
    if ((err = WebHttpRenderHtmlTabs(whc->response_bdy, file, active_tab) != WEB_HTTP_ERR_OKAY)) {
      mgmt_log(stderr, "[handle_html_tab_object] failed to render link tabs");
    }
  } else {
    mgmt_log(stderr, "[handle_html_tab_object] failed to get top_level_render_file");
  }
  if (file)
    xfree(file);
  return err;
}

//-------------------------------------------------------------------------
// handle_mgmt_auth_object
//-------------------------------------------------------------------------

static int
handle_mgmt_auth_object(WebHttpContext * whc, char *tag, char *arg)
{

  int user_count;
  INKCfgContext ctx;
  INKCfgIterState ctx_state;
  char *ctx_key;
  INKAdminAccessEle *ele;
  textBuffer *output = whc->response_bdy;
  char tmp[32];

  ctx = INKCfgContextCreate(INK_FNAME_ADMIN_ACCESS);

  if (INKCfgContextGet(ctx) != INK_ERR_OKAY)
    printf("ERROR READING FILE\n");
  INKCfgContextGetFirst(ctx, &ctx_state);

  user_count = 0;
  ele = (INKAdminAccessEle *) INKCfgContextGetFirst(ctx, &ctx_state);
  while (ele) {
    // render table row
    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    ink_snprintf(tmp, sizeof(tmp), "user:%d", user_count);
    HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_HIDDEN, tmp, ele->user, NULL, NULL);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, "33%", NULL, 0);
    output->copyFrom(ele->user, strlen(ele->user));
    HtmlRndrTdClose(output);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, "33%", NULL, 0);
    ink_snprintf(tmp, sizeof(tmp), "access:%d", user_count);
    HtmlRndrSelectOpen(output, HTML_CSS_BODY_TEXT, tmp, 1);
    ink_snprintf(tmp, sizeof(tmp), "%d", INK_ACCESS_NONE);
    HtmlRndrOptionOpen(output, tmp, ele->access == INK_ACCESS_NONE);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_AUTH_NO_ACCESS);
    HtmlRndrOptionClose(output);
    ink_snprintf(tmp, sizeof(tmp), "%d", INK_ACCESS_MONITOR);
    HtmlRndrOptionOpen(output, tmp, ele->access == INK_ACCESS_MONITOR);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_AUTH_MONITOR);
    HtmlRndrOptionClose(output);
    ink_snprintf(tmp, sizeof(tmp), "%d", INK_ACCESS_MONITOR_VIEW);
    HtmlRndrOptionOpen(output, tmp, ele->access == INK_ACCESS_MONITOR_VIEW);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_AUTH_MONITOR_VIEW);
    HtmlRndrOptionClose(output);
    ink_snprintf(tmp, sizeof(tmp), "%d", INK_ACCESS_MONITOR_CHANGE);
    HtmlRndrOptionOpen(output, tmp, ele->access == INK_ACCESS_MONITOR_CHANGE);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_AUTH_MONITOR_CHANGE);
    HtmlRndrOptionClose(output);
    HtmlRndrSelectClose(output);
    HtmlRndrTdClose(output);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, "33%", NULL, 0);
    output->copyFrom(ele->password, strlen(ele->password));
    HtmlRndrTdClose(output);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, NULL, NULL, 0);
    ink_snprintf(tmp, sizeof(tmp), "delete:%d", user_count);
    HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_CHECKBOX, tmp, ele->user, NULL, NULL);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
    // move on
    ele = (INKAdminAccessEle *) INKCfgContextGetNext(ctx, &ctx_state);
    user_count++;
  }
  // what? no users?
  if (user_count == 0) {
    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 4);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_NO_ADDITIONAL_USERS);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
  }
  // store context
  ctx_key = WebHttpMakeSessionKey_Xmalloc();
  WebHttpSessionStore(ctx_key, (void *) ctx, InkMgmtApiCtxDeleter);
  // add hidden form tags
  ink_snprintf(tmp, sizeof(tmp), "%d", user_count);
  HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_HIDDEN, "user_count", tmp, NULL, NULL);
  HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_HIDDEN, "session_id", ctx_key, NULL, NULL);
  xfree(ctx_key);

  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_tree_object
//-------------------------------------------------------------------------

static int
handle_tree_object(WebHttpContext * whc, char *tag, char *arg)
{
  int err = WEB_HTTP_ERR_OKAY;
  char *file = NULL;

  if ((err = WebHttpRender(whc, HTML_TREE_HEADER_FILE)) != WEB_HTTP_ERR_OKAY)
    goto Ldone;

  if ((file = WebHttpGetTopLevelRndrFile_Xmalloc(whc))) {
    if ((err = WebHttpRenderJsTree(whc->response_bdy, file)) != WEB_HTTP_ERR_OKAY)
      goto Ldone;

  } else {
    mgmt_log(stderr, "[handle_tree_object] failed to get top_level_render_file");
  }
  err = WebHttpRender(whc, HTML_TREE_FOOTER_FILE);

Ldone:
  if (file)
    xfree(file);
  return err;
}

//-------------------------------------------------------------------------
// handle_vip_object
//-------------------------------------------------------------------------

static int
handle_vip_object(WebHttpContext * whc, char *tag, char *arg)
{

  // Hash table iteration variables
  InkHashTableEntry *entry;
  InkHashTableIteratorState iterator_state;
  char *tmp;
  char *tmpCopy;

  // Local binding variables
  char localHostName[256];
  int hostNameLen;

  // Variables for peer bindings
  ExpandingArray peerBindings(100, true);
  Tokenizer spaceTok(" ");
  char *peerHostName;
  bool hostnameFound;
  int numPeerBindings = 0;

  textBuffer *output = whc->response_bdy;

  // different behavior is vip is enabled or not
  if (lmgmt->virt_map->enabled > 0) {

    // Get the local hostname
    varStrFromName("proxy.node.hostname", localHostName, 256);
    hostNameLen = strlen(localHostName);

    ink_mutex_acquire(&(lmgmt->ccom->mutex));

    // First dump the local VIP map
    for (entry = ink_hash_table_iterator_first(lmgmt->virt_map->our_map, &iterator_state);
         entry != NULL; entry = ink_hash_table_iterator_next(lmgmt->virt_map->our_map, &iterator_state)) {

      tmp = (char *) ink_hash_table_entry_key(lmgmt->virt_map->our_map, entry);

      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_CENTER);
      HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
      output->copyFrom(localHostName, hostNameLen);
      HtmlRndrTdClose(output);
      HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
      output->copyFrom(tmp, strlen(tmp));
      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);

    }

    // Now, dump the peer map and make a copy of it
    for (entry = ink_hash_table_iterator_first(lmgmt->virt_map->ext_map, &iterator_state);
         entry != NULL; entry = ink_hash_table_iterator_next(lmgmt->virt_map->ext_map, &iterator_state)) {
      tmp = (char *) ink_hash_table_entry_key(lmgmt->virt_map->ext_map, entry);
      tmpCopy = xstrdup(tmp);
      peerBindings.addEntry(tmpCopy);
      numPeerBindings++;
    }
    ink_mutex_release(&(lmgmt->ccom->mutex));

    // Output the peer map
    for (int i = 0; i < numPeerBindings; i++) {
      tmp = (char *) peerBindings[i];
      if (spaceTok.Initialize(tmp, SHARE_TOKS) == 2) {

        // Resolve the peer hostname
        // FIXME: is this thread safe?? this whole function used to be
        // called under the overviewGenerator lock
        peerHostName = overviewGenerator->resolvePeerHostname(spaceTok[1]);

        if (peerHostName == NULL) {
          hostnameFound = false;
          peerHostName = (char *) spaceTok[1];
        } else {
          hostnameFound = true;
          // Chop off the domain name
          tmp = strchr(peerHostName, '.');
          if (tmp != NULL) {
            *tmp = '\0';
          }
        }

        HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_CENTER);
        HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
        output->copyFrom(peerHostName, strlen(peerHostName));
        HtmlRndrTdClose(output);
        HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
        output->copyFrom(spaceTok[0], strlen(spaceTok[0]));
        HtmlRndrTdClose(output);
        HtmlRndrTrClose(output);

        if (hostnameFound == true) {
          xfree(peerHostName);
        }
      }
    }

  } else {

    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 2);
    HtmlRndrSpace(output, 2);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_VIP_DISABLED);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);

  }

  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_checked
//-------------------------------------------------------------------------

static int
handle_checked(WebHttpContext * whc, char *tag, char *arg)
{
  const char checkStr[] = "checked";
  char record_value[MAX_VAL_LENGTH];

  Tokenizer backslashTok("\\");
  if (backslashTok.Initialize(arg, SHARE_TOKS) == 2) {
    if (varStrFromName(backslashTok[0], record_value, MAX_VAL_LENGTH - 1)) {
      if (strncmp(backslashTok[1], record_value, strlen(backslashTok[1])) == 0) {
        // replace the tag with "checked" string
        whc->response_bdy->copyFrom(checkStr, strlen(checkStr));
      }
    } else {
      mgmt_log(stderr, "[handle_checked] cannot find record %s", backslashTok[0]);
    }
  } else {
    mgmt_log(stderr, "[handle_checked] invalid number of arguments passed to " "<@%s ...>", tag);
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_action_checked
//-------------------------------------------------------------------------

static int
handle_action_checked(WebHttpContext * whc, char *tag, char *arg)
{
  const char checkStr[] = "checked";
  char *action;

  Tokenizer backslashTok("\\");
  if (backslashTok.Initialize(arg, SHARE_TOKS) == 2) {
    if (whc->post_data_ht) {
      if (ink_hash_table_lookup(whc->post_data_ht, "action", (void **) &action)) {
        if (strncmp(backslashTok[1], action, strlen(backslashTok[1])) == 0) {
          // replace the tag with "checked" string
          whc->response_bdy->copyFrom(checkStr, strlen(checkStr));
        }
      }
    } else {
      if (strncmp(backslashTok[1], "view_last", strlen(backslashTok[1])) == 0) {
        // default "checked" option
        whc->response_bdy->copyFrom(checkStr, strlen(checkStr));
      }
    }
  } else {
    mgmt_log(stderr, "[handle_checked] invalid number of arguments passed to " "<@%s ...>", tag);
  }
  return WEB_HTTP_ERR_OKAY;
}

static int
handle_ftp_select(WebHttpContext * whc)
{

  char *ftp_server_name;
  char *ftp_remote_dir;
  char *ftp_login;
  char *ftp_password;
  // Not used here.
  //struct stat snapDirStat;
  ExpandingArray snap_list(25, true);
  textBuffer *output = whc->response_bdy;

  if (whc->post_data_ht != NULL) {
    if (ink_hash_table_lookup(whc->post_data_ht, "FTPServerName", (void **) &ftp_server_name)) {
      if (ftp_server_name == NULL) {
        return -1;
      }
    }

    if (ink_hash_table_lookup(whc->post_data_ht, "FTPUserName", (void **) &ftp_login)) {
      if (ftp_login == NULL) {
        return -1;
      }
    }
    if (ink_hash_table_lookup(whc->post_data_ht, "FTPPassword", (void **) &ftp_password)) {
      if (ftp_server_name == NULL) {
        return -1;
      }
    }
    if (ink_hash_table_lookup(whc->post_data_ht, "FTPRemoteDir", (void **) &ftp_remote_dir)) {
      if (ftp_server_name == NULL) {
        return -1;
      }
    }

    if ((ftp_server_name != NULL) && (ftp_login != NULL) && (ftp_password != NULL) && (ftp_remote_dir != NULL)) {
      char ftpOutput[4096];
      ink_strncpy(ftpOutput, "", sizeof(ftpOutput));
      INKMgmtFtp("list", ftp_server_name, ftp_login, ftp_password, "", ftp_remote_dir, ftpOutput);
      if (!strncmp(ftpOutput, "ERROR:", 6)) {
        HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
        HtmlRndrTdOpen(output, HTML_CSS_RED_LABEL, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, NULL, "2", 2);
        output->copyFrom(ftpOutput, strlen(ftpOutput));
        HtmlRndrTdClose(output);
        HtmlRndrTrClose(output);
        return -1;
      }

      SimpleTokenizer snapDirPathTok(ftpOutput, ' ');
      int numOptions = snapDirPathTok.getNumTokensRemaining();
      char *default_text = "- select a snapshot -";

      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
      HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
      output->copyFrom("Restore Snapshot", strlen("Restore Snapshot"));
      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);

      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
      HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);

      HtmlRndrSelectOpen(output, HTML_CSS_NONE, "ftp_select", 1);
      HtmlRndrOptionOpen(output, 0, true);
      output->copyFrom(default_text, strlen(default_text));
      HtmlRndrOptionClose(output);
      for (int i = 1; i <= numOptions; i++) {
        HtmlRndrOptionOpen(output, 0, false);
        const char *nextOption = snapDirPathTok.getNext();
        output->copyFrom(nextOption, strlen(nextOption));
        HtmlRndrOptionClose(output);
      }
      HtmlRndrSelectClose(output);
      HtmlRndrTdClose(output);

      HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_LEFT, HTML_VALIGN_TOP, NULL, NULL, 0);
      output->copyFrom("<ul> <li>Select the snapshot to restore from FTP server.</ul>",
                       strlen("<ul> <li>Select the snapshot to restore from FTP server.</ul>"));

      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);

#ifdef OEM
      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
      HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);
      output->
        copyFrom
        ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"Restore Network Snapshot\" value=\"Restore NW Snapshot\">",
         strlen
         ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"Restore Network Snapshot\" value=\"Restore NW Snapshot\">"));
      output->copyFrom("&nbsp;Restore Network Snapshot", strlen("&nbsp;Restore Network Snapshot"));

      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);
#endif //OEM

      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
      HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
      InkHashTableEntry *warn_snapname = ink_hash_table_lookup_entry(whc->submit_warn_ht, "FTPSaveName");
      if (warn_snapname) {
        output->copyFrom("<span class=\"redLabel\">!&nbsp;</span>Save Snapshot to FTP Server",
                         strlen("<span class=\"redLabel\">!&nbsp;</span>Save Snapshot to FTP Server"));
      } else {
        output->copyFrom("Save snapshot to FTP server", strlen("Save Snapshot to FTP Server"));
      }
      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);

      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
      HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);
      if (warn_snapname) {
        char *warn_name;
        char str[1024];
        if (ink_hash_table_lookup(whc->post_data_ht, "FTPSaveName", (void **) &warn_name)) {
          if (warn_name != NULL) {
            ink_snprintf(str, sizeof(str), "<input type=\"text\" size=\"22\" name=\"FTPSaveName\" value=\"%s\">",
                         warn_name);
          } else {
            ink_snprintf(str, sizeof(str), "<input type=\"text\" size=\"22\" name=\"FTPSaveName\" value=\"\">");
          }
        }
        output->copyFrom(str, strlen(str));
      } else {
        output->copyFrom("<input type=\"text\" size=\"22\" name=\"FTPSaveName\" value=\"\">",
                         strlen("<input type=\"text\" size=\"22\" name=\"FTPSaveName\" value=\"\">"));
      }
      //output->copyFrom("<input type=\"text\" size=\"22\" name=\"FTPSaveName\" value=\"\">", strlen("<input type=\"text\" size=\"22\" name=\"FTPSaveName\" value=\"\">"));

      HtmlRndrTdClose(output);

      HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_LEFT, HTML_VALIGN_TOP, NULL, NULL, 0);
      output->copyFrom("<ul> <li>Name of the snapshot to save on the FTP server.</ul>",
                       strlen("<ul> <li>Name of the snapshot to save on the FTP server.</ul>"));

      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);

#ifdef OEM
      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
      HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);
      output->
        copyFrom
        ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"NWSnapshot\" value=\"Network Settings Snapshot\">",
         strlen
         ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"NWSnapshot\" value=\"Network Settings Snapshot\">"));
      output->copyFrom("&nbsp;Save Network Snapshot", strlen("&nbsp;Save Network Snapshot"));

      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);
#endif //OEM

    }
  }
  return WEB_HTTP_ERR_OKAY;
}




char *
floppyPathString(const char *s1, const char *s2)
{
  int newLen;
  char *newStr;

  newLen = strlen(s1) + strlen(s2) + 2;
  newStr = new char[newLen];
  ink_assert(newStr != NULL);
  ink_snprintf(newStr, newLen, "%s%s%s", s1, DIR_SEP, s2);
  return newStr;
}


static int
handle_floppy_select(WebHttpContext * whc)
{

  char *snap_name;
  // Not used here.
  //struct stat snapDirStat;
  char *floppy_drive_mount_point;
  ExpandingArray snap_list(25, true);

#ifndef _WIN32

  if (whc->post_data_ht != NULL) {
    if (ink_hash_table_lookup(whc->post_data_ht, "FloppyDrive", (void **) &floppy_drive_mount_point)) {
      if (floppy_drive_mount_point == NULL) {
        printf("floppy_drive_mount_point = NULL\n");
        return -1;
      } else {
        struct dirent *dirEntry;
        DIR *dir;
        char *fileName;
        char *filePath;
        char *records_config_filePath;
        struct stat fileInfo;
        struct stat records_config_fileInfo;
        // Not used here.
        //fileEntry* fileListEntry;
        textBuffer *output = whc->response_bdy;
        char *default_text = "- select a snapshot -";

        dir = opendir(floppy_drive_mount_point);

        if (dir == NULL) {
          mgmt_log(stderr, "[WebHttpRender:: handle_floppy_select] Unable to open %s directory: %s\n",
                   floppy_drive_mount_point, strerror(errno));
          return -1;
        }

        HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
        HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);
        output->
          copyFrom
          ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"Unmount Floppy\" value=\"Unmount Floppy\">",
           strlen
           ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"Unmount Floppy\" value=\"Unmount Floppy\">"));
        output->copyFrom("&nbsp;Unmount Floppy", strlen("&nbsp;Unmount Floppy"));

        HtmlRndrTdClose(output);
        HtmlRndrTrClose(output);

        dirEntry = (struct dirent *) xmalloc(sizeof(struct dirent) + pathconf(".", _PC_NAME_MAX) + 1);

        HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
        HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
        output->copyFrom("Restore Snapshot", strlen("Restore Snapshot"));
        HtmlRndrTdClose(output);
        HtmlRndrTrClose(output);

        HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
        HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);

        HtmlRndrSelectOpen(output, HTML_CSS_NONE, "floppy_select", 1);
        HtmlRndrOptionOpen(output, 0, true);
        output->copyFrom(default_text, strlen(default_text));
        HtmlRndrOptionClose(output);

        struct dirent *result;
        while (ink_readdir_r(dir, dirEntry, &result) == 0) {
          if (!result)
            break;
          fileName = dirEntry->d_name;
          filePath = floppyPathString(floppy_drive_mount_point, fileName);
          records_config_filePath = floppyPathString(filePath, "records.config");
          if (stat(filePath, &fileInfo) < 0) {
            mgmt_log(stderr, "[WebHttpRender::handle_floppy_select] Stat of %s failed %s\n", fileName, strerror(errno));
          } else {
            if (stat(records_config_filePath, &records_config_fileInfo) < 0) {
              continue;
            }
            // Ignore ., .., and any dot files
            if (*fileName != '.' && (strcmp(fileName, ".."))) {
              HtmlRndrOptionOpen(output, 0, false);
              output->copyFrom(fileName, strlen(fileName));
              HtmlRndrOptionClose(output);
            }
          }
          delete[]filePath;
        }

        xfree(dirEntry);
        closedir(dir);

        HtmlRndrSelectClose(output);
        HtmlRndrTdClose(output);

        HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_LEFT, HTML_VALIGN_TOP, NULL, NULL, 0);
        output->copyFrom("<ul> <li>Select the snapshot to restore from the floppy drive.</ul>",
                         strlen("<ul> <li>Select the snapshot to restore from the floppy drive.</ul>"));

        HtmlRndrTdClose(output);
        HtmlRndrTrClose(output);

#if defined(OEM)
        HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
        HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);
        output->
          copyFrom
          ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"Restore Network Snapshot\" value=\"Restore NW Snapshot\">",
           strlen
           ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"Restore Network Snapshot\" value=\"Restore NW Snapshot\">"));
        output->copyFrom("&nbsp;Restore Network Snapshot", strlen("&nbsp;Restore Network Snapshot"));

        HtmlRndrTdClose(output);
        HtmlRndrTrClose(output);
#endif //OEM

        HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
        HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
        output->copyFrom("Save Snapshot to Floppy", strlen("Save Snapshot to Floppy"));
        HtmlRndrTdClose(output);
        HtmlRndrTrClose(output);

        HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
        HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);
        HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_HIDDEN, "FloppyPath", floppy_drive_mount_point, NULL, NULL);

        if (ink_hash_table_lookup(whc->post_data_ht, "FloppySnapName", (void **) &snap_name)) {
          if (snap_name == NULL) {
            output->copyFrom("<input type=\"text\" size=\"22\" name=\"FloppySnapName\" value=\"\">",
                             strlen("<input type=\"text\" size=\"22\" name=\"FloppySnapName\" value=\"\">"));
          } else {
            char input_string[256];
            ink_snprintf(input_string, sizeof(input_string),
                         "<input type=\"text\" size=\"22\" name=\"FloppySnapName\" value=\"%s\">", snap_name);
            output->copyFrom(input_string, strlen(input_string));
          }
        } else {
          output->copyFrom("<input type=\"text\" size=\"22\" name=\"FloppySnapName\" value=\"\">",
                           strlen("<input type=\"text\" size=\"22\" name=\"FloppySnapName\" value=\"\">"));
        }

        HtmlRndrTdClose(output);

        HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_LEFT, HTML_VALIGN_TOP, NULL, NULL, 0);
        output->copyFrom("<ul> <li>Select the snapshot to save on the floppy disk.</ul>",
                         strlen("<ul> <li>Select the snapshot to save on the floppy disk.</ul>"));

        HtmlRndrTdClose(output);
        HtmlRndrTrClose(output);

#if defined(OEM)
        HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
        HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);
        output->
          copyFrom
          ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"NWSnapshot\" value=\"Network Settings Snapshot\">",
           strlen
           ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<input type=\"checkbox\" name=\"NWSnapshot\" value=\"Network Settings Snapshot\">"));
        output->copyFrom("&nbsp;Save Network Snapshot", strlen("&nbsp;Save Network Snapshot"));

        HtmlRndrTdClose(output);
        HtmlRndrTrClose(output);
#endif //OEM

      }
    }

  }
#endif

  return WEB_HTTP_ERR_OKAY;
}




//-------------------------------------------------------------------------
// handle_select
//-------------------------------------------------------------------------
static int
handle_select(WebHttpContext * whc, char *tag, char *arg)
{
  if (strcmp(arg, "snapshot") == 0) {
    configFiles->displaySnapOption(whc->response_bdy);
  } else if (strcmp(arg, "ftp_snapshot") == 0) {
    handle_ftp_select(whc);
  } else if (strcmp(arg, "floppy_drive") == 0) {
    handle_floppy_select(whc);
  }
  return WEB_HTTP_ERR_OKAY;
}


//-------------------------------------------------------------------------
// handle_password_object
//-------------------------------------------------------------------------
static int
handle_password_object(WebHttpContext * whc, char *tag, char *arg)
{

  RecString pwd_file;
  RecGetRecordString_Xmalloc(arg, &pwd_file);

  if (pwd_file) {
    HtmlRndrInput(whc->response_bdy, HTML_CSS_BODY_TEXT, "password", arg, FAKE_PASSWORD, NULL, NULL);
  } else {
    HtmlRndrInput(whc->response_bdy, HTML_CSS_BODY_TEXT, "password", arg, NULL, NULL, NULL);
  }

  return WEB_HTTP_ERR_OKAY;
}

#ifdef OEM
//-------------------------------------------------------------------------
// handle_sessionid
//-------------------------------------------------------------------------
static int
handle_sessionid(WebHttpContext * whc, char *tag, char *arg)
{
  textBuffer *output = whc->response_bdy;
  char *session_key_str = (char *) xmalloc(10);
  long session_key = WebRand();
  // note: ink_snprintf takes the buffer length, not the string
  // length? Add 2 to xmalloc above to be safe.  ^_^
  ink_snprintf(session_key_str, 9, "%x", session_key);
  output->copyFrom(session_key_str, strlen(session_key_str));
  return WEB_HTTP_ERR_OKAY;
}
#endif //OEM

//-------------------------------------------------------------------------
// handle_select_*_logs
//-------------------------------------------------------------------------
static bool
readable(char *file, MgmtInt * size)
{
  WebHandle h_file;
  if ((h_file = WebFileOpenR(file)) == WEB_HANDLE_INVALID) {
    return false;
  }
  *size = WebFileGetSize(h_file);
  WebFileClose(h_file);
  return true;
}

static bool
selected_log(WebHttpContext * whc, char *file)
{
  char *selected_file;
  InkHashTable *ht = whc->post_data_ht;
  if (ht) {
    if (ink_hash_table_lookup(ht, "logfile", (void **) &selected_file)) {
      if (strcmp(selected_file, file) == 0) {
        return true;
      }
    }
  }
  return false;
}

static void
render_option(textBuffer * output, char *value, char *display, bool selected)
{
  HtmlRndrOptionOpen(output, value, selected);
  output->copyFrom(display, strlen(display));
  HtmlRndrOptionClose(output);
}

//-------------------------------------------------------------------------
// handle_select_system_logs
//-------------------------------------------------------------------------
static int
handle_select_system_logs(WebHttpContext * whc, char *tag, char *arg)
{
  char *syslog_path = NULL;
  char *syslog = NULL;
  char tmp[MAX_TMP_BUF_LEN + 1];
  char tmp2[MAX_TMP_BUF_LEN + 1];
  char tmp3[MAX_TMP_BUF_LEN + 1];
  int i;
  bool selected = false;
  textBuffer *output = whc->response_bdy;
  MgmtInt fsize;;

  // define the name of syslog in different OS
#if (HOST_OS == linux) 
  syslog = "messages";
#endif

// define the path of syslog in different OS
#if (HOST_OS == linux)
  syslog_path = "/var/log/";
#endif

  // display all syslog in the select box
  if (syslog_path) {
    // check if 'message' is readable
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, "%s%s", syslog_path, syslog);
    if (readable(tmp, &fsize)) {
      selected = selected_log(whc, tmp);
      bytesFromInt(fsize, tmp3);
      ink_snprintf(tmp2, MAX_TMP_BUF_LEN, "%s  [%s]", syslog, tmp3);
      render_option(output, tmp, tmp2, selected);
    }
    // check if 'message.n' are exist
    for (i = 0; i < 10; i++) {
      ink_snprintf(tmp, MAX_TMP_BUF_LEN, "%s%s.%d", syslog_path, syslog, i);
      if (readable(tmp, &fsize)) {
        selected = selected_log(whc, tmp);
        bytesFromInt(fsize, tmp3);
        ink_snprintf(tmp2, MAX_TMP_BUF_LEN, "%s.%d  [%s]", syslog, i, tmp3);
        render_option(output, tmp, tmp2, selected);
      }
    }
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_select_access_logs
//-------------------------------------------------------------------------
static int
handle_select_access_logs(WebHttpContext * whc, char *tag, char *arg)
{
  char *logdir;
  char *logfile;
  char tmp[MAX_TMP_BUF_LEN + 1];
  char tmp2[MAX_TMP_BUF_LEN + 1];
  char tmp3[MAX_TMP_BUF_LEN + 1];
  RecInt fsize;
  bool selected = false;
  textBuffer *output = whc->response_bdy;

  struct dirent *dent;
  DIR *dirp;
  DIR *dirp2;

  // open all files in the log directory except traffic.out
  ink_assert(RecGetRecordString_Xmalloc("proxy.config.log2.logfile_dir", &logdir)
             == REC_ERR_OKAY);
  ink_assert(RecGetRecordString_Xmalloc("proxy.config.output.logfile", &logfile)
             == REC_ERR_OKAY);

  if ((dirp = opendir(logdir))) {
    while ((dent = readdir(dirp)) != NULL) {
      // exclude traffic.out*
      if (strncmp(logfile, dent->d_name, strlen(logfile)) != 0) {
        ink_snprintf(tmp, MAX_TMP_BUF_LEN, "%s/%s", logdir, dent->d_name);
        if ((dirp2 = opendir(tmp))) {
          // exclude directory
          closedir(dirp2);

        } else if (strncmp(dent->d_name, ".", 1) == 0 &&
                   strncmp(dent->d_name + strlen(dent->d_name) - 5, ".meta", 5) == 0) {
          // exclude .*.meta files

        } else if (strncmp(dent->d_name, "traffic_server.core", 19) == 0) {
          // exclude traffic_server.core*

        } else {
          if (readable(tmp, &fsize)) {
            selected = selected_log(whc, tmp);
            bytesFromInt(fsize, tmp3);
            ink_snprintf(tmp2, MAX_TMP_BUF_LEN, "%s  [%s]", dent->d_name, tmp3);
            render_option(output, tmp, tmp2, selected);
          }
        }
      }
    }
    closedir(dirp);
  }

  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_select_debug_logs
//-------------------------------------------------------------------------
static int
handle_select_debug_logs(WebHttpContext * whc, char *tag, char *arg)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  char tmp2[MAX_TMP_BUF_LEN + 1];
  char tmp3[MAX_TMP_BUF_LEN + 1];
  MgmtInt fsize;
  textBuffer *output = whc->response_bdy;
  bool selected = false;
  char *logdir;
  char *logfile;
  int i;

  struct dirent *dent;
  DIR *dirp;
  char *debug_logs[] = {
    "diags.log",
    "manager.log",
    "lm.log"
  };

  ink_assert(RecGetRecordString_Xmalloc("proxy.config.log2.logfile_dir", &logdir)
             == REC_ERR_OKAY);
  ink_assert(RecGetRecordString_Xmalloc("proxy.config.output.logfile", &logfile)
             == REC_ERR_OKAY);

  // traffic.out*
  if ((dirp = opendir(logdir))) {
    while ((dent = readdir(dirp)) != NULL) {
      if (strncmp(logfile, dent->d_name, strlen(logfile)) == 0) {
        ink_snprintf(tmp, MAX_TMP_BUF_LEN, "%s/%s", logdir, dent->d_name);
        if (readable(tmp, &fsize)) {
          selected = selected_log(whc, tmp);
          bytesFromInt(fsize, tmp3);
          ink_snprintf(tmp2, MAX_TMP_BUF_LEN, "%s  [%s]", dent->d_name, tmp3);
          render_option(output, tmp, tmp2, selected);
        }
      }
    }
    closedir(dirp);
  }
  // others
  for (i = 0; i < 3; i++) {
    if (readable(debug_logs[i], &fsize)) {
      selected = selected_log(whc, debug_logs[i]);
      bytesFromInt(fsize, tmp3);
      ink_snprintf(tmp2, MAX_TMP_BUF_LEN, "%s  [%s]", debug_logs[i], tmp3);
      render_option(output, debug_logs[i], tmp2, selected);
    }
  }

  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_log_action
//-------------------------------------------------------------------------

static int
handle_log_action(WebHttpContext * whc, char *tag, char *arg)
{
  textBuffer *output = whc->response_bdy;
  InkHashTable *ht = whc->post_data_ht;
  char *logfile;
  char *action;
  char *nlines;
  char *substring;
  char *action_arg;
  char *script_path;
  bool truncated;

  if (arg) {
    char *args[MAX_ARGS + 1];
    for (int i = 0; i < MAX_ARGS + 1; i++)
      args[i] = NULL;

    if (!ht)
      goto Ldone;               // render not from submission
    if (!ink_hash_table_lookup(ht, "logfile", (void **) &logfile))
      goto Ldone;
    if (!ink_hash_table_lookup(ht, "action", (void **) &action))
      goto Ldone;
    if (!logfile || !action)
      goto Ldone;
    if (strcmp(logfile, "default") == 0)
      goto Ldone;

    if (strcmp(action, "view_all") == 0) {
      action_arg = NULL;

    } else if (strcmp(action, "view_last") == 0) {
      if (!ink_hash_table_lookup(ht, "nlines", (void **) &nlines))
        goto Ldone;
      if (!nlines)
        goto Ldone;
      action_arg = nlines;

    } else if (strcmp(action, "view_subset") == 0) {
      if (!ink_hash_table_lookup(ht, "substring", (void **) &substring))
        goto Ldone;
      if (!substring)
        goto Ldone;
      action_arg = substring;

    } else {
      Debug("web2", "[handle_log_action] unknown action: %s", action);
      goto Ldone;
    }
    script_path = WebHttpAddDocRoot_Xmalloc(whc, arg);
    args[0] = script_path;
    args[1] = logfile;
    args[2] = action;
    args[3] = action_arg;

    // grey bar
    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 0);
    HtmlRndrSpace(output, 1);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);

    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
    HtmlRndrTableOpen(output, "100%", 0, 0, 1, NULL);
    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_READONLY_TEXT, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, "100%", NULL, 0);
    HtmlRndrTextareaOpen(output, HTML_CSS_BODY_READONLY_TEXT, 75, 15, HTML_WRAP_OFF, NULL, true);
    processSpawn(&args[0], NULL, NULL, whc->response_bdy, false, true, &truncated);
    HtmlRndrTextareaClose(output);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
    if (truncated) {
      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
      HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, NULL, NULL, 0);
      HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_FILE_TRUNCATED);
      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);
    }
    HtmlRndrTableClose(output);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
    xfree(script_path);

  } else {
    Debug("web2", "[handle_log_action] no argument passed.");
  }

Ldone:
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_version
//-------------------------------------------------------------------------

static int
handle_version(WebHttpContext * whc, char *tag, char *arg)
{
  // bug 52754: For Tsunami only, need to hack in Traffic Edge 
  // version "1.5.0" for the web UI; NEED TO REMOVE for post-tsunami release   
  //whc->response_bdy->copyFrom(appVersionInfo.VersionStr, strlen(appVersionInfo.VersionStr));
  whc->response_bdy->copyFrom(PACKAGE_VERSION, strlen(PACKAGE_VERSION));
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_nntp_plugin_status
//-------------------------------------------------------------------------
// If nntp is enabled, checks that the nntp plugin exists; if it does not 
// exist, it will disable nntp. 
static int
handle_nntp_plugin_status(WebHttpContext * whc, char *tag, char *arg)
{
  MgmtInt nntp_enabled;
  if (varIntFromName("proxy.config.nntp.enabled", &nntp_enabled) && nntp_enabled == 1) {
    if (getNntpPluginStatus() != 1) {   // missing NNTP plugin 
      whc->request_state |= WEB_HTTP_STATE_SUBMIT_WARN;
      HtmlRndrText(whc->submit_warn, whc->lang_dict_ht, HTML_ID_NNTP_NO_PLUGIN);
      HtmlRndrBr(whc->submit_warn);
      // disable NNTP
      varSetFromStr("proxy.config.nntp.enabled", "0");
      WebHttpTreeRebuildJsTree();
    }
  }

  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_nntp_config_display
//-------------------------------------------------------------------------
// Checks if the nntp plugin exists; if it does not exist, 
// will display warning message 
static int
handle_nntp_config_display(WebHttpContext * whc, char *tag, char *arg)
{
  if (getNntpPluginStatus() != 1) {
    whc->request_state |= WEB_HTTP_STATE_SUBMIT_WARN;
    HtmlRndrText(whc->submit_warn, whc->lang_dict_ht, HTML_ID_NNTP_NO_PLUGIN);
    HtmlRndrBr(whc->submit_warn);
  }

  return WEB_HTTP_ERR_OKAY;
}


//-------------------------------------------------------------------------
// handle_clear_cluster_stats
//-------------------------------------------------------------------------
static int
handle_clear_cluster_stats(WebHttpContext * whc, char *tag, char *arg)
{
  textBuffer *output = whc->response_bdy;

  RecInt cluster_type = 0;      // current SSL value, enabled/disabled

  if (RecGetRecordInt("proxy.local.cluster.type", &cluster_type) != REC_ERR_OKAY)
    mgmt_log(stderr, "[handle_clear_cluster_stat] Error: Unable to get cluster type config variable\n");

  // only display button for full or mgmt clustering
  if (cluster_type == 1 || cluster_type == 2) {
    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CLEAR_CLUSTER_STAT);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_TOP, NULL, NULL, 0);
    HtmlRndrInput(output, HTML_CSS_CONFIGURE_BUTTON, "submit", "clear_cluster_stats", " Clear ", NULL, NULL);
    HtmlRndrTdClose(output);
    HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_LEFT, HTML_VALIGN_TOP, NULL, NULL, 0);
    HtmlRndrUlOpen(output);
    HtmlRndrLi(output);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CLEAR_CLUSTER_STAT_HELP);
    HtmlRndrUlClose(output);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
  }

  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_submit_error_msg
//-------------------------------------------------------------------------

static int
handle_submit_error_msg(WebHttpContext * whc, char *tag, char *arg)
{
  if (whc->request_state & WEB_HTTP_STATE_SUBMIT_WARN || whc->request_state & WEB_HTTP_STATE_SUBMIT_NOTE) {
    textBuffer *output = whc->response_bdy;
    HtmlRndrTableOpen(output, "100%", 0, 0, 10);
    HtmlRndrTrOpen(output, HTML_CSS_WARNING_COLOR, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, NULL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "30", 0);

    if (whc->request_state & WEB_HTTP_STATE_SUBMIT_WARN) {
      HtmlRndrSpanOpen(output, HTML_CSS_RED_LINKS);
      output->copyFrom(whc->submit_warn->bufPtr(), whc->submit_warn->spaceUsed());
      HtmlRndrSpanClose(output);
    }
    if (whc->request_state & WEB_HTTP_STATE_SUBMIT_NOTE) {
      HtmlRndrSpanOpen(output, HTML_CSS_BLUE_LINKS);
      output->copyFrom(whc->submit_note->bufPtr(), whc->submit_note->spaceUsed());
      HtmlRndrSpanClose(output);
    }
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
    HtmlRndrTableClose(output);
  }

  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_help_link
//-------------------------------------------------------------------------
static int
handle_help_link(WebHttpContext * whc, char *tag, char *arg)
{
  char *file = NULL;
  char *link;

  if ((file = WebHttpGetTopLevelRndrFile_Xmalloc(whc))) {
    link = (char *) WebHttpTreeReturnHelpLink(file);
    if (link != NULL) {
      whc->response_bdy->copyFrom(link, strlen(link));
    } else {
      whc->response_bdy->copyFrom(HTML_DEFAULT_HELP_FILE, strlen(HTML_DEFAULT_HELP_FILE));
    }
  } else {
    mgmt_log(stderr, "[handle_help_link] failed to get top_level_render_file");
  }

  if (file)
    xfree(file);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_submit_error_flg
//-------------------------------------------------------------------------

static int
handle_submit_error_flg(WebHttpContext * whc, char *tag, char *arg)
{
  if (whc->request_state & WEB_HTTP_STATE_SUBMIT_WARN) {
    char *dummy;
    if (arg && ink_hash_table_lookup(whc->submit_warn_ht, arg, (void **) &dummy)) {
      textBuffer *output = whc->response_bdy;
      HtmlRndrSpanOpen(output, HTML_CSS_RED_LABEL);
      HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_SUBMIT_WARN_FLG);
      HtmlRndrSpace(output, 1);
      HtmlRndrSpanClose(output);
    }
  }
  if (whc->request_state & WEB_HTTP_STATE_SUBMIT_NOTE) {
    char *dummy;
    if (arg && ink_hash_table_lookup(whc->submit_note_ht, arg, (void **) &dummy)) {
      textBuffer *output = whc->response_bdy;
      HtmlRndrSpanOpen(output, HTML_CSS_BLUE_LABEL);
      HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_SUBMIT_NOTE_FLG);
      HtmlRndrSpace(output, 1);
      HtmlRndrSpanClose(output);
    }
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_link
//-------------------------------------------------------------------------

static int
handle_link(WebHttpContext * whc, char *tag, char *arg)
{
  char *link;

  if (arg) {
    link = WebHttpGetLink_Xmalloc(arg);
    whc->response_bdy->copyFrom(link, strlen(link));
    xfree(link);
  } else {
    mgmt_log(stderr, "[handle_link] no arg specified");
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_link_file
//-------------------------------------------------------------------------

static int
handle_link_file(WebHttpContext * whc, char *tag, char *arg)
{
  char *file = NULL;

  if ((file = WebHttpGetTopLevelRndrFile_Xmalloc(whc))) {
    whc->response_bdy->copyFrom(file, strlen(file));
  } else {
    mgmt_log(stderr, "[handle_link_file] failed to get top_level_render_file");
  }
  if (file)
    xfree(file);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_link_query
//-------------------------------------------------------------------------

static int
handle_link_query(WebHttpContext * whc, char *tag, char *arg)
{
  char *file = NULL;
  char *query;

  if ((file = WebHttpGetTopLevelRndrFile_Xmalloc(whc))) {
    query = WebHttpGetLinkQuery_Xmalloc(file);
    whc->response_bdy->copyFrom(query, strlen(query));
    xfree(query);
  } else {
    mgmt_log(stderr, "[handle_link_query] failed to get top_level_render_file");
  }
  if (file)
    xfree(file);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_cache_query
//-------------------------------------------------------------------------

static int
handle_cache_query(WebHttpContext * whc, char *tag, char *arg)
{
  textBuffer *output = whc->response_bdy;
  char *cache_op;
  char *url;
  char *cqr = whc->cache_query_result;
  char *cqr_tmp1;
  char *cqr_tmp2;
  char tmp[MAX_TMP_BUF_LEN + 1];
  int alt_count = 0;
  int size = 0;
  InkHashTable *ht;

  ht = whc->query_data_ht;
  if (ht) {
    if (ink_hash_table_lookup(ht, "url_op", (void **) &cache_op) && ink_hash_table_lookup(ht, "url", (void **) &url)) {

      // blue bar
      HtmlRndrTableOpen(output, "100%", 0, 0, 3, NULL);
      HtmlRndrTrOpen(output, HTML_CSS_SECONDARY_COLOR, HTML_ALIGN_NONE);
      HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, "100%", NULL, 0);
      HtmlRndrSpace(output, 1);
      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);
      HtmlRndrTableClose(output);

      if (url && cqr) {
        if ((cqr = strstr(cqr, "<CACHE_INFO status=\"")) && (cqr_tmp1 = strchr(cqr + 20, '"'))) {
          cqr += 20;
          if (strncmp(cqr, "succeeded", cqr_tmp1 - cqr) == 0) {
            // cache hit
            if (strcmp(cache_op, "Lookup") == 0) {

              // found out number of alternates
              if ((cqr = strstr(cqr, "count=\""))) {
                cqr += 7;
                alt_count = ink_atoi(cqr);
              }

              HtmlRndrTableOpen(output, "100%", 0, 0, 10, NULL);
              HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
              HtmlRndrTableOpen(output, "100%", 1, 0, 0, "#CCCCCC");
              HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);

              HtmlRndrTableOpen(output, "100%", 0, 0, 5, NULL);
              // document general information
              HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL_SMALL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
              HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_GENERAL_INFO);
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);
              // document URL
              HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_TOP, NULL, NULL, 0);
              HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_DOCUMENT);
              HtmlRndrTdClose(output);
              HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
              output->copyFrom(url, strlen(url));
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);
              // number of alternates
              HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_TOP, NULL, NULL, 0);
              HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_ALTERNATE_NUM);
              HtmlRndrTdClose(output);
              HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
              ink_snprintf(tmp, MAX_TMP_BUF_LEN, "%d", alt_count);
              output->copyFrom(tmp, strlen(tmp));
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);

              for (int i = 0; i < alt_count; i++) {
                // alternate information
                HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
                HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL_SMALL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
                HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_ALTERNATE);
                ink_snprintf(tmp, MAX_TMP_BUF_LEN, " %d", i);
                output->copyFrom(tmp, strlen(tmp));
                HtmlRndrTdClose(output);
                HtmlRndrTrClose(output);
                // request sent time
                if ((cqr_tmp1 = strstr(cqr, "<REQ_SENT_TIME size=\"")) && (cqr_tmp2 = strstr(cqr, "</REQ_SENT_TIME>"))) {
                  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
                  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_TOP, NULL, NULL, 0);
                  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_REQ_TIME);
                  HtmlRndrTdClose(output);
                  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
                  size = atoi(cqr_tmp1 + 21);
                  output->copyFrom(cqr_tmp2 - size, size);
                  HtmlRndrTdClose(output);
                  HtmlRndrTrClose(output);
                }
                // request header
                if ((cqr_tmp1 = strstr(cqr, "<REQUEST_HDR size=\"")) && (cqr_tmp2 = strstr(cqr, "</REQUEST_HDR>"))) {
                  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
                  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_TOP, NULL, NULL, 0);
                  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_REQ_HEADER);
                  HtmlRndrTdClose(output);
                  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
                  HtmlRndrPreOpen(output, HTML_CSS_BODY_TEXT, NULL);
                  size = atoi(cqr_tmp1 + 19);
                  output->copyFrom(cqr_tmp2 - size, size);
                  HtmlRndrPreClose(output);
                  HtmlRndrTdClose(output);
                  HtmlRndrTrClose(output);
                }
                // response receive time
                if ((cqr_tmp1 = strstr(cqr, "<RES_RECV_TIME size=\"")) && (cqr_tmp2 = strstr(cqr, "</RES_RECV_TIME>"))) {
                  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
                  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_TOP, NULL, NULL, 0);
                  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_RPN_TIME);
                  HtmlRndrTdClose(output);
                  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
                  size = atoi(cqr_tmp1 + 21);
                  output->copyFrom(cqr_tmp2 - size, size);
                  HtmlRndrTdClose(output);
                  HtmlRndrTrClose(output);
                }
                // response header
                if ((cqr_tmp1 = strstr(cqr, "<RESPONSE_HDR size=\"")) && (cqr_tmp2 = strstr(cqr, "</RESPONSE_HDR>"))) {
                  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
                  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_TOP, NULL, NULL, 0);
                  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_RPN_HEADER);
                  HtmlRndrTdClose(output);
                  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
                  HtmlRndrPreOpen(output, HTML_CSS_BODY_TEXT, NULL);
                  size = atoi(cqr_tmp1 + 20);
                  output->copyFrom(cqr_tmp2 - size, size);
                  HtmlRndrPreClose(output);
                  HtmlRndrTdClose(output);
                  HtmlRndrTrClose(output);
                }
              }
              HtmlRndrTableClose(output);
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);
              HtmlRndrTableClose(output);
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);
              HtmlRndrTableClose(output);

              // blue bar with delete button
              HtmlRndrTableOpen(output, "100%", 0, 0, 3, NULL);
              HtmlRndrTrOpen(output, HTML_CSS_SECONDARY_COLOR, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, "100%", NULL, 0);
              HtmlRndrSpace(output, 1);
              HtmlRndrTdClose(output);
              HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
              HtmlRndrInput(output, HTML_CSS_CONFIGURE_BUTTON, "submit", "url_op", "Delete", NULL, NULL);
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);
              HtmlRndrTableClose(output);
            } else if (strcmp(cache_op, "Delete") == 0) {

              HtmlRndrTableOpen(output, "100%", 0, 0, 10, NULL);
              HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
              HtmlRndrTableOpen(output, "100%", 1, 0, 3, "#CCCCCC");

              // table of deleted urls & status
              while ((cqr = strstr(cqr, "<URL name=\"")) && (cqr_tmp1 = strchr(cqr + 11, '"'))) {
                cqr += 11;

                // document url
                HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
                HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
                output->copyFrom(cqr, cqr_tmp1 - cqr);
                HtmlRndrTdClose(output);

                // deletion status
                cqr = cqr_tmp1 + 2;
                HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, NULL, NULL, 0);
                HtmlRndrSpanOpen(output, HTML_CSS_BLACK_ITEM);
                if ((cqr_tmp1 = strstr(cqr, "</URL>")) && (strncmp(cqr, "succeeded", cqr_tmp1 - cqr) == 0)) {
                  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_DELETED);
                } else {
                  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_CACHE_MISSED);
                }
                HtmlRndrSpanClose(output);
                HtmlRndrTdClose(output);
                HtmlRndrTrClose(output);
              }

              HtmlRndrTableClose(output);
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);
              HtmlRndrTableClose(output);

              // blue bar
              HtmlRndrTableOpen(output, "100%", 0, 0, 3, NULL);
              HtmlRndrTrOpen(output, HTML_CSS_SECONDARY_COLOR, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, "100%", NULL, 0);
              HtmlRndrSpace(output, 1);
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);
              HtmlRndrTableClose(output);
            }
          } else {
            // cache miss
            HtmlRndrTableOpen(output, "100%", 0, 0, 10, NULL);
            HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
            HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
            HtmlRndrTableOpen(output, "100%", 1, 0, 3, "#CCCCCC");
            HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);

            // document url
            HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
            ink_snprintf(tmp, MAX_TMP_BUF_LEN, "%s", url);
            output->copyFrom(tmp, strlen(tmp));
            HtmlRndrTdClose(output);
            // cache miss message
            HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, NULL, NULL, 0);
            HtmlRndrSpanOpen(output, HTML_CSS_BLACK_ITEM);
            HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_CACHE_MISSED);
            HtmlRndrSpanClose(output);
            HtmlRndrTdClose(output);

            HtmlRndrTrClose(output);
            HtmlRndrTableClose(output);
            HtmlRndrTdClose(output);
            HtmlRndrTrClose(output);
            HtmlRndrTableClose(output);

            // blue bar
            HtmlRndrTableOpen(output, "100%", 0, 0, 3, NULL);
            HtmlRndrTrOpen(output, HTML_CSS_SECONDARY_COLOR, HTML_ALIGN_NONE);
            HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, "100%", NULL, 0);
            HtmlRndrSpace(output, 1);
            HtmlRndrTdClose(output);
            HtmlRndrTrClose(output);
            HtmlRndrTableClose(output);
          }
        }
      }
    }
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_cache_regex_query
//-------------------------------------------------------------------------

static int
handle_cache_regex_query(WebHttpContext * whc, char *tag, char *arg)
{
  textBuffer *output = whc->response_bdy;
  char *cache_op;
  char *regex;
  char *cqr = whc->cache_query_result;
  char *cqr_tmp1;
  char *cqr_tmp2;
  char tmp[MAX_TMP_BUF_LEN + 1];
  char *url;
  int url_size;
  InkHashTable *ht;

  ht = whc->post_data_ht;
  if (ht) {
    if (ink_hash_table_lookup(ht, "regex_op", (void **) &cache_op) &&
        ink_hash_table_lookup(ht, "regex", (void **) &regex)) {

      // Result label
      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
      HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
      HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_REGEX_MATCHED);
      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);

      if (regex && cqr) {
        if ((cqr = strstr(cqr, "<CACHE_INFO status=\"")) && (cqr_tmp1 = strchr(cqr + 20, '"'))) {
          cqr += 20;
          if (strncmp(cqr, "succeeded", cqr_tmp1 - cqr) == 0) {
            // cache hit
            if (strcmp(cache_op, "Lookup") == 0 ||
                strcmp(cache_op, "Delete") == 0 || strcmp(cache_op, "Invalidate") == 0) {

              HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 2);
              HtmlRndrTableOpen(output, "100%", 0, 0, 10, NULL);
              HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
              HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
              HtmlRndrTableOpen(output, "100%", 1, 0, 3, "#CCCCCC");

              if (strcmp(cache_op, "Lookup") == 0) {
                HtmlRndrFormOpen(output, "regex_form", HTML_METHOD_GET, HTML_SUBMIT_INSPECTOR_DPY_FILE);
              }
              // Table of Documents
              cqr_tmp1 = cqr;
              while ((cqr_tmp1 = strstr(cqr_tmp1, "<URL>")) && (cqr_tmp2 = strstr(cqr_tmp1, "</URL>"))) {
                cqr_tmp1 += 5;
                HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
                HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 2);

                // calculate url length
                url_size = cqr_tmp2 - cqr_tmp1;
                url = xstrndup(cqr_tmp1, url_size);
                if (strcmp(cache_op, "Lookup") == 0) {
                  // display document lookup link
                  ink_snprintf(tmp, MAX_TMP_BUF_LEN, "%s?url_op=%s&url=%s", HTML_SUBMIT_INSPECTOR_DPY_FILE, cache_op,
                               url);
                  HtmlRndrAOpen(output, HTML_CSS_GRAPH, tmp, "display",
                                "window.open('display', 'width=350, height=400')");
                  output->copyFrom(url, url_size);
                  HtmlRndrAClose(output);
                } else {
                  // display document URL only
                  output->copyFrom(url, url_size);
                }
                HtmlRndrTdClose(output);

                if (strcmp(cache_op, "Lookup") == 0) {
                  HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, NULL, NULL, 0);
                  HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_CHECKBOX, url, NULL, NULL, "addToUrlList(this)");
                  HtmlRndrTdClose(output);
                } else if (strcmp(cache_op, "Delete") == 0) {
                  HtmlRndrTdOpen(output, HTML_CSS_BLACK_ITEM, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, NULL, NULL, 0);
                  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_DELETED);
                  HtmlRndrTdClose(output);
                } else if (strcmp(cache_op, "Invalidate") == 0) {
                  HtmlRndrTdOpen(output, HTML_CSS_BLACK_ITEM, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, NULL, NULL, 0);
                  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_INVALIDATED);
                  HtmlRndrTdClose(output);
                }
                xfree(url);
                HtmlRndrTrClose(output);
                cqr_tmp1 = cqr_tmp2 + 6;
              }
              HtmlRndrTableClose(output);
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);

              // delete button for lookup regex
              if (strcmp(cache_op, "Lookup") == 0) {
                HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
                HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_RIGHT, HTML_VALIGN_NONE, NULL, NULL, 2);
                HtmlRndrInput(output, HTML_CSS_CONFIGURE_BUTTON, HTML_TYPE_BUTTON, NULL, "Delete", "display",
                              "setUrls(window.document.regex_form)");
                HtmlRndrTdClose(output);
                HtmlRndrTrClose(output);
                HtmlRndrFormClose(output);
              }

              HtmlRndrTableClose(output);
              HtmlRndrTdClose(output);
              HtmlRndrTrClose(output);
            }
          } else if (strncmp(cqr, "failed", cqr_tmp1 - cqr) == 0) {
            HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
            HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 2);
            HtmlRndrTableOpen(output, "100%", 0, 0, 10, NULL);
            HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
            HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
            HtmlRndrTableOpen(output, "100%", 1, 0, 3, "#CCCCCC");
            HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
            HtmlRndrTdOpen(output, HTML_CSS_BLACK_ITEM, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 2);
            HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_REGEX_MISSED);
            HtmlRndrTdClose(output);
            HtmlRndrTrClose(output);
            HtmlRndrTableClose(output);
            HtmlRndrTdClose(output);
            HtmlRndrTrClose(output);
            HtmlRndrTableClose(output);
            HtmlRndrTdClose(output);
            HtmlRndrTrClose(output);

          } else if (strncmp(cqr, "error", cqr_tmp1 - cqr) == 0) {
            HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
            HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 2);
            HtmlRndrTableOpen(output, "100%", 0, 0, 10, NULL);
            HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
            HtmlRndrTdOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 0);
            HtmlRndrTableOpen(output, "100%", 1, 0, 3, "#CCCCCC");
            HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
            HtmlRndrTdOpen(output, HTML_CSS_BLACK_ITEM, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 2);
            HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_INSPECTOR_REGEX_ERROR);
            HtmlRndrTdClose(output);
            HtmlRndrTrClose(output);
            HtmlRndrTableClose(output);
            HtmlRndrTdClose(output);
            HtmlRndrTrClose(output);
            HtmlRndrTableClose(output);
            HtmlRndrTdClose(output);
            HtmlRndrTrClose(output);
          }
        }
      }
    }
  }
  return WEB_HTTP_ERR_OKAY;
}


//-------------------------------------------------------------------------
// handle_time
//-------------------------------------------------------------------------

static int
handle_time(WebHttpContext * whc, char *tag, char *arg)
{
  time_t my_time_t;
  char my_ctime_str[32];
  time(&my_time_t);
  ink_ctime_r(&my_time_t, my_ctime_str);
  char *p = my_ctime_str;
  while (*p != '\n' && *p != '\0')
    p++;
  if (*p == '\n')
    *p = '\0';
  whc->response_bdy->copyFrom(my_ctime_str, strlen(my_ctime_str));
  return WEB_HTTP_ERR_OKAY;
}

#ifdef OEM
//------------------------------------------------------------------------
// handle_date
//------------------------------------------------------------------------

extern int find_value(char *pathname, char *key, char *value, char *delim, int no);
static int
handle_date(WebHttpContext * whc, char *tag, char *arg)
{
  textBuffer *output = whc->response_bdy;
  char value[256], hour[10], minute[10], second[10], month[10], day[10], year[10];
  char *dummy, *old_value, *value_safe;

//conside different render method for timezone
  if (ink_hash_table_lookup(whc->submit_warn_ht, arg, (void **) &dummy) && strcmp(arg, "timezone_select") != 0) {
    if (ink_hash_table_lookup(whc->post_data_ht, arg, (void **) &old_value) && old_value != NULL) {
      value_safe = substituteForHTMLChars(old_value);
      whc->response_bdy->copyFrom(value_safe, strlen(value_safe));
    }
    goto Ldone;
  }

  ink_strncpy(value, "", sizeof(value));

  if (strcmp(arg, "hour") == 0) {
    Config_GetTime(value, minute, second);
  } else if (strcmp(arg, "minute") == 0) {
    Config_GetTime(hour, value, second);
  } else if (strcmp(arg, "second") == 0) {
    Config_GetTime(hour, minute, value);
  } else if (strcmp(arg, "month") == 0) {
    Config_GetDate(value, day, year);
  } else if (strcmp(arg, "day") == 0) {
    Config_GetDate(month, value, year);
  } else if (strcmp(arg, "year") == 0) {
    Config_GetDate(month, day, value);
  } else if (strcmp(arg, "timezone") == 0) {
    FILE *fp;
    char buffer[1024];
    char old_zone[1024];
    char *zone;

    Config_GetTimezone(old_zone, sizeof(old_zone));
    Config_SortTimezone();

    fp = fopen("/tmp/zonetab", "r");
    fgets(buffer, 1024, fp);
    while (!feof(fp)) {
      zone = buffer;
      if (zone[strlen(zone) - 1] == '\n') {
        zone[strlen(zone) - 1] = '\0';
      }
      if (strcmp(zone, old_zone) == 0) {
        HtmlRndrOptionOpen(output, zone, true);
      } else {
        HtmlRndrOptionOpen(output, zone, false);
      }
      output->copyFrom(zone, strlen(zone));
      HtmlRndrOptionClose(output);
      fgets(buffer, 1024, fp);
    }
    fclose(fp);
    remove("/tmp/zonetab");
  } else if (strstr(arg, "ntp_server") != NULL) {
    if (strstr(arg, "1") != NULL) {
      Config_GetNTP_Server(value, sizeof(value), 0);
    } else if (strstr(arg, "2") != NULL) {
      Config_GetNTP_Server(value, sizeof(value), 1);
    } else if (strstr(arg, "3") != NULL) {
      Config_GetNTP_Server(value, sizeof(value), 2);
    }
  } else if (strcmp(arg, "ntp_enable") == 0) {
    char status[10];
    Config_GetNTP_Status(status, sizeof(status));
    if (strcmp(status, "on") == 0) {
      ink_strncpy(value, "checked", sizeof(value));
    } else {
      ink_strncpy(value, "", sizeof(value));
    }
  } else if (strcmp(arg, "ntp_disable") == 0) {
    char status[10];
    Config_GetNTP_Status(status, sizeof(status));
    if (strcmp(status, "off") == 0) {
      ink_strncpy(value, "checked", sizeof(value));
    } else {
      ink_strncpy(value, "", sizeof(value));
    }
  }

  if (strlen(value) != 0) {
    value_safe = substituteForHTMLChars(value);
    whc->response_bdy->copyFrom(value_safe, strlen(value_safe));
  }

Ldone:
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_ftp_logging
//-------------------------------------------------------------------------
static int
handle_ftp_logging(WebHttpContext * whc, char *tag, char *arg)
{
  int err = WEB_HTTP_ERR_OKAY;
  char buffer[1024], value[1024];
  char *value_safe, *old_value, *dummy;
  int i;
  FILE *f;

  if (ink_hash_table_lookup(whc->submit_warn_ht, arg, (void **) &dummy)) {
    if (ink_hash_table_lookup(whc->post_data_ht, arg, (void **) &old_value) && old_value != NULL) {
      value_safe = substituteForHTMLChars(old_value);
      whc->response_bdy->copyFrom(value_safe, strlen(value_safe));
    }
    goto Ldone;
  }

  if (strcmp(arg, "ftp_logging_enable") == 0) {
    if (access("conf/yts/internal/ftp_logging.config", F_OK) == 0) {
      ink_strncpy(value, "checked", sizeof(value));
    } else {
      ink_strncpy(value, "", sizeof(value));
    }
  } else if (strcmp(arg, "ftp_logging_disable") == 0) {
    if (access("conf/yts/internal/ftp_logging.config", F_OK) != 0) {
      ink_strncpy(value, "checked", sizeof(value));
    } else {
      ink_strncpy(value, "", sizeof(value));
    }
  } else if (strcmp(arg, "FTPServerName") == 0) {
    f = fopen("conf/yts/internal/ftp_logging.config", "r");
    if (f != NULL) {
      fgets(buffer, 1024, f);
      sscanf(buffer, "%s\n", value);
      fclose(f);
    } else {
      ink_strncpy(value, "", sizeof(value));
    }
  } else if (strcmp(arg, "FTPUserName") == 0) {
    f = fopen("conf/yts/internal/ftp_logging.config", "r");
    if (f != NULL) {
      fgets(buffer, 1024, f);
      fgets(buffer, 1024, f);
      sscanf(buffer, "%s\n", value);
      fclose(f);
    } else {
      ink_strncpy(value, "", sizeof(value));
    }
  } else if (strcmp(arg, "FTPRemoteDir") == 0) {
    f = fopen("conf/yts/internal/ftp_logging.config", "r");
    if (f != NULL) {
      fgets(buffer, 1024, f);
      fgets(buffer, 1024, f);
      fgets(buffer, 1024, f);
      fgets(buffer, 1024, f);
      sscanf(buffer, "%s\n", value);
      fclose(f);
    } else {
      ink_strncpy(value, "", sizeof(value));
    }
  }

  if (strlen(value) != 0) {
    value_safe = substituteForHTMLChars(value);
    whc->response_bdy->copyFrom(value_safe, strlen(value_safe));
  }

Ldone:
  return WEB_HTTP_ERR_OKAY;
}

#endif
//-------------------------------------------------------------------------
// handle_user
//-------------------------------------------------------------------------

static int
handle_user(WebHttpContext * whc, char *tag, char *arg)
{
  MgmtInt basic_auth_enabled;
  if (!varIntFromName("proxy.config.admin.basic_auth", &basic_auth_enabled)) {
    return WEB_HTTP_ERR_FAIL;
  }
  if (basic_auth_enabled) {
    char *user = whc->current_user.user;
    HtmlRndrText(whc->response_bdy, whc->lang_dict_ht, HTML_ID_USER);
    HtmlRndrSpace(whc->response_bdy, 1);
    whc->response_bdy->copyFrom(user, strlen(user));
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_plugin_object
//-------------------------------------------------------------------------

static int
handle_plugin_object(WebHttpContext * whc, char *tag, char *arg)
{

  textBuffer *output = whc->response_bdy;
  WebPluginConfig *wpc = lmgmt->plugin_list.getFirst();
  char *config_link;
  int config_link_len;

  if (wpc != NULL) {
    while (wpc) {
      HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_LEFT);
      HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_TOP, NULL, NULL, 0);
      config_link_len = strlen(wpc->config_path) + 16;
      config_link = (char *) alloca(config_link_len + 1);
      ink_snprintf(config_link, config_link_len, "/plugins/%s", wpc->config_path);
      HtmlRndrAOpen(output, HTML_CSS_GRAPH, config_link, "_blank");
      output->copyFrom(wpc->name, strlen(wpc->name));
      HtmlRndrAClose(output);
      HtmlRndrTdClose(output);
      HtmlRndrTrClose(output);
      wpc = lmgmt->plugin_list.getNext(wpc);
    }
  } else {
    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 3);
    HtmlRndrSpace(output, 2);
#ifdef OEM
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_OEM_NO_PLUGINS);
#else
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_NO_PLUGINS);
#endif
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
  }

  return WEB_HTTP_ERR_OKAY;

}

#if defined(OEM)
//-------------------------------------------------------------------------
// int GetPluginEnableStatus ()
// Find if plugin line is commented in the plugin.config file
// return 0 if it is off
// return 1 if it is on
// return -1 if plugin does not exist 
// return -2 if any error happens
//-------------------------------------------------------------------------

static int
GetPlugInEnableStatus(WebHttpContext * whc, Plugin_t which_plugin)
{
  textBuffer *output = whc->response_bdy;
  char *p1;
  Rollback *file_rb;
  textBuffer *file_content = NULL;
  version_t ver;
  int return_code;
  char *plugin_lib = NULL;

  if (!(configFiles->getRollbackObj("plugin.config", &file_rb))) {
    mgmt_log(stderr, "[handleWebsenseFile] ERROR getting rollback object\n");
    return_code = -2;
    goto done;
  }
  ver = file_rb->getCurrentVersion();
  file_rb->getVersion(ver, &file_content);

  switch (which_plugin) {
  case PLUGIN_WEBSENSE:
    plugin_lib = "WebsenseEnterprise/websense.so";
    break;
  case PLUGIN_VSCAN:
    plugin_lib = "vscan.so";
    break;
  default:
    return_code = -2;
    goto done;
  }

  if ((p1 = strstr(file_content->bufPtr(), plugin_lib)) == NULL) {
    return_code = -1;
    goto done;
  }

  do {
    p1--;
  } while (*p1 == ' ');

  if ((char) *p1 == '#') {
    return_code = 0;
    goto done;
  } else {
    return_code = 1;
    goto done;
  }

done:
  if (file_content) {
    delete file_content;
  }
  return return_code;
}

//-------------------------------------------------------------------------
// handle_websense_status
//-------------------------------------------------------------------------

static int
handle_websense_status(WebHttpContext * whc, char *tag, char *arg)
{

  textBuffer *output = whc->response_bdy;
  char websense_status[256];
  int OnOff = GetPlugInEnableStatus(whc, PLUGIN_WEBSENSE);

  if (OnOff == -1)
    goto done;

  HtmlRndrTableOpen(output, "100%", 0, 0, 10, NULL);

  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
  HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
  handle_submit_error_flg(whc, "submit_error_flg", "plugin.required.restart");
  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CFG_WEBSENSE);
  HtmlRndrTdClose(output);
  HtmlRndrTrClose(output);

  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
  switch (OnOff) {
  case 0:
    ink_snprintf(websense_status, sizeof(websense_status), "%s\n%s",
                 "<input type=\"radio\" name=\"proxy.config.plugin.websense.enabled\" value=\"1\"> Enabled <br>",
                 "<input type=\"radio\" name=\"proxy.config.plugin.websense.enabled\" value=\"0\" checked> Disabled");
    break;
  case 1:
    ink_snprintf(websense_status, sizeof(websense_status), "%s\n%s",
                 "<input type=\"radio\" name=\"proxy.config.plugin.websense.enabled\" value=\"1\" checked> Enabled <br>",
                 "<input type=\"radio\" name=\"proxy.config.plugin.websense.enabled\" value=\"0\"> Disabled");
    break;
  case -2:
    ink_snprintf(websense_status, sizeof(websense_status), "%s\n", "Configuration not available <br>");
    break;
  }
  output->copyFrom(websense_status, strlen(websense_status));
  HtmlRndrTdClose(output);
  HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_LEFT, HTML_VALIGN_TOP, NULL, NULL, 0);
  HtmlRndrUlOpen(output);
  HtmlRndrLi(output);
  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CFG_WEBSENSE_HELP);
  HtmlRndrUlClose(output);
  HtmlRndrTdClose(output);
  HtmlRndrTrClose(output);
  HtmlRndrTableClose(output);

done:
  return WEB_HTTP_ERR_OKAY;

}

//-------------------------------------------------------------------------
// getMaximumClientConnections
// From the real proxy proxy.lic file
//-------------------------------------------------------------------------
int
getMaximumClientConnections()
{
  int Mcc;
  char *path;
  char *end_of_path1;
  char license_path[1024];
  char *fileBuff;
  char *ptr, *end_ptr;
  FILE *fp;
  long fsize;

  path = GetRmCfgPath();
  if (!path) {
    fprintf(stderr, "Error:rmserver.cfg path not found!\n");
    goto Lerror;
  }
  if ((end_of_path1 = strstr(path, "/rmserver.cfg")) != NULL) {
    memset(license_path, 0, 1024);
    strncpy(license_path, path, (size_t) (end_of_path1 - path));
    strncat(license_path, "/License/proxy.lic", sizeof(license_path) - strlen(license_path) - 1);
  }
  if (path) {
    xfree(path);
  }
  if ((fp = fopen(license_path, "r")) == NULL) {
    mgmt_log(stderr, "Error: unable to open %s\n", license_path);
    goto Lerror;
  }
  /* Get the file size to alloc an text buffer */
  if (fseek(fp, 0, SEEK_END) < 0) {
    goto Lerror;
  } else {
    fsize = ftell(fp);
    rewind(fp);
    fileBuff = new char[fsize + 1];
    memset(fileBuff, 0, fsize + 1);
    if (fread(fileBuff, sizeof(char), fsize, fp) == (size_t) fsize) {
      fclose(fp);
    } else {
      fclose(fp);
      delete[]fileBuff;
      goto Lerror;
    }
  }
  if ((ptr = strstr(fileBuff, "\"ClientConnections\"")) != NULL) {
    if ((ptr = strstr(ptr + strlen("\"ClientConnections\""), "\"")) != NULL) {
      sscanf(ptr + 1, "%d", &Mcc);
    } else {
      delete[]fileBuff;
      goto Lerror;
    }
  } else {
    delete[]fileBuff;
    goto Lerror;
  }
  goto done;

Lerror:
  Mcc = -1;
  return Mcc;

done:
  delete[]fileBuff;
  Debug("web2", "getMaximumClientConnection =%d\n", Mcc);
  return Mcc;
}


static int
handle_license_check(WebHttpContext * whc, char *tag, char *arg)
{
  textBuffer *output = whc->response_bdy;
  int MCC = getMaximumClientConnections();
  if (MCC == -1) {
    output->copyFrom("disabled", strlen("disabled"));
  }
done:
  return WEB_HTTP_ERR_OKAY;

}
static int
handle_mcc(WebHttpContext * whc, char *tag, char *arg)
{
  textBuffer *output = whc->response_bdy;
  int MCC = getMaximumClientConnections();
  if (MCC == -1) {
    output->copyFrom("Not Available", strlen("Not Available"));
  } else {
    char tmpbuff[100];
    memset(tmpbuff, 0, 100);
    ink_snprintf(tmpbuff, sizeof(tmpbuff), "%d", MCC);
    output->copyFrom(tmpbuff, strlen(tmpbuff));
  }
done:
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// int GetArmOnOff ()
// Find if Arm is enabled
// return 0 if it is off
// return 1 if it is on
// return -1 if any error happens
//-------------------------------------------------------------------------
int
GetArmOnOff()
{
  return 0;
}

//-------------------------------------------------------------------------
// getPort
// get PNA port if arm is not enabled
// get PNA Redirect port if arm is enabled.
//-------------------------------------------------------------------------
int
getPort(int enabled)
{
  INKCfgContext ctx;
  INKRmServerEle *ele;
  int port;

  ctx = INKCfgContextCreate(INK_FNAME_RMSERVER);
  if (!ctx) {
    Debug("config", "[getport] can't allocate ctx memory");
    goto Lerror;
  }
  if (INKCfgContextGet(ctx) != INK_ERR_OKAY) {
    Debug("config", "[Rmserver getPort ] Failed to Get and Clear CfgContext");
    goto Lerror;
  }
  //  Debug("web", "INKCfgContextGet in WebHttpRender and arm is %d\n", enabled);
  switch (enabled) {
  case 0:
    ele = (INKRmServerEle *) CfgContextGetEleAt(ctx, (int) INK_RM_RULE_PNA_PORT);
    break;
  case 1:
    ele = (INKRmServerEle *) CfgContextGetEleAt(ctx, (int) INK_RM_RULE_PNA_RDT_PORT);
    break;
  case -1:
    goto Lerror;
  }
  port = ele->int_val;
  goto done;

Lerror:
  port = -1;

done:
  INKCfgContextDestroy(ctx);
  return port;

}

//-------------------------------------------------------------------------
// handle_rm_pna_port
//-------------------------------------------------------------------------

static int
handle_rm_pna_port(WebHttpContext * whc, char *tag, char *arg)
{
  textBuffer *output = whc->response_bdy;
  char port_str[20], arm_str[10];
  int port;
  int OnOff = GetArmOnOff();
  char *warning_str;

  ink_snprintf(arm_str, sizeof(arm_str), "%d", OnOff);
  memset(port_str, 0, 20);
  if (ink_hash_table_lookup(whc->submit_warn_ht, "rmserver_rule_1", (void **) &warning_str)) {
    ink_snprintf(port_str, sizeof(port_str), "%s", warning_str);
  } else {
    port = getPort(OnOff);
    if (port == -1) {
      goto done;
    }
    ink_snprintf(port_str, sizeof(port_str), "%d", port);
  }
  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
  HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
  handle_submit_error_flg(whc, "submit_error_flg", "rmserver_rule_1");
  switch (OnOff) {
  case 0:
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CFG_RM_PNA_PORT);
    break;
  case 1:
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CFG_RM_PNA_RDT_PORT);
    break;
  }
  HtmlRndrTdClose(output);
  HtmlRndrTrClose(output);

  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);
  HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_TEXT, "rmserver_rule_1", port_str, NULL, NULL, "5");
  HtmlRndrTdClose(output);
  HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_LEFT, HTML_VALIGN_TOP, NULL, NULL, 0);
  HtmlRndrUlOpen(output);
  HtmlRndrLi(output);
  switch (OnOff) {
  case 0:
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CFG_RM_PNA_PORT_HELP);
    break;
  case 1:
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CFG_RM_PNA_RDT_PORT_HELP);
    break;
  }
  HtmlRndrUlClose(output);
  HtmlRndrTdClose(output);
  HtmlRndrTrClose(output);

done:
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_rm_rule
//-------------------------------------------------------------------------

static int
handle_rm_rule(WebHttpContext * whc, char *tag, char *arg)
{
  char rule_value[MAX_VAL_LENGTH];
  INKCfgContext ctx;
  INKRmServerEle *ele;
  int rule_no;
  char *warning_str;

  if (arg != NULL) {
    if (ink_hash_table_lookup(whc->submit_warn_ht, arg, (void **) &warning_str)) {
      ink_snprintf(rule_value, sizeof(rule_value), "%s", warning_str);
    } else {
      ctx = INKCfgContextCreate(INK_FNAME_RMSERVER);
      if (!ctx) {
        Debug("config", "[handle_rm_rule] can't allocate ctx memory");
        goto Lerror;
      }
      if (INKCfgContextGet(ctx) != INK_ERR_OKAY) {
        Debug("config", "[handle_rm_rule] Failed to Get and Clear CfgContext");
        goto Lerror;
      }
      sscanf(arg, "rmserver_rule_%d", &rule_no);
      switch (rule_no) {
      case 0:{
          ele = (INKRmServerEle *) CfgContextGetEleAt(ctx, (int) INK_RM_RULE_ADMIN_PORT);
          break;
        }
      case 2:{
          ele = (INKRmServerEle *) CfgContextGetEleAt(ctx, (int) INK_RM_RULE_MAX_PROXY_CONN);
          break;
        }
      case 3:{
          ele = (INKRmServerEle *) CfgContextGetEleAt(ctx, (int) INK_RM_RULE_MAX_GWBW);
          break;
        }
      case 4:{
          ele = (INKRmServerEle *) CfgContextGetEleAt(ctx, (int) INK_RM_RULE_MAX_PXBW);
          break;
        }
      default:
        goto Lerror;
      }
      ink_snprintf(rule_value, sizeof(rule_value), "%d", ele->int_val);
      INKCfgContextDestroy(ctx);
    }
  } else {
    mgmt_log(stderr, "[handle_rm_rule] no argument passed to <@%s ...>", tag);
  }
  goto done;

Lerror:
  ink_snprintf(rule_value, sizeof(rule_value), "%s", "not found");

done:
  whc->response_bdy->copyFrom(rule_value, strlen(rule_value));
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_vscan_plugin_status
//-------------------------------------------------------------------------

static int
handle_vscan_plugin_status(WebHttpContext * whc, char *tag, char *arg)
{

  char *old_value, *dummy;
  textBuffer *output;
  char vscan_plugin_status[256];
  int OnOff;

  output = whc->response_bdy;

  if (whc->post_data_ht &&      // render old values if any
      ink_hash_table_lookup(whc->post_data_ht, "proxy.config.plugin.vscan.enabled", (void **) &old_value)) {
    OnOff = atoi(old_value);
  } else
    OnOff = GetPlugInEnableStatus(whc, PLUGIN_VSCAN);

  HtmlRndrTableOpen(output, "100%", 0, 0, 10, NULL);

  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
  HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_LABEL, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, "2", 2);
  handle_submit_error_flg(whc, "submit_error_flg", "plugin.required.restart");
  HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CFG_VIRUS_SCAN);
  HtmlRndrTdClose(output);
  HtmlRndrTrClose(output);

  HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
  HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_LEFT, HTML_VALIGN_NONE, NULL, NULL, 0);

  switch (OnOff) {
  case 0:
    ink_snprintf(vscan_plugin_status, sizeof(vscan_plugin_status), "%s\n%s",
                 "<input type=\"radio\" name=\"proxy.config.plugin.vscan.enabled\" value=\"1\"> Enabled <br>",
                 "<input type=\"radio\" name=\"proxy.config.plugin.vscan.enabled\" value=\"0\" checked> Disabled");
    break;
  case 1:
    ink_snprintf(vscan_plugin_status, sizeof(vscan_plugin_status), "%s\n%s",
                 "<input type=\"radio\" name=\"proxy.config.plugin.vscan.enabled\" value=\"1\" checked> Enabled <br>",
                 "<input type=\"radio\" name=\"proxy.config.plugin.vscan.enabled\" value=\"0\"> Disabled");
    break;
  case -1:
    ink_snprintf(vscan_plugin_status, sizeof(vscan_plugin_status), "%s\n",
                 "Plugin is not installed or hasn't been installed properly <br>");
    break;
  case -2:
    ink_snprintf(vscan_plugin_status, sizeof(vscan_plugin_status), "%s\n", "Configuration not available <br>");
    break;
  }

  output->copyFrom(vscan_plugin_status, strlen(vscan_plugin_status));
  HtmlRndrTdClose(output);
  if (OnOff == 0 || OnOff == 1) {       // only display help when plugin is properly installed
    HtmlRndrTdOpen(output, HTML_CSS_CONFIGURE_HELP, HTML_ALIGN_LEFT, HTML_VALIGN_TOP, NULL, NULL, 0);
    HtmlRndrUlOpen(output);
    HtmlRndrLi(output);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CFG_VIRUS_SCAN_HELP);
    HtmlRndrUlClose(output);
    HtmlRndrTdClose(output);
  }
  HtmlRndrTrClose(output);
  HtmlRndrTableClose(output);

Ldone:
  return WEB_HTTP_ERR_OKAY;
}

/*
  handle_vscan_plugin_config_object
  This handle determines whether to render the dispaly of plugin configuration.
  If the plugin is not installed properly, then no plugin configuration dispaly
  will be rendered
 */
static int
handle_vscan_plugin_config_object(WebHttpContext * whc, char *tag, char *arg)
{

  if (GetPlugInEnableStatus(whc, PLUGIN_VSCAN) >= 0)
    WebHttpRender(whc, "/configure/c_plugin_vscan_confobj.ink");
  return WEB_HTTP_ERR_OKAY;
}

/*
  server.address has the format of Server:ip:port;;;Server:ip:port;;;ip:port
 */

int
parseVscanServerAddr(char *str, VscanObj_t type, int index, char *&rule_value)
{

  char *ip_port_pair = NULL;
  bool found = false;
  int count = 0;
  char *retval = NULL;

  if (strlen(str) <= 0) {       // empty string
    return WEB_HTTP_ERR_FAIL;
  }

  if (strstr(str, ";;;") != NULL) {     // has more than one server
    ip_port_pair = strtok(str, ";;;");
    while (ip_port_pair) {
      index--;
      if (index == 0) {
        found = true;
        break;
      }
      ip_port_pair = strtok(NULL, ";;;");
    }
  } else {
    if (index == 1) {
      found = true;
      ip_port_pair = str;
    }
  }

  if (found) {                  //found Server:ip:port pair
    strtok(ip_port_pair, ":");  // skip Server
    if (type == VSCAN_PORT) {
      strtok(NULL, ":");
    }
    retval = strtok(NULL, ":");
    strcpy(rule_value, retval);
    // get rid of ;;;
    retval = strstr(rule_value, ";");
    if (retval)
      *retval = '\0';
  } else
    return WEB_HTTP_ERR_FAIL;

  return WEB_HTTP_ERR_OKAY;
}

/*
  handle_vscan_rule
  parse vscan.config file
  return server name, server port
 */
static int
handle_vscan_rule(WebHttpContext * whc, char *tag, char *arg)
{

  char rule_value[MAX_VAL_LENGTH] = { 0 };
  INKCfgContext ctx;
  INKCfgIterState ctx_state;
  INKVscanEle *ele;
  char *warning_str;
  VscanObj_t query_type;
  int query_index;
  char *value_safe = NULL, *old_value = NULL, *dummy = NULL;
  // render old values

  if (arg == NULL) {
    mgmt_log(stderr, "[handle_vscan_rule] no argument passed to <@%s ...>", tag);
    goto done;
  }

  if (whc->post_data_ht && ink_hash_table_lookup(whc->post_data_ht, arg, (void **) &old_value)) {
    if (old_value && strlen(old_value) != 0) {
      ink_strncpy(rule_value, old_value, sizeof(rule_value));
    }                           // else, could be empty value
    goto done;
  }

  if (ink_hash_table_lookup(whc->submit_warn_ht, arg, (void **) &warning_str)) {
    ink_snprintf(rule_value, sizeof(rule_value), "%s", warning_str);
  } else {
    // first, parse SERVER/PORT and server_no
    if (strstr(arg, "vscan_rule_server") != NULL) {     // server
      query_type = VSCAN_SERVER;
      sscanf(arg, "vscan_rule_server_%d", &query_index);
    } else if (strstr(arg, "vscan_rule_port") != NULL) {        // port
      query_type = VSCAN_PORT;
      sscanf(arg, "vscan_rule_port_%d", &query_index);
    } else {
      goto Lerror;
    }

    ctx = INKCfgContextCreate(INK_FNAME_VSCAN);
    if (!ctx) {
      Debug("config", "[handle_vscan_rule] can't allocate ctx memory");
      goto Lerror;
    }
    if (INKCfgContextGet(ctx) != INK_ERR_OKAY) {
      Debug("config", "[handle_vscan_rule] Failed to Get and Clear CfgContext");
      goto Lerror;
    }
    // currently, only server.address attribute is pull from the file
    // need to fix the code below if more fields are needed
    ele = (INKVscanEle *) INKCfgContextGetFirst(ctx, &ctx_state);
    while (ele) {
      if (strcmp(ele->attr_name, "server.address") == 0) {
        if (parseVscanServerAddr(xstrdup(ele->attr_val), query_type, query_index, rule_value) != WEB_HTTP_ERR_OKAY)
          goto Lerror;
        break;
      }
      ele = (INKVscanEle *) INKCfgContextGetNext(ctx, &ctx_state);
    }
  }
  goto done;

Lerror:
  ink_snprintf(rule_value, sizeof(rule_value), "%s", "\0");

done:
  whc->response_bdy->copyFrom(rule_value, strlen(rule_value));
  return WEB_HTTP_ERR_OKAY;

}

//-------------------------------------------------------------------------
// handle_vscan_trusted_hosts_list
//-------------------------------------------------------------------------

static int
handle_vscan_trusted_hosts_list(WebHttpContext * whc, char *tag, char *arg)
{

  int host_count = 0;
  INKCfgContext ctx;
  INKCfgIterState ctx_state;
  INKVsTrustedHostEle *ele;
  textBuffer *output = whc->response_bdy;
  char *old_value = NULL;
  char tmp[32];

  if (arg && strcmp(arg, "new_trusted_host") == 0) {
    if (whc->post_data_ht)
      ink_hash_table_lookup(whc->post_data_ht, arg, (void **) &old_value);
    // no old value foun
    if (old_value)
      whc->response_bdy->copyFrom(old_value, strlen(old_value));
    return WEB_HTTP_ERR_OKAY;
  }

  ctx = INKCfgContextCreate(INK_FNAME_VS_TRUSTED_HOST);
  INKCfgContextGet(ctx);
  ele = (INKVsTrustedHostEle *) INKCfgContextGetFirst(ctx, &ctx_state);
  while (ele) {
    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    ink_snprintf(tmp, sizeof(tmp), "host:%d", host_count);
    HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_HIDDEN, tmp, ele->hostname, NULL, NULL);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, "99%", NULL, 0);
    output->copyFrom(ele->hostname, strlen(ele->hostname));
    HtmlRndrTdClose(output);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_CENTER, HTML_VALIGN_NONE, NULL, NULL, 0);
    ink_snprintf(tmp, sizeof(tmp), "delete:%d", host_count);
    HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_CHECKBOX, tmp, ele->hostname, NULL, NULL);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
    // move on
    ele = (INKVsTrustedHostEle *) INKCfgContextGetNext(ctx, &ctx_state);
    host_count++;
  }
  if (host_count == 0) {
    HtmlRndrTrOpen(output, HTML_CSS_NONE, HTML_ALIGN_NONE);
    HtmlRndrTdOpen(output, HTML_CSS_BODY_TEXT, HTML_ALIGN_NONE, HTML_VALIGN_NONE, NULL, NULL, 4);
    HtmlRndrText(output, whc->lang_dict_ht, HTML_ID_CFG_VSCAN_NO_TRUSTED_HOSTS);
    HtmlRndrTdClose(output);
    HtmlRndrTrClose(output);
  }
  // add hidden form tags
  ink_snprintf(tmp, sizeof(tmp), "%d", host_count);
  HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_HIDDEN, "host_count", tmp, NULL, NULL);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// handle_vscan_extensions_list
//-------------------------------------------------------------------------

static int
handle_vscan_extensions_list(WebHttpContext * whc, char *tag, char *arg)
{

  int ext_count = 0;
  INKCfgContext ctx;
  INKCfgIterState ctx_state;
  INKVsExtensionEle *ele;
  textBuffer *output = whc->response_bdy;
  char *old_value = NULL;
  char tmp[32];

  if (arg && strcmp(arg, "new_file_extension") == 0) {
    if (whc->post_data_ht)
      ink_hash_table_lookup(whc->post_data_ht, arg, (void **) &old_value);
    // no old value found
    if (old_value)
      whc->response_bdy->copyFrom(old_value, strlen(old_value));
    return WEB_HTTP_ERR_OKAY;
  }

  ctx = INKCfgContextCreate(INK_FNAME_VS_EXTENSION);
  INKCfgContextGet(ctx);
  ele = (INKVsExtensionEle *) INKCfgContextGetFirst(ctx, &ctx_state);
  while (ele) {
    HtmlRndrOptionOpen(output, ele->file_ext, false);
    output->copyFrom(ele->file_ext, strlen(ele->file_ext));
    HtmlRndrOptionClose(output);
    ele = (INKVsExtensionEle *) INKCfgContextGetNext(ctx, &ctx_state);
    ext_count++;
  }
  // add hidden form tags
  ink_snprintf(tmp, sizeof(tmp), "%d", ext_count);
  HtmlRndrInput(output, HTML_CSS_NONE, HTML_TYPE_HIDDEN, "ext_count", tmp, NULL, NULL);
  return WEB_HTTP_ERR_OKAY;
}

#endif

//-------------------------------------------------------------------------
// handle_ssl_redirect_url
//-------------------------------------------------------------------------

static int
handle_ssl_redirect_url(WebHttpContext * whc, char *tag, char *arg)
{

  RecInt ssl_value = 0;         // current SSL value, enabled/disabled
  char *hostname_FQ = NULL;
  char ssl_redirect_url[256] = "";
  char *link = NULL;

  // get current SSL value and fully qualified local hostname
  if (RecGetRecordInt("proxy.config.admin.use_ssl", &ssl_value) != REC_ERR_OKAY)
    mgmt_log(stderr, "[handle_ssl_redirect_url] Error: Unable to get SSL enabled config variable\n");
  if (RecGetRecordString_Xmalloc("proxy.node.hostname_FQ", &hostname_FQ) != REC_ERR_OKAY)
    mgmt_log(stderr, "[handle_ssl_redirect_url] Error: Unable to get local hostname \n");

  link = WebHttpGetLink_Xmalloc(HTML_MGMT_GENERAL_FILE);

  // construct proper redirect url
  ink_snprintf(ssl_redirect_url, sizeof(ssl_redirect_url), "%s://%s:%d%s",
               ssl_value ? "https" : "http", hostname_FQ, wGlobals.webPort, link);

  whc->response_bdy->copyFrom(ssl_redirect_url, strlen(ssl_redirect_url));

  // free allocated space
  xfree(link);
  if (hostname_FQ)
    xfree(hostname_FQ);

  return WEB_HTTP_ERR_OKAY;

}

//-------------------------------------------------------------------------
// handle_host_redirect_url
//-------------------------------------------------------------------------

static int
handle_host_redirect_url(WebHttpContext * whc, char *tag, char *arg)
{

  RecInt ssl_value = 0;         // current SSL value, enabled/disabled
  char hostname[1024];
  char host_redirect_url[256] = "";
  char *link = NULL;

  // get current SSL value and fully qualified local hostname
  if (RecGetRecordInt("proxy.config.admin.use_ssl", &ssl_value) != REC_ERR_OKAY)
    mgmt_log(stderr, "[handle_ssl_redirect_url] Error: Unable to get SSL enabled config variable\n");
  gethostname(hostname, 1024);
  link = WebHttpGetLink_Xmalloc("/configure/c_net_config.ink");

  // construct proper redirect url
  ink_snprintf(host_redirect_url, sizeof(host_redirect_url), "%s://%s:%d%s",
               ssl_value ? "https" : "http", hostname, wGlobals.webPort, link);

  whc->response_bdy->copyFrom(host_redirect_url, strlen(host_redirect_url));

  // free allocated space
  xfree(link);
  return WEB_HTTP_ERR_OKAY;

}

//-------------------------------------------------------------------------
// handle_network
//-------------------------------------------------------------------------
static int
handle_network(WebHttpContext * whc, char *tag, char *arg)
{
  int err = WEB_HTTP_ERR_OKAY;
#if (HOST_OS == linux) || (HOST_OS == sunos)
  char value[1024];
  char *value_safe, *old_value, *dummy;
  char *pos;
  char *interface;

  if (ink_hash_table_lookup(whc->submit_warn_ht, arg, (void **) &dummy)) {
    if (ink_hash_table_lookup(whc->post_data_ht, arg, (void **) &old_value)) {
      // coverity[var_assign]
      value_safe = substituteForHTMLChars(old_value);
      // coverity[noescape]
      whc->response_bdy->copyFrom(value_safe, strlen(value_safe));
    }
    goto Ldone;
  }

  if (strcmp(arg, "HOSTNAME") == 0) {
    Config_GetHostname(value, sizeof(value));
  } else if (strcmp(arg, "GATEWAY") == 0) {
    Config_GetDefaultRouter(value, sizeof(value));
  } else if (strstr(arg, "DNS") != NULL) {
    if (strstr(arg, "1") != NULL) {
      Config_GetDNS_Server(value, sizeof(value), 0);
    } else if (strstr(arg, "2") != NULL) {
      Config_GetDNS_Server(value, sizeof(value), 1);
    } else if (strstr(arg, "3") != NULL) {
      Config_GetDNS_Server(value, sizeof(value), 2);
    }
  } else if (strcmp(arg, "domain") == 0) {
    Config_GetDomain(value, sizeof(value));
  } else {
    interface = strchr(arg, '_') + 1;
    pos = strchr(interface, '_');
    *pos = '\0';
    arg = pos + 1;
    if (strcmp(arg, "up") == 0) {
      Config_GetNIC_Status(interface, value, sizeof(value));
      if (strcmp(value, "up") == 0) {
        ink_strncpy(value, "checked", sizeof(value));
      } else {
        ink_strncpy(value, "", sizeof(value));
      }
    } else if (strcmp(arg, "down") == 0) {
      Config_GetNIC_Status(interface, value, sizeof(value));
      if (interface[strlen(interface) - 1] == '0') {
        ink_strncpy(value, "disabled", sizeof(value));
      } else if (strcmp(value, "down") == 0) {
        ink_strncpy(value, "checked", sizeof(value));
      } else {
        ink_strncpy(value, "", sizeof(value));
      }
    } else if (strcmp(arg, "boot_enable") == 0) {
      Config_GetNIC_Start(interface, value, sizeof(value));
      if (strcmp(value, "onboot") == 0) {
        ink_strncpy(value, "checked", sizeof(value));
      } else {
        ink_strncpy(value, "", sizeof(value));
      }
    } else if (strcmp(arg, "boot_disable") == 0) {
      Config_GetNIC_Start(interface, value, sizeof(value));
      if (interface[strlen(interface) - 1] == '0') {
        ink_strncpy(value, "disabled", sizeof(value));
      } else if (strcmp(value, "not-onboot") == 0) {
        ink_strncpy(value, "checked", sizeof(value));
      } else {
        ink_strncpy(value, "", sizeof(value));
      }
    } else if (strcmp(arg, "boot_static") == 0) {
      Config_GetNIC_Protocol(interface, value, sizeof(value));
      if (strcmp(value, "none") == 0 || strcmp(value, "static") == 0) {
        ink_strncpy(value, "checked", sizeof(value));
      } else {
        ink_strncpy(value, "", sizeof(value));
      }
    } else if (strcmp(arg, "boot_dynamic") == 0) {
      Config_GetNIC_Protocol(interface, value, sizeof(value));
      if (strcmp(value, "dhcp") == 0) {
        ink_strncpy(value, "checked", sizeof(value));
      } else {
        ink_strncpy(value, "", sizeof(value));
      }
    } else if (strcmp(arg, "updown") == 0) {
      Config_GetNIC_Status(interface, value, sizeof(value));
      if (strcmp(value, "up") == 0) {
        char protocol[80];
        Config_GetNIC_Protocol(interface, protocol, sizeof(value));
        if (strcmp(protocol, "dhcp") == 0) {
          strncat(value, " (DHCP)", sizeof(value) - strlen(value));
        }
      }
    } else if (strcmp(arg, "yesno") == 0) {
      Config_GetNIC_Start(interface, value, sizeof(value));
      if (strcmp(value, "onboot") == 0) {
        ink_strncpy(value, "yes", sizeof(value));
        char protocol[80];
        Config_GetNIC_Protocol(interface, protocol, sizeof(protocol));
        if (strcmp(protocol, "dhcp") == 0) {
          strncat(value, " (DHCP)", sizeof(value) - strlen(value) - 1);
        }
      } else {
        ink_strncpy(value, "no", sizeof(value));
      }
    } else if (strcmp(arg, "staticdynamic") == 0) {
      Config_GetNIC_Protocol(interface, value, sizeof(value));
      if (strcmp(value, "dhcp") == 0) {
        ink_strncpy(value, "dynamic", sizeof(value));
      } else {
        ink_strncpy(value, "static", sizeof(value));
      }
    } else if (strcmp(arg, "IPADDR") == 0) {
      if (Config_GetNIC_IP(interface, value, sizeof(value)) == 0) {
        char protocol[80];
        Config_GetNIC_Protocol(interface, protocol, sizeof(protocol));
        if (strcmp(protocol, "dhcp") == 0) {
          strncat(value, " (DHCP)", sizeof(value) - strlen(value) - 1);
        }
      }
    } else if (strcmp(arg, "NETMASK") == 0) {
      if (Config_GetNIC_Netmask(interface, value, sizeof(value)) == 0) {
        char protocol[80];
        Config_GetNIC_Protocol(interface, protocol, sizeof(protocol));
        if (strcmp(protocol, "dhcp") == 0) {
          strncat(value, " (DHCP)", sizeof(value) - strlen(value) - 1);
        }
      }
    } else if (strcmp(arg, "GATEWAY") == 0) {
      Config_GetNIC_Gateway(interface, value, sizeof(value));
    }
  }

  // coverity[var_assign]
  value_safe = substituteForHTMLChars(value);
  // coverity[noescape]
  whc->response_bdy->copyFrom(value_safe, strlen(value_safe));
  xfree(value_safe);
Ldone:
  return err;
#else
  return err;
#endif
}


//-------------------------------------------------------------------------
// handle_network_object
//-------------------------------------------------------------------------

static int
handle_network_object(WebHttpContext * whc, char *tag, char *arg)
{
#if (HOST_OS == linux)
  char *device_ink_path, *template_ink_path;
  char command[200], tmpname[80], interface[80];

  if (strcmp(arg, "configure") == 0) {
    template_ink_path = WebHttpAddDocRoot_Xmalloc(whc, "/configure/c_net_device.ink");
  } else {
    template_ink_path = WebHttpAddDocRoot_Xmalloc(whc, "/monitor/m_net_device.ink");
  }

  int count = Config_GetNetworkIntCount();
  for (int i = 0; i < count; i++) {
    Config_GetNetworkInt(i, interface, sizeof(interface));
    ink_strncpy(tmpname, "/", sizeof(tmpname));
    strncat(tmpname, arg, sizeof(tmpname) - strlen(tmpname) - 1);
    strncat(tmpname, "/", sizeof(tmpname) - strlen(tmpname) - 1);
    strncat(tmpname, interface, sizeof(tmpname) - strlen(tmpname) - 1);
    strncat(tmpname, ".ink", sizeof(tmpname) - strlen(tmpname) - 1);

    device_ink_path = WebHttpAddDocRoot_Xmalloc(whc, tmpname);
    remove(device_ink_path);
    ink_strncpy(command, "cat ", sizeof(command));
    strncat(command, template_ink_path, sizeof(command) - strlen(command) - 1);
    strncat(command, "| sed 's/netdev/", sizeof(command) - strlen(command) - 1);
    strncat(command, interface, sizeof(command) - strlen(command) - 1);
    strncat(command, "/g' >", sizeof(command) - strlen(command) - 1);
    strncat(command, device_ink_path, sizeof(command) - strlen(command) - 1);
    strncat(command, " 2>/dev/null", sizeof(command) - strlen(command) - 1);
    NOWARN_UNUSED_RETURN(system(command));
    WebHttpRender(whc, tmpname);
    remove(device_ink_path);

    xfree(device_ink_path);
    xfree(template_ink_path);
  }
  if (template_ink_path)
    xfree(template_ink_path);
#endif
  return WEB_HTTP_ERR_OKAY;
}

#ifdef OEM
//-------------------------------------------------------------------------
// handle_driver_object
//-------------------------------------------------------------------------

static int
handle_driver_object(WebHttpContext * whc, char *tag, char *arg)
{
  char *driver_ink_path, *template_ink_path;
  FILE *net_device;
  char buffer[200], command[200], tmpname[80];
  int i, space_len;
  char *pos, *interface;

  net_device = fopen("/proc/net/dev", "r");
  for (i = 0; i < 3; i++) {
    fgets(buffer, 200, net_device);
  }
  template_ink_path = WebHttpAddDocRoot_Xmalloc(whc, "/configure/c_net_devdri.ink");
  fgets(buffer, 200, net_device);
  while (!feof(net_device)) {
    pos = strchr(buffer, ':');
    *pos = '\0';
    space_len = strspn(buffer, " ");
    interface = buffer + space_len;
    ink_strncpy(tmpname, "/configure/driver_", sizeof(tmpname));
    strncat(tmpname, interface, sizeof(tmpname) - strlen(tmpname) - 1);
    strncat(tmpname, ".ink", sizeof(tmpname) - strlen(tmpname) - 1);

    driver_ink_path = WebHttpAddDocRoot_Xmalloc(whc, tmpname);
    remove(driver_ink_path);
    ink_strncpy(command, "cat ", sizeof(command));
    strncat(command, template_ink_path, sizeof(command) - strlen(command) - 1);
    strncat(command, "| sed 's/netdev/", sizeof(command) - strlen(command) - 1);
    strncat(command, interface, sizeof(command) - strlen(command) - 1);
    strncat(command, "/g' >", sizeof(command) - strlen(command) - 1);
    strncat(command, driver_ink_path, sizeof(command) - strlen(command) - 1);
    strncat(command, " 2>/dev/null", sizeof(command) - strlen(command) - 1);
    system(command);

    WebHttpRender(whc, tmpname);
    remove(driver_ink_path);
    fgets(buffer, 200, net_device);
  }

  return WEB_HTTP_ERR_OKAY;
}
#endif


#ifdef OEM
#if (HOST_OS == linux)
char *
removequotes(char *find)
{
  char *newword = new char[1024];
  int i = 0;
  while (*find) {
    if (*find != '\"') {
      newword[i] = *find;
      i++;
    }
    find++;
  }
  newword[i] = '\0';
  return newword;
}
#endif
#endif

//handle_snmp
#ifdef OEM
#if (HOST_OS == linux)
static int
handle_snmp(WebHttpContext * whc, char *tag, char *arg)
{
  int err = WEB_HTTP_ERR_OKAY;
  char buffer[1024], value[1024], command[256];
  char *value_safe, *old_value, *dummy;
  char *pos, *open_quot, *close_quot;
  char *interface;
  char *newvalue;
  char sys_name[1024], sys_loc[1024], sys_contact[1024], auth_trap[1024], trap_commun[1024], trap_host[1024];

#if (HOST_OS == linux)
  Config_SNMPGetInfo(sys_loc, sizeof(sys_loc), sys_contact, sizeof(sys_contact), sys_name, sizeof(sys_name), auth_trap,
                     sizeof(auth_trap), trap_commun, sizeof(trap_commun), trap_host, sizeof(trap_host));
  value[0] = '\0';
  if (strcmp(arg, "SNMP_SYSTEM_NAME") == 0)
    strncpy(value, sys_name, 1024);
  value[1023] = '\0';
  newvalue = removequotes(value);
  ink_strncpy(value, newvalue, sizeof(value));
  delete[]newvalue;
  if (strcmp(arg, "SNMP_TRAP_IP") == 0)
    strncpy(value, trap_host, 1022);
  value[1023] = '\0';

  if (strcmp(arg, "AUTH_TRAP_ENABLE") == 0) {
    strncpy(value, auth_trap, 1022);
    value[1023] = '\0';
    if (strcmp(value, "1") == 0)
      ink_strncpy(value, "Enabled", sizeof(value));
    else if (strcmp(value, "2") == 0)
      ink_strncpy(value, "Disabled", sizeof(value));
    else
      ink_strncpy(value, "Blank", sizeof(value));

  }

  if (strcmp(arg, "SYS_LOCATION") == 0)
    strncpy(value, sys_loc, 1022);
  value[1023] = '\0';
  newvalue = removequotes(value);
  ink_strncpy(value, newvalue, sizeof(value));
  delete[]newvalue;
  if (strcmp(arg, "SYS_CONTACT") == 0)
    strncpy(value, sys_contact, 1022);
  value[1023] = '\0';
  newvalue = removequotes(value);
  ink_strncpy(value, newvalue, sizeof(value));
  delete[]newvalue;
  if (strcmp(arg, "COMMUNITY_NAME") == 0)
    strncpy(value, trap_commun, 1022);
  value[1023] = '\0';

  if (strcmp(arg, "auth_trap_enable") == 0) {
    strncpy(value, auth_trap, 1022);
    value[1023] = '\0';
    if (strcmp(value, "1") == 0)
      ink_strncpy(value, "checked", sizeof(value));
    else
      ink_strncpy(value, "", sizeof(value));
  }
  if (strcmp(arg, "auth_trap_disable") == 0) {
    strncpy(value, auth_trap, 1022);
    value[1023] = '\0';
    if (strcmp(value, "2") == 0)
      ink_strncpy(value, "checked", sizeof(value));
    else
      ink_strncpy(value, "", sizeof(value));
  }

  value_safe = substituteForHTMLChars(value);
  whc->response_bdy->copyFrom(value_safe, strlen(value_safe));
Ldone:
  return err;
#else
  return err;
#endif
}
#endif
#endif


//-------------------------------------------------------------------------
// WebHttpRenderInit
//-------------------------------------------------------------------------

void
WebHttpRenderInit()
{

  // bind display tags to their display handlers (e.g. <@tag ...> maps
  // to handle_tag())
  g_display_bindings_ht = ink_hash_table_create(InkHashTableKeyType_String);
  ink_hash_table_insert(g_display_bindings_ht, "alarm_object", (void *) handle_alarm_object);
  ink_hash_table_insert(g_display_bindings_ht, "alarm_summary_object", (void *) handle_alarm_summary_object);
  ink_hash_table_insert(g_display_bindings_ht, "file_edit", (void *) handle_file_edit);
  ink_hash_table_insert(g_display_bindings_ht, "include", (void *) handle_include);
  ink_hash_table_insert(g_display_bindings_ht, "overview_object", (void *) handle_overview_object);
  ink_hash_table_insert(g_display_bindings_ht, "overview_details_object", (void *) handle_overview_details_object);
  ink_hash_table_insert(g_display_bindings_ht, "query", (void *) handle_query);
  ink_hash_table_insert(g_display_bindings_ht, "post_data", (void *) handle_post_data);
  ink_hash_table_insert(g_display_bindings_ht, "record", (void *) handle_record);
  ink_hash_table_insert(g_display_bindings_ht, "record_version", (void *) handle_record_version);
  ink_hash_table_insert(g_display_bindings_ht, "summary_object", (void *) handle_summary_object);
  ink_hash_table_insert(g_display_bindings_ht, "tab_object", (void *) handle_tab_object);
  ink_hash_table_insert(g_display_bindings_ht, "html_tab_object", (void *) handle_html_tab_object);
  ink_hash_table_insert(g_display_bindings_ht, "mgmt_auth_object", (void *) handle_mgmt_auth_object);
  ink_hash_table_insert(g_display_bindings_ht, "tree_object", (void *) handle_tree_object);
  ink_hash_table_insert(g_display_bindings_ht, "vip_object", (void *) handle_vip_object);
  ink_hash_table_insert(g_display_bindings_ht, "checked", (void *) handle_checked);
  ink_hash_table_insert(g_display_bindings_ht, "action_checked", (void *) handle_action_checked);
  ink_hash_table_insert(g_display_bindings_ht, "select", (void *) handle_select);
  ink_hash_table_insert(g_display_bindings_ht, "password_object", (void *) handle_password_object);
#ifdef OEM
  ink_hash_table_insert(g_display_bindings_ht, "sessionid", (void *) handle_sessionid);
#endif //OEM
  ink_hash_table_insert(g_display_bindings_ht, "select_system_logs", (void *) handle_select_system_logs);
  ink_hash_table_insert(g_display_bindings_ht, "select_access_logs", (void *) handle_select_access_logs);
  ink_hash_table_insert(g_display_bindings_ht, "select_debug_logs", (void *) handle_select_debug_logs);
  ink_hash_table_insert(g_display_bindings_ht, "log_action", (void *) handle_log_action);
  ink_hash_table_insert(g_display_bindings_ht, "version", (void *) handle_version);
  // FIXME: submit_error_msg and submit_error_flg is kind of a bad
  // name, should pick something more like 'submit_diags_*'.  ^_^
  ink_hash_table_insert(g_display_bindings_ht, "submit_error_msg", (void *) handle_submit_error_msg);
  ink_hash_table_insert(g_display_bindings_ht, "submit_error_flg", (void *) handle_submit_error_flg);
  ink_hash_table_insert(g_display_bindings_ht, "link", (void *) handle_link);
  ink_hash_table_insert(g_display_bindings_ht, "link_file", (void *) handle_link_file);
  ink_hash_table_insert(g_display_bindings_ht, "link_query", (void *) handle_link_query);
  ink_hash_table_insert(g_display_bindings_ht, "cache_query", (void *) handle_cache_query);
  ink_hash_table_insert(g_display_bindings_ht, "cache_regex_query", (void *) handle_cache_regex_query);
  ink_hash_table_insert(g_display_bindings_ht, "time", (void *) handle_time);
  ink_hash_table_insert(g_display_bindings_ht, "user", (void *) handle_user);
  ink_hash_table_insert(g_display_bindings_ht, "plugin_object", (void *) handle_plugin_object);
#if defined(OEM)
  ink_hash_table_insert(g_display_bindings_ht, "snmp", (void *) handle_snmp);
  ink_hash_table_insert(g_display_bindings_ht, "websense_status", (void *) handle_websense_status);
  ink_hash_table_insert(g_display_bindings_ht, "rm_rule", (void *) handle_rm_rule);
  ink_hash_table_insert(g_display_bindings_ht, "rm_pna_port", (void *) handle_rm_pna_port);
  ink_hash_table_insert(g_display_bindings_ht, "license_check", (void *) handle_license_check);
  ink_hash_table_insert(g_display_bindings_ht, "mcc", (void *) handle_mcc);
  ink_hash_table_insert(g_display_bindings_ht, "vscan_plugin_status", (void *) handle_vscan_plugin_status);
  ink_hash_table_insert(g_display_bindings_ht, "vscan_plugin_config_object",
                        (void *) handle_vscan_plugin_config_object);
  ink_hash_table_insert(g_display_bindings_ht, "vscan_rule", (void *) handle_vscan_rule);
  ink_hash_table_insert(g_display_bindings_ht, "vscan_trusted_hosts_list", (void *) handle_vscan_trusted_hosts_list);
  ink_hash_table_insert(g_display_bindings_ht, "vscan_extensions_list", (void *) handle_vscan_extensions_list);
#endif
  ink_hash_table_insert(g_display_bindings_ht, "ssl_redirect_url", (void *) handle_ssl_redirect_url);
  ink_hash_table_insert(g_display_bindings_ht, "host_redirect_url", (void *) handle_host_redirect_url);
  ink_hash_table_insert(g_display_bindings_ht, "help_link", (void *) handle_help_link);
  ink_hash_table_insert(g_display_bindings_ht, "include_cgi", (void *) handle_include_cgi);

  ink_hash_table_insert(g_display_bindings_ht, "help_config_link", (void *) handle_help_config_link);
  ink_hash_table_insert(g_display_bindings_ht, "config_input_form", (void *) handle_config_input_form);
  ink_hash_table_insert(g_display_bindings_ht, "dynamic_javascript", (void *) handle_dynamic_javascript);
  ink_hash_table_insert(g_display_bindings_ht, "config_table_object", (void *) handle_config_table_object);
  ink_hash_table_insert(g_display_bindings_ht, "network", (void *) handle_network);
  ink_hash_table_insert(g_display_bindings_ht, "network_object", (void *) handle_network_object);
  ink_hash_table_insert(g_display_bindings_ht, "nntp_plugin_status", (void *) handle_nntp_plugin_status);
  ink_hash_table_insert(g_display_bindings_ht, "nntp_config_display", (void *) handle_nntp_config_display);
#ifdef OEM
  ink_hash_table_insert(g_display_bindings_ht, "date", (void *) handle_date);
  ink_hash_table_insert(g_display_bindings_ht, "driver_object", (void *) handle_driver_object);
  ink_hash_table_insert(g_display_bindings_ht, "ftp_logging", (void *) handle_ftp_logging);
#endif
  ink_hash_table_insert(g_display_bindings_ht, "clear_cluster_stats", (void *) handle_clear_cluster_stats);
  return;
}

//-------------------------------------------------------------------------
// WebHttpRender
//-------------------------------------------------------------------------

int
WebHttpRender(WebHttpContext * whc, const char *file)
{
  int err;
  char *file_buf;
  int file_size;
  char *doc_root_file;
  ink_debug_assert(file != NULL);
#if (HOST_OS == linux)
//Bug 49922, for those .ink files which may meet the root-only system files,  
//upgrade the uid to root.
  int old_euid;
  bool change_uid = false;
  if (strstr(file, "m_net.ink") != NULL ||
      strstr(file, "c_net_") != NULL || strstr(file, "c_time.ink") != NULL || strstr(file, "c_ntp.ink") != NULL) {

    change_uid = true;
    Config_User_Root(&old_euid);
  }
#endif

  doc_root_file = WebHttpAddDocRoot_Xmalloc(whc, file);
  // FIXME: probably should mmap here for better performance
  file_buf = 0;
  if (WebFileImport_Xmalloc(doc_root_file, &file_buf, &file_size) != WEB_HTTP_ERR_OKAY) {
    goto Lnot_found;
  }
  err = WebHttpRender(whc, file_buf, file_size);
  goto Ldone;

Lnot_found:

  // missing file
  mgmt_log(stderr, "[WebHttpRender] requested file not found (%s)", file);
  whc->response_hdr->setStatus(STATUS_NOT_FOUND);
  WebHttpSetErrorResponse(whc, STATUS_NOT_FOUND);
  err = WEB_HTTP_ERR_REQUEST_ERROR;
  goto Ldone;

Ldone:

#if (HOST_OS == linux) || (HOST_OS == sunos)
  if (change_uid) {
    Config_User_Inktomi(old_euid);
  }
#endif

  xfree(doc_root_file);
  if (file_buf)
    xfree(file_buf);
  return err;
}

int
WebHttpRender(WebHttpContext * whc, char *file_buf, int file_size)
{

  int err;
  char *cpy_p, *cur_p, *end_p;

  char *display_tag;
  char *display_arg;
  WebHttpDisplayHandler display_handler;

  // parse the file and call handlers
  cur_p = cpy_p = file_buf;
  end_p = file_buf + file_size;
  while (cur_p < end_p) {
    if (*cur_p == '<') {
      if (*(cur_p + 1) == '@' || *(cur_p + 1) == '#') {
        // copy the data from cpy_p to cur_p into resposne_bdy
        whc->response_bdy->copyFrom(cpy_p, cur_p - cpy_p);
        // set cpy_p to end of "<?...>" and zero '>'
        cpy_p = strstr(cur_p, ">");
        if (cpy_p == NULL) {
          goto Lserver_error;
        }
        *cpy_p = '\0';
        cpy_p++;
        switch (*(cur_p + 1)) {
        case '@':
          // tokenize arguments
          cur_p += 2;           // skip past '<@'
          display_tag = cur_p;
          display_arg = cur_p;
          while (*display_arg != ' ' && *display_arg != '\0')
            display_arg++;
          if (*display_arg == ' ') {
            *display_arg = '\0';
            display_arg++;
            while (*display_arg == ' ')
              display_arg++;
            if (*display_arg == '\0')
              display_arg = NULL;
          } else {
            display_arg = NULL;
          }
          // call the display handler
          if (display_tag != NULL) {
            if (ink_hash_table_lookup(g_display_bindings_ht, display_tag, (void **) &display_handler)) {
              if ((err = display_handler(whc, display_tag, display_arg)) != WEB_HTTP_ERR_OKAY) {
                goto Ldone;
              }
            } else {
              mgmt_log(stderr, "[WebHttpRender] invalid display tag (%s) ", display_tag);
            }
          } else {
            mgmt_log(stderr, "[WebHttpRender] missing display tag ");
          }
          break;
        case '#':
          cur_p += 2;           // skip past '<#'
          substitute_language(whc, cur_p);
          break;
        }
        // advance to one past the closing '>'
        cur_p = cpy_p;
      } else {
        // move along
        cur_p++;
      }
    } else {
      // move along
      cur_p++;
    }
  }

  // copy data from cpy_p to cur_p into resposne_bdy
  whc->response_bdy->copyFrom(cpy_p, cur_p - cpy_p);

  whc->response_hdr->setStatus(STATUS_OK);
  err = WEB_HTTP_ERR_OKAY;
  goto Ldone;

Lserver_error:

  // corrupt or truncated file
  mgmt_log(stderr, "[WebHttpRender] partial file detected");
  whc->response_hdr->setStatus(STATUS_INTERNAL_SERVER_ERROR);
  WebHttpSetErrorResponse(whc, STATUS_INTERNAL_SERVER_ERROR);
  err = WEB_HTTP_ERR_REQUEST_ERROR;
  goto Ldone;

Ldone:

  return err;

}

//-------------------------------------------------------------------------
// HtmlRndrTrOpen
//-------------------------------------------------------------------------

int
HtmlRndrTrOpen(textBuffer * html, HtmlCss css, HtmlAlign align)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<tr", 3);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (align) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " align=\"%s\"", align);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">\n", 2);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrTdOpen
//-------------------------------------------------------------------------

int
HtmlRndrTdOpen(textBuffer * html,
               HtmlCss css, HtmlAlign align, HtmlValign valign, char *width, char *height, int colspan, char *bg)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<td", 3);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (align) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " align=\"%s\"", align);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (valign) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " valign=\"%s\"", valign);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (width) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " width=\"%s\"", width);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (height) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " height=\"%s\"", height);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (colspan > 0) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " colspan=\"%d\"", colspan);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (bg) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " background=\"%s\"", bg);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">", 1);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrAOpen
//-------------------------------------------------------------------------

int
HtmlRndrAOpen(textBuffer * html, HtmlCss css, char *href, char *target, char *onclick)
{
  char tmp[512 + 1];            // larger, since href's can be lengthy
  html->copyFrom("<a", 2);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (href) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " href=\"%s\"", href);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (target) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " target=\"%s\"", target);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (onclick) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " onclick=\"%s\"", onclick);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">", 1);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrFormOpen
//-------------------------------------------------------------------------

int
HtmlRndrFormOpen(textBuffer * html, char *name, HtmlMethod method, char *action)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<form", 5);
  if (name) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " name=\"%s\"", name);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (method) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " method=\"%s\"", method);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (action) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " action=\"%s\"", action);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">\n", 2);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrTextareaOpen
//-------------------------------------------------------------------------

int
HtmlRndrTextareaOpen(textBuffer * html, HtmlCss css, int cols, int rows, HtmlWrap wrap, char *name, bool readonly)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<textarea", 9);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (cols > 0) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " cols=\"%d\"", cols);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (rows > 0) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " rows=\"%d\"", rows);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (wrap) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " wrap=\"%s\"", wrap);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (name) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " name=\"%s\"", name);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (readonly) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " readonly");
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">\n", 2);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrTableOpen
//-------------------------------------------------------------------------

int
HtmlRndrTableOpen(textBuffer * html, char *width, int border, int cellspacing, int cellpadding, char *bordercolor)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<table", 6);
  if (width > 0) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " width=\"%s\"", width);
    html->copyFrom(tmp, strlen(tmp));
  }
  ink_snprintf(tmp, MAX_TMP_BUF_LEN, " border=\"%d\"", border);
  html->copyFrom(tmp, strlen(tmp));
  ink_snprintf(tmp, MAX_TMP_BUF_LEN, " cellspacing=\"%d\"", cellspacing);
  html->copyFrom(tmp, strlen(tmp));
  ink_snprintf(tmp, MAX_TMP_BUF_LEN, " cellpadding=\"%d\"", cellpadding);
  html->copyFrom(tmp, strlen(tmp));
  if (bordercolor) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " bordercolor=\"%s\"", bordercolor);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">\n", 2);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrSpanOpen
//-------------------------------------------------------------------------

int
HtmlRndrSpanOpen(textBuffer * html, HtmlCss css)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<span", 5);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">", 1);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrSelectOpen
//-------------------------------------------------------------------------

int
HtmlRndrSelectOpen(textBuffer * html, HtmlCss css, char *name, int size)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<select", 7);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (name) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " name=\"%s\"", name);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (size > 0) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " size=\"%d\"", size);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">\n", 2);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrOptionOpen
//-------------------------------------------------------------------------

int
HtmlRndrOptionOpen(textBuffer * html, char *value, bool selected)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<option", 7);
  if (value) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " value=\"%s\"", value);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (selected) {
    html->copyFrom(" selected", 10);
  }
  html->copyFrom(">", 1);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrPreOpen
//-------------------------------------------------------------------------

int
HtmlRndrPreOpen(textBuffer * html, HtmlCss css, char *width)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<PRE", 4);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (width) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " width=\"%s\"", width);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">", 1);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrUlOpen
//-------------------------------------------------------------------------

int
HtmlRndrUlOpen(textBuffer * html)
{
  html->copyFrom("<ul>", 4);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrTrClose
//-------------------------------------------------------------------------

int
HtmlRndrTrClose(textBuffer * html)
{
  html->copyFrom("</tr>\n", 6);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrTdClose
//-------------------------------------------------------------------------

int
HtmlRndrTdClose(textBuffer * html)
{
  html->copyFrom("</td>\n", 6);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrAClose
//-------------------------------------------------------------------------

int
HtmlRndrAClose(textBuffer * html)
{
  html->copyFrom("</a>", 4);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrFormClose
//-------------------------------------------------------------------------

int
HtmlRndrFormClose(textBuffer * html)
{
  html->copyFrom("</form>\n", 8);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrTextareaClose
//-------------------------------------------------------------------------

int
HtmlRndrTextareaClose(textBuffer * html)
{
  html->copyFrom("</textarea>\n", 12);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrTableClose
//-------------------------------------------------------------------------

int
HtmlRndrTableClose(textBuffer * html)
{
  html->copyFrom("</table>\n", 9);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrSpanClose
//-------------------------------------------------------------------------

int
HtmlRndrSpanClose(textBuffer * html)
{
  html->copyFrom("</span>", 7);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrSelectClose
//-------------------------------------------------------------------------

int
HtmlRndrSelectClose(textBuffer * html)
{
  html->copyFrom("</select>\n", 10);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrOptionClose
//-------------------------------------------------------------------------

int
HtmlRndrOptionClose(textBuffer * html)
{
  html->copyFrom("</option>\n", 10);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrPreClose
//-------------------------------------------------------------------------

int
HtmlRndrPreClose(textBuffer * html)
{
  html->copyFrom("</pre>\n", 7);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrUlClose
//-------------------------------------------------------------------------

int
HtmlRndrUlClose(textBuffer * html)
{
  html->copyFrom("</ul>\n", 7);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrInput
//-------------------------------------------------------------------------
#ifdef OEM
int
HtmlRndrInput(textBuffer * html,
              HtmlCss css, HtmlType type, char *name, char *value, char *target, char *onclick, char *size = NULL)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<input", 6);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (type) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " type=\"%s\"", type);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (size) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " size=\"%s\"", size);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (name) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " name=\"%s\"", name);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (value) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " value=\"%s\"", value);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (target) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " target=\"%s\"", target);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (onclick) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " onclick=\"%s\"", onclick);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">\n", 2);
  return WEB_HTTP_ERR_OKAY;
}
#else
int
HtmlRndrInput(textBuffer * html, HtmlCss css, HtmlType type, char *name, char *value, char *target, char *onclick)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<input", 6);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (type) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " type=\"%s\"", type);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (name) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " name=\"%s\"", name);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (value) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " value=\"%s\"", value);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (target) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " target=\"%s\"", target);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (onclick) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " onclick=\"%s\"", onclick);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">\n", 2);
  return WEB_HTTP_ERR_OKAY;
}
#endif

//-------------------------------------------------------------------------
// HtmlRndrInput
//-------------------------------------------------------------------------

int
HtmlRndrInput(textBuffer * html, MgmtHashTable * dict_ht, HtmlCss css, HtmlType type, char *name, HtmlId value_id)
{

  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<input", 6);
  if (css) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " class=\"%s\"", css);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (type) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " type=\"%s\"", type);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (name) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " name=\"%s\"", name);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (value_id) {
    html->copyFrom(" value=\"", 8);
    HtmlRndrText(html, dict_ht, value_id);
    html->copyFrom("\"", 1);
  }
  html->copyFrom(">", 1);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrBr
//-------------------------------------------------------------------------

int
HtmlRndrBr(textBuffer * html)
{
  html->copyFrom("<br>\n", 5);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrLi
//-------------------------------------------------------------------------

int
HtmlRndrLi(textBuffer * html)
{
  html->copyFrom("<li>", 4);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrSpace
//-------------------------------------------------------------------------

int
HtmlRndrSpace(textBuffer * html, int num_spaces)
{
  while (num_spaces > 0) {
    html->copyFrom("&nbsp;", 6);
    num_spaces--;
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrText
//-------------------------------------------------------------------------

int
HtmlRndrText(textBuffer * html, MgmtHashTable * dict_ht, HtmlId text_id)
{
  char *value;
  if (dict_ht->mgmt_hash_table_lookup(text_id, (void **) &value)) {
    html->copyFrom(value, strlen(value));
  } else {
    if (dict_ht->mgmt_hash_table_lookup(HTML_ID_UNDEFINED, (void **) &value)) {
      html->copyFrom(value, strlen(value));
    }
  }
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrImg
//-------------------------------------------------------------------------

int
HtmlRndrImg(textBuffer * html, char *src, char *border, char *width, char *height, char *hspace)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  html->copyFrom("<img", 4);
  if (src) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " src=\"%s\"", src);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (border) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " border=\"%s\"", border);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (width) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " width=\"%s\"", width);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (height) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " height=\"%s\"", height);
    html->copyFrom(tmp, strlen(tmp));
  }
  if (hspace) {
    ink_snprintf(tmp, MAX_TMP_BUF_LEN, " HSPACE='%s'", hspace);
    html->copyFrom(tmp, strlen(tmp));
  }
  html->copyFrom(">", 1);
  return WEB_HTTP_ERR_OKAY;
}

//-------------------------------------------------------------------------
// HtmlRndrDotClear
//-------------------------------------------------------------------------

int
HtmlRndrDotClear(textBuffer * html, int width, int height)
{
  char tmp[MAX_TMP_BUF_LEN + 1];
  ink_snprintf(tmp, MAX_TMP_BUF_LEN, "<img src=\"" HTML_DOT_CLEAR "\" " "width=\"%d\" height=\"%d\">", width, height);
  html->copyFrom(tmp, strlen(tmp));
  return WEB_HTTP_ERR_OKAY;
}
