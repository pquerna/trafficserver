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

#ifndef _MAIN_H_
#define _MAIN_H_

#include "LocalManager.h"
#include "FileManager.h"
#include "MgmtPing.h"
//#include "SNMP.h"
#include "WebOverview.h"
#include "I_Version.h"

void MgmtShutdown(int status);
void fileUpdated(char *fname);
void runAsUser(char *userName);
void extractConfigInfo(char *mgmt_path, char *recs_conf, char *userName);
void printUsage(void);

extern MgmtPing *icmp_ping;
//extern SNMP *snmp;
extern FileManager *configFiles;
extern overviewPage *overviewGenerator;
extern AppVersionInfo appVersionInfo;

// Global string constants
extern char *mgmt_path;
extern char *recs_conf;
//extern char *lm_conf;

// Root of Traffic Server
extern char *ts_base_dir;

// Global variable to replace ifdef MGMT_LAUNCH_PROXY so that
// we can turn on/off proxy launch at runtime to facilitate
// manager only testing.
extern bool mgmt_launch_proxy;

#endif /* _MAIN_H_ */
