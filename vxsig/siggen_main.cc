// Copyright 2011-2019 Google LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A program that implements AV signature generation from sets of binaries.
// Siggen operates on similar binaries that have been bindiffed pairwise.

#include <gflags/gflags.h>

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

#include "absl/base/internal/raw_logging.h"
#include "absl/memory/memory.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "vxsig/siggen.h"
#include "vxsig/signature_formatter.h"
#include "vxsig/types.h"
#include "vxsig/vxsig.pb.h"

DEFINE_string(detection_name, "VxSig_Signature",
              "Detection name of the signature");
DEFINE_int32(trim_length, kint32max,
             "Maximum length of the signature, subject to truncation of due to "
             "limitations of the target format");
DEFINE_string(trim_algorithm, "TRIM_RANDOM",
              "Signature trimming algorithm to use");
DEFINE_bool(disable_nibble_masking, false,
            "Whether or not to disable masking of instruction immediate bytes");
DEFINE_string(function_whitelist, "",
              "List of (hex) addresses of functions in the first binary to "
              "consider for the signature. Mutually exclusive with "
              "function_blacklist.");
DEFINE_string(function_blacklist, "", "Inverse of function_whitelist");

namespace security {
namespace vxsig {
namespace {

void SiggenMain(int argc, char* argv[]) {
  ABSL_RAW_CHECK(argc >= 2, "Need at least one .BinDiff file");

  SignatureDefinition::SignatureTrimAlgorithm trim_algorithm;
  if (!SignatureDefinition::SignatureTrimAlgorithm_Parse(FLAGS_trim_algorithm,
                                                         &trim_algorithm)) {
    ABSL_RAW_LOG(FATAL, "Invalid signature trimming algorithm: %s",
                 FLAGS_trim_algorithm.c_str());
  }

  ABSL_RAW_CHECK(
      FLAGS_function_whitelist.empty() || FLAGS_function_blacklist.empty(),
      "function_whitelist and function_blacklist are mutually exclusive");

  Signature signature;
  auto& signature_definition = *signature.mutable_definition();
  signature_definition.set_detection_name(FLAGS_detection_name);
  signature_definition.set_trim_length(FLAGS_trim_length);
  signature_definition.set_trim_algorithm(trim_algorithm);
  signature_definition.set_disable_nibble_masking(FLAGS_disable_nibble_masking);
  signature_definition.set_function_filter(SignatureDefinition::FILTER_NONE);

  string filter_list(FLAGS_function_whitelist);
  if (!filter_list.empty()) {
    signature_definition.set_function_filter(
        SignatureDefinition::FILTER_WHITELIST);
  } else {
    signature_definition.set_function_filter(
        SignatureDefinition::FILTER_BLACKLIST);
    filter_list = FLAGS_function_blacklist;
  }
  if (!filter_list.empty()) {
    uint64_t address = 0;
    for (const auto& function :
         absl::StrSplit(filter_list, ',', absl::SkipWhitespace())) {
      QCHECK(
          absl::numbers_internal::safe_strtou64_base(function, &address, 16));
      signature_definition.add_filtered_function_address(address);
    }
  }

  AvSignatureGenerator siggen;
  siggen.AddDiffResultsFromCommandLineArguments(--argc, ++argv);
  QCHECK_OK(siggen.Generate(&signature));

  // Output the signature itself to stdout, so we can use redirected output
  // from this tool in scripts.
  std::cout << "----8<--------8<---- Signature ----8<--------8<----\n";
  QCHECK_OK(SignatureFormatter::Create(YARA)->Format(&signature));
  printf("%s\n", signature.yara_signature().data().c_str());
  std::cout << "---->8-------->8---- Signature ---->8-------->8----\n";
}

}  // namespace
}  // namespace vxsig
}  // namespace security

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);
  security::vxsig::SiggenMain(argc, argv);
  return EXIT_SUCCESS;
}
