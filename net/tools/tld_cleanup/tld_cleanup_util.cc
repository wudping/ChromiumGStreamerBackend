// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/tld_cleanup/tld_cleanup_util.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace {

const char kBeginPrivateDomainsComment[] = "// ===BEGIN PRIVATE DOMAINS===";
const char kEndPrivateDomainsComment[] = "// ===END PRIVATE DOMAINS===";

const int kExceptionRule = 1;
const int kWildcardRule = 2;
const int kPrivateRule = 4;
}

namespace net {
namespace tld_cleanup {

// Writes the list of domain rules contained in the 'rules' set to the
// 'outfile', with each rule terminated by a LF.  The file must already have
// been created with write access.
bool WriteRules(const RuleMap& rules, const base::FilePath& outfile) {
  std::string data;
  data.append("%{\n"
              "// Copyright 2012 The Chromium Authors. All rights reserved.\n"
              "// Use of this source code is governed by a BSD-style license "
              "that can be\n"
              "// found in the LICENSE file.\n\n"
              "// This file is generated by net/tools/tld_cleanup/.\n"
              "// DO NOT MANUALLY EDIT!\n"
              "%}\n"
              "struct DomainRule {\n"
              "  int name_offset;\n"
              "  int type;  // flags: 1: exception, 2: wildcard, 4: private\n"
              "};\n"
              "%%\n");

  for (RuleMap::const_iterator i = rules.begin(); i != rules.end(); ++i) {
    data.append(i->first);
    data.append(", ");
    int type = 0;
    if (i->second.exception) {
      type = kExceptionRule;
    } else if (i->second.wildcard) {
      type = kWildcardRule;
    }
    if (i->second.is_private) {
      type += kPrivateRule;
    }
    data.append(base::IntToString(type));
    data.append("\n");
  }

  data.append("%%\n");

  int written = base::WriteFile(outfile,
                                     data.data(),
                                     static_cast<int>(data.size()));

  return written == static_cast<int>(data.size());
}

// Adjusts the rule to a standard form: removes single extraneous dots and
// canonicalizes it using GURL. Returns kSuccess if the rule is interpreted as
// valid; logs a warning and returns kWarning if it is probably invalid; and
// logs an error and returns kError if the rule is (almost) certainly invalid.
NormalizeResult NormalizeRule(std::string* domain, Rule* rule) {
  NormalizeResult result = kSuccess;

  // Strip single leading and trailing dots.
  if (domain->at(0) == '.')
    domain->erase(0, 1);
  if (domain->empty()) {
    LOG(WARNING) << "Ignoring empty rule";
    return kWarning;
  }
  if (domain->at(domain->size() - 1) == '.')
    domain->erase(domain->size() - 1, 1);
  if (domain->empty()) {
    LOG(WARNING) << "Ignoring empty rule";
    return kWarning;
  }

  // Allow single leading '*.' or '!', saved here so it's not canonicalized.
  size_t start_offset = 0;
  if (domain->at(0) == '!') {
    domain->erase(0, 1);
    rule->exception = true;
  } else if (domain->find("*.") == 0) {
    domain->erase(0, 2);
    rule->wildcard = true;
  }
  if (domain->empty()) {
    LOG(WARNING) << "Ignoring empty rule";
    return kWarning;
  }

  // Warn about additional '*.' or '!'.
  if (domain->find("*.", start_offset) != std::string::npos ||
      domain->find('!', start_offset) != std::string::npos) {
    LOG(WARNING) << "Keeping probably invalid rule: " << *domain;
    result = kWarning;
  }

  // Make a GURL and normalize it, then get the host back out.
  std::string url = "http://";
  url.append(*domain);
  GURL gurl(url);
  const std::string& spec = gurl.possibly_invalid_spec();
  url::Component host = gurl.parsed_for_possibly_invalid_spec().host;
  if (host.len < 0) {
    LOG(ERROR) << "Ignoring rule that couldn't be normalized: " << *domain;
    return kError;
  }
  if (!gurl.is_valid()) {
    LOG(WARNING) << "Keeping rule that GURL says is invalid: " << *domain;
    result = kWarning;
  }
  domain->assign(spec.substr(host.begin, host.len));

  return result;
}

NormalizeResult NormalizeDataToRuleMap(const std::string data,
                                       RuleMap* rules) {
  CHECK(rules);
  // We do a lot of string assignment during parsing, but simplicity is more
  // important than performance here.
  std::string domain;
  NormalizeResult result = kSuccess;
  size_t line_start = 0;
  size_t line_end = 0;
  bool is_private = false;
  RuleMap extra_rules;
  int begin_private_length = arraysize(kBeginPrivateDomainsComment) - 1;
  int end_private_length = arraysize(kEndPrivateDomainsComment) - 1;
  while (line_start < data.size()) {
    if (line_start + begin_private_length < data.size() &&
        !data.compare(line_start, begin_private_length,
                      kBeginPrivateDomainsComment)) {
      is_private = true;
      line_end = line_start + begin_private_length;
    } else if (line_start + end_private_length < data.size() &&
        !data.compare(line_start, end_private_length,
                      kEndPrivateDomainsComment)) {
      is_private = false;
      line_end = line_start + end_private_length;
    } else if (line_start + 1 < data.size() &&
        data[line_start] == '/' &&
        data[line_start + 1] == '/') {
      // Skip comments.
      line_end = data.find_first_of("\r\n", line_start);
      if (line_end == std::string::npos)
        line_end = data.size();
    } else {
      // Truncate at first whitespace.
      line_end = data.find_first_of("\r\n \t", line_start);
      if (line_end == std::string::npos)
        line_end = data.size();
      domain.assign(data.data(), line_start, line_end - line_start);

      Rule rule;
      rule.wildcard = false;
      rule.exception = false;
      rule.is_private = is_private;
      NormalizeResult new_result = NormalizeRule(&domain, &rule);
      if (new_result != kError) {
        // Check the existing rules to make sure we don't have an exception and
        // wildcard for the same rule, or that the same domain is listed as both
        // private and not private. If we did, we'd have to update our
        // parsing code to handle this case.
        CHECK(rules->find(domain) == rules->end())
            << "Duplicate rule found for " << domain;

        (*rules)[domain] = rule;
        // Add true TLD for multi-level rules.  We don't add them right now, in
        // case there's an exception or wild card that either exists or might be
        // added in a later iteration.  In those cases, there's no need to add
        // it and it would just slow down parsing the data.
        size_t tld_start = domain.find_last_of('.');
        if (tld_start != std::string::npos && tld_start + 1 < domain.size()) {
          std::string extra_rule_domain = domain.substr(tld_start + 1);
          RuleMap::const_iterator iter = extra_rules.find(extra_rule_domain);
          Rule extra_rule;
          extra_rule.exception = false;
          extra_rule.wildcard = false;
          if (iter == extra_rules.end()) {
            extra_rule.is_private = is_private;
          } else {
            // A rule already exists, so we ensure that if any of the entries is
            // not private the result should be that the entry is not private.
            // An example is .au which is not listed as a real TLD, but only
            // lists second-level domains such as com.au. Subdomains of .au
            // (eg. blogspot.com.au) are also listed in the private section,
            // which is processed later, so this ensures that the real TLD
            // (eg. .au) is listed as public.
            extra_rule.is_private = is_private && iter->second.is_private;
          }
          extra_rules[extra_rule_domain] = extra_rule;
        }
      }
      result = std::max(result, new_result);
    }

    // Find beginning of next non-empty line.
    line_start = data.find_first_of("\r\n", line_end);
    if (line_start == std::string::npos)
      line_start = data.size();
    line_start = data.find_first_not_of("\r\n", line_start);
    if (line_start == std::string::npos)
      line_start = data.size();
  }

  for (RuleMap::const_iterator iter = extra_rules.begin();
       iter != extra_rules.end();
       ++iter) {
    if (rules->find(iter->first) == rules->end()) {
      (*rules)[iter->first] = iter->second;
    }
  }

  return result;
}

NormalizeResult NormalizeFile(const base::FilePath& in_filename,
                              const base::FilePath& out_filename) {
  RuleMap rules;
  std::string data;
  if (!base::ReadFileToString(in_filename, &data)) {
    LOG(ERROR) << "Unable to read file";
    // We return success since we've already reported the error.
    return kSuccess;
  }

  NormalizeResult result = NormalizeDataToRuleMap(data, &rules);

  if (!WriteRules(rules, out_filename)) {
    LOG(ERROR) << "Error(s) writing output file";
    result = kError;
  }

  return result;
}


}  // namespace tld_cleanup
}  // namespace net
