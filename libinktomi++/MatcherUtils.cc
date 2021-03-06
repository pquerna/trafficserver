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

/*****************************************************************************
 *
 *  MatcherUtils.cc - Various helper routines used in ControlMatcher 
 *                    and ReverseProxy
 *
 *
 ****************************************************************************/

#include "inktomi++.h"      /* MAGIC_EDITING_TAG */

// char* readIntoBuffer(const char* file_path, const char* module_name,
//                          int* read_size_ptr)
//
//  Attempts to open and read arg file_path into a buffer allocated
//   off the heap (via malloc() )  Returns a pointer to the buffer
//   is successful and NULL otherwise.
//
//  CALLEE is responsibled for deallocating the buffer via free()
//
char *
readIntoBuffer(char *file_path, const char *module_name, int *read_size_ptr)
{

  int fd;
  struct stat file_info;
  char *file_buf;
  int read_size = 0;

  if (read_size_ptr != NULL) {
    *read_size_ptr = 0;
  }
  // Open the file for Blocking IO.  We will be reading this
  //   at start up and infrequently afterward
  if ((fd = ink_open(file_path, O_RDONLY | _O_ATTRIB_NORMAL)) < 0) {
    Error("%s Can not open %s file : %s", module_name, file_path, strerror(errno));
    return NULL;
  }

  if (ink_fstat(fd, &file_info) < 0) {
    Error("%s Can not stat %s file : %s", module_name, file_path, strerror(errno));
    ink_close(fd);
    return NULL;
  }

  if (file_info.st_size < 0) {
    Error("%s Can not get correct file size for %s file : %lld", module_name, file_path, (long long) file_info.st_size);
    ink_close(fd);
    return NULL;
  }
  // Allocate a buffer large enough to hold the entire file
  //   File size should be small and this makes it easy to
  //   do two passes on the file
  if ((file_buf = (char *) xmalloc((file_info.st_size + 1))) != NULL) {
    // Null terminate the buffer so that string operations will work
    file_buf[file_info.st_size] = '\0';

    read_size = (file_info.st_size > 0) ? ink_read(fd, file_buf, file_info.st_size) : 0;

    // Check to make sure that we got the whole file
    if (read_size < 0) {
      Error("%s Read of %s file failed : %s", module_name, file_path, strerror(errno));
      xfree(file_buf);
      file_buf = NULL;
    } else if (read_size < file_info.st_size) {
      // We don't want to signal this error on WIN32 because the sizes
      // won't match if the file contains any CR/LF sequence.
      Error("%s Only able to read %d bytes out %d for %s file",
            module_name, read_size, (int) file_info.st_size, file_path);
      file_buf[read_size] = '\0';
    }
  } else {
    Error("%s Insufficient memory to read %s file", module_name, file_path);
  }

  if (file_buf && read_size_ptr) {
    *read_size_ptr = read_size;
  }

  ink_close(fd);

  return file_buf;
}

// void unescapifyStr(char* buffer)
//
//   Unescapifies a URL without a making a copy.
//    The passed in string is modified
//
void
unescapifyStr(char *buffer)
{
  char *read = buffer;
  char *write = buffer;
  char subStr[3];
  long charVal;

  subStr[2] = '\0';
  while (*read != '\0') {
    if (*read == '%' && *(read + 1) != '\0' && *(read + 2) != '\0') {
      subStr[0] = *(++read);
      subStr[1] = *(++read);
      charVal = strtol(subStr, (char **) NULL, 16);
      *write = (char) charVal;
      read++;
      write++;
    } else if (*read == '+') {
      *write = ' ';
      write++;
      read++;
    } else {
      *write = *read;
      write++;
      read++;
    }
  }
  *write = '\0';
}

//   char* ExtractIpRange(char* match_str, ip_addr_t* addr1,
//                         ip_addr_t* addr2)
//
//   Attempts to extract either an Ip Address or an IP Range
//     fron match_str.  The range should be two addresses
//     separated by a hyphen and no spaces
//
//   If the extraction is successful, sets addr1 and addr2
//     to the extracted values (in the case of a single
//     address addr2 = addr1) and returns NULL
//
//   If the extraction fails, returns a static string
//     that describes the reason for the error. 
//
char *
ExtractIpRange(char *match_str, ip_addr_t * addr1, ip_addr_t * addr2)
{
  Tokenizer rangeTok("-/");
  bool mask = false;
  int mask_bits;
  int mask_val;
  int numToks;
  ip_addr_t addr1_local;
  ip_addr_t addr2_local;

  if (strchr(match_str, '/') != NULL) {
    mask = true;
  }
  // Extract the IP addresses from match data
  numToks = rangeTok.Initialize(match_str, SHARE_TOKS);

  if (numToks < 0) {
    return "no IP address given";
  } else if (numToks > 2) {
    return "malformed IP range";
  }

  addr1_local = htonl(inet_addr(rangeTok[0]));

  if (addr1_local == (ip_addr_t) - 1 && strcmp(rangeTok[0], "255.255.255.255") != 0) {
    return "malformed ip address";
  }
  // Handle a IP range
  if (numToks == 2) {

    if (mask == true) {
      // coverity[secure_coding]
      if (sscanf(rangeTok[1], "%d", &mask_bits) != 1) {
        return "bad mask specification";
      }

      if (!(mask_bits >= 0 && mask_bits <= 32)) {
        return "invalid mask specification";
      }

      if (mask_bits == 32) {
        mask_val = 0;
      } else {
        mask_val = 0xffffffff >> mask_bits;
      }

      addr2_local = addr1_local | mask_val;
      addr1_local = addr1_local & (mask_val ^ 0xffffffff);

    } else {
      addr2_local = htonl(inet_addr(rangeTok[1]));
      if (addr2_local == (ip_addr_t) - 1 && strcmp(rangeTok[1], "255.255.255.255") != 0) {
        return "malformed ip address at range end";
      }
    }

    if (addr1_local > addr2_local) {
      return "range start greater than range end";
    }
  } else {
    addr2_local = addr1_local;
  }

  *addr1 = addr1_local;
  *addr2 = addr2_local;
  return NULL;
}

// char* tokLine(char* buf, char** last)
//
//  Similar to strtok_r but only tokenizes on '\n'
//   and will return tokens that are empty strings
//
char *
tokLine(char *buf, char **last)
{
  char *start;
  char *cur;

  if (buf != NULL) {
    start = cur = buf;
    *last = buf;
  } else {
    start = cur = (*last) + 1;
  }

  while (*cur != '\0') {
    if (*cur == '\n') {
      *cur = '\0';
      *last = cur;
      return start;
    }
    cur++;
  }

  // Return the last line even if it does 
  //  not end in a newline
  if (cur > (*last + 1)) {
    *last = cur - 1;
    return start;
  }

  return NULL;
}

char *matcher_type_str[] = {
  "invalid",
  "host",
  "domain",
  "ip",
  "url_regex",
  "host_regex"
};

// char* processDurationString(char* str, int* seconds)
//
//   Take a duration sting which is composed of
//      digits followed by a unit specifier
//         w - week
//         d - day
//         h - hour
//         m - min
//         s - sec
//
//   Trailing digits without a specifier are
//    assumed to be seconds
//
//   Returns NULL on success and a static
//    error string on failure
//
char *
processDurationString(char *str, int *seconds)
{
  char *s = str;
  char *current = str;
  char unit;
  int tmp;
  int multiplier;
  int result = 0;
  int len;

  if (str == NULL) {
    return "Missing time";
  }

  len = strlen(str);
  for (int i = 0; i < len; i++) {
    if (!ParseRules::is_digit(*current)) {

      // Make sure there is a time to proces
      if (current == s) {
        return "Malformed time";
      }

      unit = *current;

      switch (unit) {
      case 'w':
        multiplier = 7 * 24 * 60 * 60;
        break;
      case 'd':
        multiplier = 24 * 60 * 60;
        break;
      case 'h':
        multiplier = 60 * 60;
        break;
      case 'm':
        multiplier = 60;
        break;
      case 's':
        multiplier = 1;
        break;
      case '-':
        return "Negative time not permitted";
      default:
        return "Invalid time unit specified";
      }

      *current = '\0';

      // coverity[secure_coding]
      if (sscanf(s, "%d", &tmp) != 1) {
        // Really should not happen since everything
        //   in the string is digit
        ink_assert(0);
        return "Malformed time";
      }

      result += (multiplier * tmp);
      s = current + 1;

    }
    current++;
  }

  // Read any trailing seconds
  if (current != s) {
    // coverity[secure_coding]
    if (sscanf(s, "%d", &tmp) != 1) {
      // Really should not happen since everything
      //   in the string is digit
      ink_assert(0);
      return "Malformed time";
    } else {
      result += tmp;
    }
  }
  // We rolled over the int
  if (result < 0) {
    return "Time too big";
  }

  *seconds = result;
  return NULL;
}

const matcher_tags http_dest_tags = {
  "dest_host", "dest_domain", "dest_ip", "url_regex", "host_regex", true
};

const matcher_tags ip_allow_tags = {
  NULL, NULL, "src_ip", NULL, NULL, false
};

const matcher_tags socks_server_tags = {
  NULL, NULL, "dest_ip", NULL, NULL, false
};

// char* parseConfigLine(char* line, matcher_line* p_line, 
//                       const matcher_tags* tags)
//
//   Parse out a config file line suitable for passing to
//    a ControlMatcher object
//
//   If successful, NULL is returned.  If unsuccessful,
//     a static error string is returned
//
char *
parseConfigLine(char *line, matcher_line * p_line, const matcher_tags * tags)
{

  enum pState
  { FIND_LABEL, PARSE_LABEL,
    PARSE_VAL, START_PARSE_VAL, CONSUME
  };

  pState state = FIND_LABEL;
  bool inQuote = false;
  char *copyForward = NULL;
  char *copyFrom = NULL;
  char *s = line;
  char *label = NULL;
  char *val = NULL;
  int num_el = 0;
  matcher_type type = MATCH_NONE;

  // Zero out the parsed line structure
  memset(p_line, 0, sizeof(matcher_line));

  if (*s == '\0') {
    return NULL;
  }

  do {

    switch (state) {
    case FIND_LABEL:
      if (!isspace(*s)) {
        state = PARSE_LABEL;
        label = s;
      }
      s++;
      break;
    case PARSE_LABEL:
      if (*s == '=') {
        *s = '\0';
        state = START_PARSE_VAL;
      }
      s++;
      break;
    case START_PARSE_VAL:
      // Init state needed for parsing values
      copyForward = NULL;
      copyFrom = NULL;

      if (*s == '"') {
        inQuote = true;
        val = s + 1;
      } else if (*s == '\\') {
        inQuote = false;
        val = s + 1;
      } else {
        inQuote = false;
        val = s;

      }

      if (inQuote == false && (isspace(*s) || *(s + 1) == '\0')) {
        state = CONSUME;
      } else {
        state = PARSE_VAL;
      }

      s++;
      break;
    case PARSE_VAL:
      if (inQuote == true) {
        if (*s == '\\') {
          // The next character is esacped
          //
          // To remove the escaped character
          // we need to copy
          //  the rest of the entry over it
          //  but since we do not know where the
          //  end is right now, defer the work
          //  into the future

          if (copyForward != NULL) {
            // Perform the prior copy forward
            int bytesCopy = s - copyFrom;
            memcpy(copyForward, copyFrom, s - copyFrom);
            ink_assert(bytesCopy > 0);

            copyForward += bytesCopy;
            copyFrom = s + 1;
          } else {
            copyForward = s;
            copyFrom = s + 1;
          }

          // Scroll past the escape character
          s++;

          // Handle the case that places us
          //  at the end of the file
          if (*s == '\0') {
            break;
          }
        } else if (*s == '"') {
          state = CONSUME;
          *s = '\0';
        }
      } else if ((*s == '\\' && ParseRules::is_digit(*(s + 1)))
                 || !ParseRules::is_char(*s)) {
        // INKqa10511
        // traffic server need to handle unicode characters
        // right now ignore the entry
        return "Unrecognized encoding scheme";
      } else if (isspace(*s)) {
        state = CONSUME;
        *s = '\0';
      }

      s++;

      // If we are now at the end of the line,
      //   we need to consume final data
      if (*s == '\0') {
        state = CONSUME;
      }
      break;
    case CONSUME:
      break;
    }

    if (state == CONSUME) {

      // See if there are any quote copy overs
      //   we've pushed into the future
      if (copyForward != NULL) {
        int toCopy = (s - 1) - copyFrom;
        memcpy(copyForward, copyFrom, toCopy);
        *(copyForward + toCopy) = '\0';
      }

      p_line->line[0][num_el] = label;
      p_line->line[1][num_el] = val;
      type = MATCH_NONE;

      // Check to see if this the primary specifier we are looking for
      if (tags->match_ip && strcasecmp(tags->match_ip, label) == 0) {
        type = MATCH_IP;
      } else if (tags->match_host && strcasecmp(tags->match_host, label) == 0) {
        type = MATCH_HOST;
      } else if (tags->match_domain && strcasecmp(tags->match_domain, label) == 0) {
        type = MATCH_DOMAIN;
      } else if (tags->match_regex && strcasecmp(tags->match_regex, label) == 0) {
        type = MATCH_REGEX;
      } else if (tags->match_host_regex && strcasecmp(tags->match_host_regex, label) == 0) {
        type = MATCH_HOST_REGEX;
      }
      // If this a destination tag, use it
      if (type != MATCH_NONE) {
        // Check to see if this second destination specifier
        if (p_line->type != MATCH_NONE) {
          if (tags->dest_error_msg == false) {
            return "Muliple Sources Specified";
          } else {
            return "Muliple Destinations Specified";
          }
        } else {
          p_line->dest_entry = num_el;
          p_line->type = type;
        }
      }
      num_el++;

      if (num_el > MATCHER_MAX_TOKENS) {
        return "Malformed line: Too many tokens";
      }

      state = FIND_LABEL;
    }
  } while (*s != '\0');

  p_line->num_el = num_el;

  if (state != CONSUME && state != FIND_LABEL) {
    return "Malformed entry";
  }

  if (p_line->type == MATCH_NONE) {
    if (tags->dest_error_msg == false) {
      return "No source specifier";
    } else {
      return "No destination specifier";
    }
  }

  return NULL;
}
